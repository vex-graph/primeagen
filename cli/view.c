#define _XOPEN_SOURCE_EXTENDED 1
#include "cli/view.h"
#include "tui/utf8.h"
#include "hot/hot.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <curses.h>
#include <string.h>
#include <wchar.h>

;;DEFINITION
/* The reloadable terminal view paints resident Tui state. It owns neither the
 * screen nor conversation, session pipes or permissions. A cold host-excluded
 * swap changes this code while the same ncurses screen/input remain alive. */
;;OVERVIEW
/* ============================================================================
 * MODULE: Primeagen view (cli/view.c)
 * OWNED STATE: none; host owns every resource and palette.
 * FUNCTION REGISTRY:
 *   bool Primeagen_draw(Tui *, bool owo, bool focused, bool sessionMode, bool busy,
 *                       bool busy, char *row, uint8_t *styles, size_t cap)
 *       Paint resident model into caller's live ncurses screen.
 *   const void *VkModuleGetTrampolines(uint32_t *count) : trusted loader exports.
 *   int Primeagen_command(Tui *, bool sessionMode) : local command admission;
 *       emits display notices/actions, never executes model/provider/tools.
 * PRIVATE: attributes(style) : host-defined visible style to ncurses attributes.
 *   drawRegion(...) : legacy/full-width or right-pane paint with fixed input.
 *     Whole UTF-8 scalars use wide ncurses cells and wcwidth; combining marks
 *     group up to CCHARW_MAX. Unsupported locale glyphs use '?'. Not full Unicode
 *     grapheme/emoji-ZWJ shaping; terminal font/width agreement remains external.
 *   symbol(y, glyph, fallback, selected) : one-cell monochrome rail glyph.
 * PUBLIC PANEL SURFACE:
 *   int Primeagen_chatLeft(int columns, int listWidth) : adaptive chat origin.
 *   bool Primeagen_panelsClick(x,y,rows,columns,listWidth,panel,selection,
 *                             int *outPanel,int *outSelection)
 *       Stateless hit admission; preserves outputs on invalid/outside input.
 *   bool Primeagen_drawPanels(Tui *,bool,bool,bool,bool,bool,int panel,int selection,
 *                             int listWidth,char *row,uint8_t *styles,size_t cap)
 *       Connected rail + people/server/channel preview + independent chat region.
 * PANEL CONTRACT: host owns mode/selection, not a provider registry. Selection 0
 * is the existing OpenCode chat; 1..5 are explicitly offline preview entries.
 * Preview selection never displays or sends another conversation's draft/history.
 * Switching returns to the same resident live chat. Old draw ABI is retained;
 * the new export receives navigation by value and changes no Tui layout.
 * EXPORTED DATA: const uint64_t Hot_cold_abi : compatible cold module ABI.
 * SLOT RECORD: behaviorless export rows {const char *name; void *fn}.
 * CONTRACT: owner-thread, live screen/model/buffers; nullptr entry rejects;
 * no retained pointers, callbacks, module state or model/permission operations.
 * Layout changes require a cold ABI bump and resident host rebuild. Module code
 * is trusted; checksum admission is not native-code sandboxing.
 * ============================================================================ */
const uint64_t Hot_cold_abi = HOT_COLD_ABI_VERSION;
enum { VIEW_HEADER = 1, VIEW_ACCENT = 2, VIEW_STATUS = 3, VIEW_INPUT = 4, VIEW_CODE = 5, VIEW_ERROR = 6 };
#ifndef PRIMEAGEN_VIEW_TITLE
#define PRIMEAGEN_VIEW_TITLE "Primeagen"
#endif

int Primeagen_command(Tui *state, bool sessionMode) {
    if (state == nullptr)
        return -1;
    const char *input = Tui_getInput(state);
    if (*input != '/')
        return 0;
    if (strcmp(input, "/quit") == 0)
        return PRIMEAGEN_COMMAND_QUIT;
    if (strcmp(input, "/update") == 0)
        return PRIMEAGEN_COMMAND_UPDATE;
    if (strcmp(input, "/connect") == 0 && !sessionMode)
        return PRIMEAGEN_COMMAND_CONNECT;
    if (strcmp(input, "/clear") == 0) {
        Tui_clear(state);
        Tui_inputClear(state);
        return PRIMEAGEN_COMMAND_MESSAGE;
    }
    const char *notice;
    if (strcmp(input, "/help") == 0)
        notice = "/connect: existing OpenCode Go login | /update: local built CLI, pause/resolve/continue | /clear: display only | /quit: exit. Ctrl-T: terminal text selection; Ctrl-P People, Ctrl-S Servers, Ctrl-O OpenCode. Example entries are previews; settings/roles are unfinished.";
    else if (strcmp(input, "/connect") == 0)
        notice = "Already connected to OpenCode. Enter sends; remote history is retained. Tools remain denied.";
    else
        notice = "Unknown or unfinished command. /help lists working commands. This command was not sent to OpenCode.";
    if (!Tui_add(state, notice, strlen(notice)))
        return -1;
    Tui_inputClear(state);
    return PRIMEAGEN_COMMAND_MESSAGE;
}

static chtype attributes(uint8_t style) {
    chtype value = A_NORMAL;
    if (style & TEXT_STYLE_BOLD)
        value |= A_BOLD;
    if (style & TEXT_STYLE_ITALIC)
        value |= A_UNDERLINE;
    if (style & TEXT_STYLE_CODE)
        value |= COLOR_PAIR(VIEW_CODE);
    else if (style & TEXT_STYLE_ERROR)
        value |= COLOR_PAIR(VIEW_ERROR);
    else if (style & TEXT_STYLE_USER)
        value |= COLOR_PAIR(VIEW_INPUT);
    else if (style & (TEXT_STYLE_ASSISTANT | TEXT_STYLE_HEADING))
        value |= COLOR_PAIR(VIEW_HEADER);
    return value;
}

static bool drawRegion(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                       int left, int selection, char *row, uint8_t *styles, size_t cap) {
    if (state == nullptr)
        return false;
    int rows, columns;
    Tui_getSize(state, &rows, &columns);
    if (rows < TUI_MIN_ROWS || columns < TUI_MIN_COLUMNS) {
        mvaddnstr(0, 0, "Resize terminal; Ctrl-Q exits", columns > 1 ? columns - 1 : 0);
        curs_set(0);
        return refresh() != ERR;
    }
    attron(COLOR_PAIR(VIEW_HEADER) | A_BOLD);
    mvaddnstr(0, left + 1, PRIMEAGEN_VIEW_TITLE, columns - 16);
    attroff(COLOR_PAIR(VIEW_HEADER) | A_BOLD);
    attron(COLOR_PAIR(VIEW_ACCENT));
    mvaddstr(0, left + columns - 15, "[Clear]");
    mvaddstr(0, left + columns - 7, "[Quit]");
    attroff(COLOR_PAIR(VIEW_ACCENT));
    mvhline(1, left, ACS_HLINE, columns);
    size_t first = Tui_getFirstVisibleRow(state);
    size_t total = Tui_getVisualRowCount(state);
    size_t height = Tui_getViewportHeight(state);
    for (size_t i = 0; selection == 0 && i < height && first + i < total; ++i) {
        if (!Tui_copyStyledRow(state, first + i, row, styles, cap, nullptr))
            return false;
        size_t length = strlen(row), cells = 0;
        for (size_t at = 0; at < length;) {
            uint32_t scalar;
            size_t bytes = TuiUtf8_decode(row + at, length - at, &scalar);
            if (bytes == 0)
                return false;
            wchar_t text[CCHARW_MAX] = {0};
            text[0] = wcwidth((wchar_t) scalar) < 0 ? L'?' : (wchar_t) scalar;
            size_t next = at + bytes, count = 1;
            while (next < length && count < CCHARW_MAX - 1) {
                uint32_t mark;
                size_t markBytes = TuiUtf8_decode(row + next, length - next, &mark);
                if (markBytes == 0 || wcwidth((wchar_t) mark) != 0)
                    break;
                text[count++] = (wchar_t) mark;
                next += markBytes;
            }
            cchar_t glyph;
            chtype style = attributes(styles[at]);
            if (setcchar(&glyph, text, (attr_t) (style & ~A_COLOR), (short) PAIR_NUMBER(style), nullptr) != OK)
                return false;
            mvadd_wch((int) i + 2, left + (int) cells + 1, &glyph);
            cells += TuiUtf8_width(scalar);
            at = next;
        }
    }
    if (selection != 0) {
        const char *names[] = {"OpenCode", "Sola (@sola6sol)", "WhateverName (@whatever)", "Ecosystem / #channel1", "Ecosystem / #channel2", "Ecosystem / #channel3"};
        mvaddnstr(2, left + 1, names[selection], columns - 2);
        mvaddnstr(4, left + 1, "Preview only: no model/session attached.", columns - 2);
        mvaddnstr(5, left + 1, "Select OpenCode to return to your chat.", columns - 2);
    }
    mvhline(rows - 4, left, ACS_HLINE, columns);
    attron(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    mvaddnstr(rows - 3, left + 1, sessionMode ? "Ctrl-T select text | scroll: wheel/Up-Down | /help | /update | Ctrl-Q quit" : "/connect (click here) | Ctrl-T select | /help | Ctrl-Q quit", columns - 2);
    attroff(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    attron(COLOR_PAIR(VIEW_INPUT) | A_BOLD);
    mvaddstr(rows - 2, left, "> ");
    attroff(COLOR_PAIR(VIEW_INPUT) | A_BOLD);
    size_t length = Tui_getInputLength(state);
    size_t width = (size_t) columns - 3;
    size_t offset = length > width ? length - width : 0;
    if (selection == 0)
        mvaddnstr(rows - 2, left + 2, Tui_getInput(state) + offset, (int) width);
    const char *status = sessionMode ? (failed ? "OpenCode session closed after a failure | Ctrl-Q and relaunch" : busy ? "OpenCode working | Ctrl-Q cancels locally" : "OpenCode chat | inspect auto | edit/shell ask | Enter sends") : (owo ? "OwO | disconnected | /connect uses Go login" : "Disconnected | /connect uses OpenCode Go login");
    attron(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    mvaddnstr(rows - 1, left, selection == 0 ? status : "Offline preview | Ctrl-P People | Ctrl-S Servers", columns - 1);
    attroff(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    curs_set(focused && selection == 0 ? 1 : 0);
    move(rows - 2, left + 2 + (int) (selection == 0 ? length - offset : 0));
    return true;
}

bool Primeagen_draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                    char *row, uint8_t *styles, size_t cap) {
    erase();
    if (!drawRegion(state, owo, focused, sessionMode, busy, failed, 0, 0, row, styles, cap))
        return false;
    return refresh() != ERR;
}

int Primeagen_chatLeft(int columns, int listWidth) {
    if (columns < PRIMEAGEN_PANELS_MIN_COLUMNS || listWidth < 12)
        return 0;
    int width = listWidth < columns / 2 ? listWidth : columns / 2;
    return PRIMEAGEN_RAIL_WIDTH + width;
}

bool Primeagen_panelsClick(int x, int y, int rows, int columns, int listWidth,
                           int panel, int selection, int *outPanel, int *outSelection) {
    int left = Primeagen_chatLeft(columns, listWidth);
    if (outPanel == nullptr || outSelection == nullptr || panel < 0 || panel > 2 ||
        selection < 0 || selection > 5 || rows < TUI_MIN_ROWS || left == 0 ||
        x <= 0 || x == PRIMEAGEN_RAIL_WIDTH - 1 || x >= left - 1 || y < 0 || y >= rows)
        return false;
    if (x < PRIMEAGEN_RAIL_WIDTH - 1) {
        if (y != 1 && y != 3)
            return false;
        panel = y == 1 ? PRIMEAGEN_PANEL_PEOPLE : PRIMEAGEN_PANEL_SERVERS;
    } else if (panel == PRIMEAGEN_PANEL_PEOPLE) {
        if (y >= 2 && y <= 3)
            selection = 0;
        else if (y >= 5 && y <= 6)
            selection = 1;
        else if (y >= 8 && y <= 9)
            selection = 2;
        else
            return false;
    } else if (panel == PRIMEAGEN_PANEL_SERVERS && y == 2) {
        panel = PRIMEAGEN_PANEL_CHANNELS;
        selection = 3;
    } else if (panel == PRIMEAGEN_PANEL_CHANNELS && y == 2)
        panel = PRIMEAGEN_PANEL_SERVERS;
    else if (panel == PRIMEAGEN_PANEL_CHANNELS && y >= 4 && y <= 6)
        selection = y - 1;
    else
        return false;
    *outPanel = panel;
    *outSelection = selection;
    return true;
}

static void symbol(int y, const wchar_t *text, char fallback, bool selected) {
    cchar_t glyph;
    attr_t attr = selected ? A_REVERSE | A_BOLD : A_NORMAL;
    if (wcwidth(*text) == 1 && setcchar(&glyph, text, attr, 0, nullptr) == OK)
        mvadd_wch(y, 2, &glyph);
    else
        mvaddch(y, 2, (chtype) fallback | attr);
}

bool Primeagen_drawPanels(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                          int panel, int selection, int listWidth,
                          char *row, uint8_t *styles, size_t cap) {
    if (state == nullptr || panel < 0 || panel > 2 || selection < 0 || selection > 5)
        return false;
    int rows, columns;
    getmaxyx(stdscr, rows, columns);
    int left = Primeagen_chatLeft(columns, listWidth);
    erase();
    if (!drawRegion(state, owo, focused, sessionMode, busy, failed, left, selection, row, styles, cap))
        return false;
    int caretY, caretX;
    getyx(stdscr, caretY, caretX);
    if (left > 0 && rows >= TUI_MIN_ROWS) {
        mvvline(0, 4, ACS_VLINE, rows);
        mvvline(0, left - 1, ACS_VLINE, rows);
        for (int y = 0; y <= 4; y += 2) {
            mvhline(y, 0, ACS_HLINE, 4);
            mvaddch(y, 0, y == 0 ? ACS_ULCORNER : y == 4 ? ACS_LLCORNER : ACS_LTEE);
            mvaddch(y, 4, y == 0 ? ACS_TTEE : y == 4 ? ACS_RTEE : ACS_PLUS);
        }
        mvaddch(1, 0, ACS_VLINE);
        mvaddch(3, 0, ACS_VLINE);
        symbol(1, L"⌘", '@', panel == PRIMEAGEN_PANEL_PEOPLE);
        symbol(3, L"▦", '#', panel != PRIMEAGEN_PANEL_PEOPLE);
        mvaddnstr(0, 6, panel == PRIMEAGEN_PANEL_PEOPLE ? "People" : panel == PRIMEAGEN_PANEL_SERVERS ? "Servers" : "Ecosystem", left - 8);
        if (panel == PRIMEAGEN_PANEL_PEOPLE) {
            const char *lines[] = {"OpenCode", "[configured connection]", "", "Sola (@sola6sol)", "[GPT-6.1 sol default]", "", "WhateverName (@whatever)", "[Claude Sonnet 5 Low]", "", "Examples: preview only"};
            for (int i = 0; i < 10 && i + 2 < rows; ++i) {
                if ((selection == 0 && i < 2) || (selection == 1 && i >= 3 && i < 5) || (selection == 2 && i >= 6 && i < 8))
                    attron(A_REVERSE);
                mvaddnstr(i + 2, 6, lines[i], left - 8);
                attroff(A_REVERSE);
            }
        } else if (panel == PRIMEAGEN_PANEL_SERVERS)
            mvaddnstr(2, 6, "Ecosystem [preview]", left - 8);
        else {
            mvaddnstr(2, 6, "< Servers (Ctrl-S)", left - 8);
            for (int i = 0; i < 3 && i + 4 < rows; ++i) {
                if (selection == i + 3)
                    attron(A_REVERSE);
                mvprintw(i + 4, 6, "#channel%d", i + 1);
                attroff(A_REVERSE);
            }
            if (rows > 8)
                mvaddnstr(8, 6, "Channels: preview only", left - 8);
        }
    }
    move(caretY, caretX); // sidebar painting must not relocate the input caret
    return refresh() != ERR;
}

const void *VkModuleGetTrampolines(uint32_t *count) {
    static const struct { const char *name; void *fn; } exports[] = {
        {"Primeagen_draw", (void*) Primeagen_draw}, {"Primeagen_command", (void*) Primeagen_command},
        {"Primeagen_drawPanels", (void*) Primeagen_drawPanels}
    };
    if (count == nullptr)
        return nullptr;
    *count = sizeof(exports) / sizeof(exports[0]);
    return exports;
}
