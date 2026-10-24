#define _XOPEN_SOURCE_EXTENDED 1
#include "cli/view.h"
#include "cli/appearance.h"
#include "cli/screen.h"
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
 *                       bool failed, char *row, uint8_t *styles, size_t cap)
 *       Paint the sole live Users/OpenCode context to the right-side chat pane.
 *   const void *VkModuleGetTrampolines(uint32_t *count) : trusted loader exports.
 *   int Primeagen_command(Tui *, bool sessionMode) : local command admission;
 *       emits display notices/actions, never executes model/provider/tools.
 * PRIVATE: attributes(style) : host-defined visible style to ncurses attributes.
 *   drawRegion(...) : persistent chat pane for selected tree context; live Tui
 *     history/input appear only for Users/OpenCode, examples for other nodes.
 *     Whole UTF-8 scalars use wide ncurses cells and wcwidth; combining marks
 *     group up to CCHARW_MAX. Unsupported locale glyphs use '?'. Not full Unicode
 *     grapheme/emoji-ZWJ shaping; terminal font/width agreement remains external.
 * PUBLIC NAVIGATION SURFACE:
 *   int Primeagen_chatLeft(int columns, int listWidth) : tree width/chat origin.
 *   bool Primeagen_panelsClick(x,y,rows,columns,listWidth,panel,selection,
 *                             int *outPanel,int *outSelection)
 *       Stateless tree-row hit admission; preserves outputs on invalid/outside input.
 *   bool Primeagen_drawPanels(Tui *,bool,bool,bool,bool,bool,int panel,int selection,
 *                             int listWidth,char *row,uint8_t *styles,size_t cap)
 *       Legacy panel entry; retained for older resident hosts.
 *   bool Primeagen_drawTreePanels(Tui *,TerminalTreeView *,bool,bool,bool,bool,
 *                                 bool,int panel,int selection,int listWidth,...)
 *       Paint host-owned fold/scroll/selection state in a filled, borderless
 *       explorer panel with Semicolon-style two-cell depth steps, disclosure
 *       marks and semantic Nerd Font icons. Selection paints a full-width row
 *       rectangle. No divider is drawn; a breathing gap separates explorer from
 *       chat. Width zero hides the explorer without changing tree state.
 * NAVIGATION CONTRACT: chats, users, servers, private chat. The tree is a
 * collapsible IDE-style hierarchy, not persisted filesystem/database state. Only Users/OpenCode draws
 * or submits the resident session. Other nodes use mock chat/composer content and
 * never display, scroll, edit, or submit the live Tui draft/history. There is no
 * server/member/rules/model implementation or private-chat persistence.
 * EXPORTED DATA: const uint64_t Hot_cold_abi : compatible cold module ABI; the
 * tree renderer is an additive export, preserving the legacy panel signature.
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
        notice = "/connect: existing OpenCode Go login | /update: local built CLI, pause/resolve/continue | /clear: display only | /quit: exit. Ctrl-T: terminal selection; Ctrl-P Users, Ctrl-S Servers, Ctrl-G Chats, Ctrl-N Private, Ctrl-O OpenCode. Other tree entries are mock previews.";
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
                       int left, int panel, int selection, char *row, uint8_t *styles, size_t cap) {
    if (state == nullptr)
        return false;
    int rows, columns;
    Tui_getSize(state, &rows, &columns);
    if (rows < TUI_MIN_ROWS || columns < TUI_MIN_COLUMNS) {
        mvaddnstr(0, 0, "Resize terminal; Ctrl-Q exits", columns > 1 ? columns - 1 : 0);
        curs_set(0);
        return refresh() != ERR;
    }
    bool liveConversation = panel == PRIMEAGEN_PANEL_PEOPLE && selection == 0;
    const char *title = PRIMEAGEN_VIEW_TITLE;
    const char *mode = "offline example";
    if (panel == PRIMEAGEN_PANEL_PEOPLE) {
        static const char *const users[] = {"OpenCode", "solasol · glm-5.3-flash", "whatever · model preview"};
        title = users[selection < 3 ? selection : 0];
        mode = selection == 0 ? "OpenCode session" : "user chat · model preview";
    } else if (panel == PRIMEAGEN_PANEL_SERVERS || panel == PRIMEAGEN_PANEL_CHANNELS) {
        title = panel == PRIMEAGEN_PANEL_CHANNELS ? "#general" : "serverNumber1";
        mode = "server channel · mention-driven · no model";
    } else if (panel == PRIMEAGEN_PANEL_CHATS) {
        static const char *const chats[] = {"chat #1 · Claude Sonnet 5.5", "chat #2 · saved", "chat #3 · saved"};
        title = chats[selection < 3 ? selection : 0];
        mode = "saved chat · model preview";
    } else {
        title = "# private-chat · unsaved";
        mode = "private chat · not saved";
    }
    attron(COLOR_PAIR(VIEW_HEADER) | A_BOLD);
    mvaddnstr(0, left + 1, title, columns - 16);
    attroff(COLOR_PAIR(VIEW_HEADER) | A_BOLD);
    attron(COLOR_PAIR(VIEW_ACCENT));
    mvaddstr(0, left + columns - 15, "[Clear]");
    mvaddstr(0, left + columns - 7, "[Quit]");
    attroff(COLOR_PAIR(VIEW_ACCENT));
    // The horizontal rule is a toolkit CONNECTOR run (junctions/striping), not a
    // bare ACS_HLINE; the chosen appearance decides the glyphs.
    PrimeagenAppearance_drawRule(stdscr, 1, left, columns);
    size_t first = Tui_getFirstVisibleRow(state);
    size_t total = Tui_getVisualRowCount(state);
    size_t height = Tui_getViewportHeight(state);
    for (size_t i = 0; liveConversation && i < height && first + i < total; ++i) {
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
    if (!liveConversation) {
        attron(COLOR_PAIR(VIEW_STATUS) | A_DIM);
        mvaddnstr(2, left + 1, mode, columns - 2);
        attroff(COLOR_PAIR(VIEW_STATUS) | A_DIM);
        if (panel == PRIMEAGEN_PANEL_SERVERS || panel == PRIMEAGEN_PANEL_CHANNELS) {
            mvaddnstr(4, left + 1, "@mention a member to start an agent turn.", columns - 2);
            mvaddnstr(6, left + 1, "Server membership and agent responses are not connected.", columns - 2);
        } else if (panel == PRIMEAGEN_PANEL_PRIVATE) {
            mvaddnstr(4, left + 1, "This conversation is temporary until saved.", columns - 2);
            mvaddnstr(6, left + 1, "[Claude Sonnet 5.5]  [Ultra]  [fast]  · preview", columns - 2);
        } else if (panel == PRIMEAGEN_PANEL_CHATS) {
            mvaddnstr(4, left + 1, "Saved conversation preview; history is not loaded.", columns - 2);
            mvaddnstr(6, left + 1, "[Ultra]  [fast]  · model controls are visual only", columns - 2);
        } else {
            mvaddnstr(4, left + 1, "User conversation preview; no model is connected.", columns - 2);
            mvaddnstr(6, left + 1, "[Ultra]  [fast]  · model controls are visual only", columns - 2);
        }
    }
    PrimeagenAppearance_drawRule(stdscr, rows - 4, left, columns);
    attron(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    mvaddnstr(rows - 3, left + 1, liveConversation ? (sessionMode ? "Ctrl-T select text | scroll: wheel/Up-Down | /help | /update | Ctrl-Q quit" : "/connect | Ctrl-T select | /help | Ctrl-Q quit") : "Offline preview · Ctrl-P Users · Ctrl-S Servers · Ctrl-G Chats · Ctrl-N Private", columns - 2);
    attroff(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    attron(COLOR_PAIR(VIEW_INPUT) | A_BOLD);
    mvaddstr(rows - 2, left, "> ");
    attroff(COLOR_PAIR(VIEW_INPUT) | A_BOLD);
    size_t length = Tui_getInputLength(state);
    size_t width = (size_t) columns - 3;
    size_t offset = length > width ? length - width : 0;
    if (liveConversation)
        mvaddnstr(rows - 2, left + 2, Tui_getInput(state) + offset, (int) width);
    else
        mvaddnstr(rows - 2, left + 2, panel == PRIMEAGEN_PANEL_SERVERS || panel == PRIMEAGEN_PANEL_CHANNELS ? "@member ..." : "message ...", columns - 15);
    const char *status = liveConversation ? (sessionMode ? (failed ? "OpenCode session closed after a failure | Ctrl-Q and relaunch" : busy ? "OpenCode working | Ctrl-Q cancels locally" : "OpenCode chat | inspect auto | edit/shell ask | Enter sends") : (owo ? "OwO | disconnected | /connect uses Go login" : "Disconnected | /connect uses Go login")) : "Local UI mock · no message sent, model called, or conversation persisted";
    attron(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    mvaddnstr(rows - 1, left, liveConversation ? status : "Local UI mock | Ctrl-P Users | Ctrl-S Servers | Ctrl-G Chats | Ctrl-N Private", columns - 1);
    attroff(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    curs_set(focused && liveConversation ? 1 : 0);
    move(rows - 2, left + 2 + (int) (liveConversation ? length - offset : 0));
    return true;
}

bool Primeagen_setAppearance(const char *presetName) {
    PrimeagenAppearance_use(presetName);
    return true;
}

bool Primeagen_setBanner(const uint8_t *rgba, uint32_t width, uint32_t height) {
    PrimeagenScreen_setBanner(rgba, width, height);
    return true;
}

bool Primeagen_draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                    char *row, uint8_t *styles, size_t cap) {
    erase();
    if (!drawRegion(state, owo, focused, sessionMode, busy, failed, 0, PRIMEAGEN_PANEL_PEOPLE, 0, row, styles, cap))
        return false;
    return refresh() != ERR;
}

int Primeagen_chatLeft(int columns, int listWidth) {
    if (columns < PRIMEAGEN_PANELS_MIN_COLUMNS || listWidth < 12)
        return 0;
    int width = listWidth < columns / 2 ? listWidth : columns / 2;
    return width;
}

static bool validNavigation(int panel, int selection) {
    if (selection < 0)
        return false;
    if (panel == PRIMEAGEN_PANEL_PEOPLE || panel == PRIMEAGEN_PANEL_CHATS)
        return selection <= 2;
    if (panel == PRIMEAGEN_PANEL_SERVERS || panel == PRIMEAGEN_PANEL_PRIVATE)
        return selection == 0;
    return panel == PRIMEAGEN_PANEL_CHANNELS && selection == 3;
}

bool Primeagen_panelsClick(int x, int y, int rows, int columns, int listWidth,
                           int panel, int selection, int *outPanel, int *outSelection) {
    int left = Primeagen_chatLeft(columns, listWidth);
    if (outPanel == nullptr || outSelection == nullptr || panel < 0 || panel > PRIMEAGEN_PANEL_PRIVATE ||
        !validNavigation(panel, selection) || rows < TUI_MIN_ROWS || left == 0 ||
        x < 1 || x >= left - 1 || y < 0 || y >= rows)
        return false;
    if (y == 2) {
        panel = PRIMEAGEN_PANEL_CHATS;
        selection = 0;
    } else if (y >= 3 && y <= 5) {
        panel = PRIMEAGEN_PANEL_CHATS;
        selection = y - 3;
    } else if (y == 7) {
        panel = PRIMEAGEN_PANEL_PEOPLE;
        selection = 0;
    } else if (y == 8 || y == 9) {
        panel = PRIMEAGEN_PANEL_PEOPLE;
        selection = y - 8;
    } else if (y == 11 || y == 12) {
        panel = PRIMEAGEN_PANEL_SERVERS;
        selection = 0;
    } else if (y == 13) {
        panel = PRIMEAGEN_PANEL_CHANNELS;
        selection = 3;
    } else if (y == 15) {
        panel = PRIMEAGEN_PANEL_PRIVATE;
        selection = 0;
    } else
        return false;
    *outPanel = panel;
    *outSelection = selection;
    return true;
}

bool Primeagen_drawPanels(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                          int panel, int selection, int listWidth,
                          char *row, uint8_t *styles, size_t cap) {
    if (state == nullptr || panel < 0 || panel > PRIMEAGEN_PANEL_PRIVATE || !validNavigation(panel, selection))
        return false;
    int rows, columns;
    getmaxyx(stdscr, rows, columns);
    int left = Primeagen_chatLeft(columns, listWidth);
    erase();
    if (!drawRegion(state, owo, focused, sessionMode, busy, failed, left, panel, selection, row, styles, cap))
        return false;
    int caretY, caretX;
    getyx(stdscr, caretY, caretX);
    if (left > 0 && rows >= TUI_MIN_ROWS) {
        for (int y = 0; y < rows; ++y)
            (void) mvaddwstr(y, left - 1, L"│");
        const char *tree[] = {
            "vexgraph  ~/vexgraph", "", "> chats", "  chat #1", "  chat #2", "  chat #3",
            "", "> users", "  OpenCode", "  user preview", "", "> servers",
            "  serverNumber1", "    #general", "", "> private chat"
        };
        for (int y = 0; y < (int) (sizeof tree / sizeof tree[0]) && y < rows; ++y) {
            bool selected = (panel == PRIMEAGEN_PANEL_CHATS && y >= 2 && y <= 5) ||
                            (panel == PRIMEAGEN_PANEL_PEOPLE && y >= 7 && y <= 9) ||
                            (panel == PRIMEAGEN_PANEL_SERVERS && y >= 11 && y <= 12) ||
                            (panel == PRIMEAGEN_PANEL_CHANNELS && y == 13) ||
                            (panel == PRIMEAGEN_PANEL_PRIVATE && y == 15);
            if (selected)
                attron(A_REVERSE);
            mvaddnstr(y, 1, tree[y], left - 2);
            if (selected)
                attroff(A_REVERSE);
        }
    }
    move(caretY, caretX); // sidebar painting must not relocate the input caret
    return refresh() != ERR;
}

bool Primeagen_drawTreePanels(Tui *state, TerminalTreeView *tree, bool owo, bool focused,
                              bool sessionMode, bool busy, bool failed, int panel, int selection,
                              int listWidth, char *row, uint8_t *styles, size_t cap) {
    if (state == nullptr || tree == nullptr || panel < PRIMEAGEN_PANEL_PEOPLE ||
        panel > PRIMEAGEN_PANEL_PRIVATE || !validNavigation(panel, selection))
        return false;
    int rows, columns;
    getmaxyx(stdscr, rows, columns);
    int left = Primeagen_chatLeft(columns, listWidth);
    if (left > 0)
        ++left;
    erase();
    if (!drawRegion(state, owo, focused, sessionMode, busy, failed, left, panel, selection, row, styles, cap))
        return false;
    int caretY, caretX;
    getyx(stdscr, caretY, caretX);
    if (left > 1 && rows >= TUI_MIN_ROWS) {
        int panelWidth = left - 1;
        attron(COLOR_PAIR(PRIMEAGEN_PAIR_TREE_BACKGROUND));
        for (int y = 0; y < rows; ++y)
            mvhline(y, 0, ' ', panelWidth);
        attroff(COLOR_PAIR(PRIMEAGEN_PAIR_TREE_BACKGROUND));

        attron(COLOR_PAIR(PRIMEAGEN_PAIR_TREE_FOLDER) | A_BOLD);
        (void) mvaddwstr(0, 1, L"⌄");
        (void) mvaddwstr(0, 3, L"\uf07c");
        if (panelWidth > 5)
            mvaddnstr(0, 5, "vexgraph", panelWidth - 5);
        attroff(COLOR_PAIR(PRIMEAGEN_PAIR_TREE_FOLDER) | A_BOLD);
        if (panelWidth > 14) {
            attron(COLOR_PAIR(PRIMEAGEN_PAIR_TREE_BACKGROUND) | A_DIM);
            mvaddnstr(0, 14, "~/vexgraph", panelWidth - 14);
            attroff(COLOR_PAIR(PRIMEAGEN_PAIR_TREE_BACKGROUND) | A_DIM);
        }

        size_t viewport = (size_t) (rows - 1);
        (void) TerminalTreeView_setViewportRows(tree, viewport);
        size_t first = TerminalTreeView_scrollRow(tree);
        size_t visible = TerminalTreeView_visibleCount(tree);
        for (size_t index = 0; index < viewport && first + index < visible; ++index) {
            TerminalTreeRow item = TerminalTreeView_rowAt(tree, first + index);
            int y = (int) index + 1;
            int x = 1 + (int) item.depth * 2;
            if (item.selected) {
                attron(COLOR_PAIR(PRIMEAGEN_PAIR_TREE_SELECTED));
                mvhline(y, 0, ' ', panelWidth);
            }
            const wchar_t *arrow = item.folder ? (item.expanded ? L"⌄" : L"›") : L" ";
            const wchar_t *icon = L"\uf15c";
            int iconPair = PRIMEAGEN_PAIR_TREE_CHAT;
            if (item.folder) {
                icon = item.expanded ? L"\uf07c" : L"\uf07b";
                iconPair = PRIMEAGEN_PAIR_TREE_FOLDER;
            } else if (strcmp(item.label, "OpenCode") == 0 || strcmp(item.label, "example-user") == 0) {
                icon = strcmp(item.label, "OpenCode") == 0 ? L"\uf544" : L"\uf007";
                iconPair = PRIMEAGEN_PAIR_TREE_USER;
            } else if (strcmp(item.label, "serverNumber1") == 0) {
                icon = L"\uf233";
                iconPair = PRIMEAGEN_PAIR_TREE_SERVER;
            } else if (strcmp(item.label, "#general") == 0) {
                icon = L"\uf086";
                iconPair = PRIMEAGEN_PAIR_TREE_CHAT;
            }
            attron(COLOR_PAIR(item.selected ? PRIMEAGEN_PAIR_TREE_SELECTED : PRIMEAGEN_PAIR_TREE_BACKGROUND));
            (void) mvaddwstr(y, x, arrow);
            attroff(COLOR_PAIR(item.selected ? PRIMEAGEN_PAIR_TREE_SELECTED : PRIMEAGEN_PAIR_TREE_BACKGROUND));
            attron(COLOR_PAIR(item.selected ? PRIMEAGEN_PAIR_TREE_SELECTED : iconPair));
            (void) mvaddwstr(y, x + 2, icon);
            attroff(COLOR_PAIR(item.selected ? PRIMEAGEN_PAIR_TREE_SELECTED : iconPair));
            attron(COLOR_PAIR(item.selected ? PRIMEAGEN_PAIR_TREE_SELECTED : PRIMEAGEN_PAIR_TREE_BACKGROUND));
            x += 4;
            if (x < panelWidth)
                mvaddnstr(y, x, item.label, panelWidth - x);
            attroff(COLOR_PAIR(item.selected ? PRIMEAGEN_PAIR_TREE_SELECTED : PRIMEAGEN_PAIR_TREE_BACKGROUND));
            if (item.selected)
                attroff(COLOR_PAIR(PRIMEAGEN_PAIR_TREE_SELECTED));
        }
    }
    move(caretY, caretX);
    return refresh() != ERR;
}

const void *VkModuleGetTrampolines(uint32_t *count) {
    static const struct { const char *name; void *fn; } exports[] = {
        {"Primeagen_draw", (void*) Primeagen_draw}, {"Primeagen_command", (void*) Primeagen_command},
        {"Primeagen_drawPanels", (void*) Primeagen_drawPanels},
        {"Primeagen_drawTreePanels", (void*) Primeagen_drawTreePanels},
        {"Primeagen_setAppearance", (void*) Primeagen_setAppearance},
        {"Primeagen_setBanner", (void*) Primeagen_setBanner}
    };
    if (count == nullptr)
        return nullptr;
    *count = sizeof(exports) / sizeof(exports[0]);
    return exports;
}
