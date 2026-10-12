#include "cli/view.h"
#include "hot/hot.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <curses.h>
#include <string.h>

;;DEFINITION
/* The reloadable terminal view paints resident Tui state. It owns neither the
 * screen nor conversation, session pipes or permissions. A cold host-excluded
 * swap changes this code while the same ncurses screen/input remain alive. */
;;OVERVIEW
/* ============================================================================
 * MODULE: Primeagen view (cli/view.c)
 * OWNED STATE: none; host owns every resource and palette.
 * FUNCTION REGISTRY:
 *   bool Primeagen_draw(Tui *, bool owo, bool focused, bool sessionMode,
 *                       bool busy, char *row, uint8_t *styles, size_t cap)
 *       Paint resident model into caller's live ncurses screen.
 *   const void *VkModuleGetTrampolines(uint32_t *count) : trusted loader exports.
 *   int Primeagen_command(Tui *, bool sessionMode) : local command admission;
 *       emits display notices/actions, never executes model/provider/tools.
 * PRIVATE: attributes(style) : host-defined visible style to ncurses attributes.
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
        notice = "/connect: existing OpenCode Go login | /update: local built CLI, pause/resolve/continue | /clear: display only | /quit: exit. Chat-only; settings/servers are not implemented yet.";
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

bool Primeagen_draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy,
                    char *row, uint8_t *styles, size_t cap) {
    if (state == nullptr)
        return false;
    int rows, columns;
    Tui_getSize(state, &rows, &columns);
    erase();
    if (rows < TUI_MIN_ROWS || columns < TUI_MIN_COLUMNS) {
        mvaddnstr(0, 0, "Resize terminal; Ctrl-Q exits", columns > 1 ? columns - 1 : 0);
        curs_set(0);
        return refresh() != ERR;
    }
    attron(COLOR_PAIR(VIEW_HEADER) | A_BOLD);
    mvaddnstr(0, 1, PRIMEAGEN_VIEW_TITLE, columns - 16);
    attroff(COLOR_PAIR(VIEW_HEADER) | A_BOLD);
    attron(COLOR_PAIR(VIEW_ACCENT));
    mvaddstr(0, columns - 15, "[Clear]");
    mvaddstr(0, columns - 7, "[Quit]");
    attroff(COLOR_PAIR(VIEW_ACCENT));
    mvhline(1, 0, ACS_HLINE, columns);
    size_t first = Tui_getFirstVisibleRow(state);
    size_t total = Tui_getVisualRowCount(state);
    size_t height = Tui_getViewportHeight(state);
    for (size_t i = 0; i < height && first + i < total; ++i) {
        if (!Tui_copyStyledRow(state, first + i, row, styles, cap, nullptr))
            return false;
        for (size_t j = 0; row[j] != '\0'; ++j)
            mvaddch((int) i + 2, (int) j + 1, (chtype) (unsigned char) row[j] | attributes(styles[j]));
    }
    mvhline(rows - 4, 0, ACS_HLINE, columns);
    attron(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    mvaddnstr(rows - 3, 1, sessionMode ? "/help | /update | PgUp/PgDn scroll | Ctrl-Q quit" : "/connect (click here) | /help | /update | Ctrl-Q quit", columns - 2);
    attroff(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    attron(COLOR_PAIR(VIEW_INPUT) | A_BOLD);
    mvaddstr(rows - 2, 0, "> ");
    attroff(COLOR_PAIR(VIEW_INPUT) | A_BOLD);
    size_t length = Tui_getInputLength(state);
    size_t width = (size_t) columns - 3;
    size_t offset = length > width ? length - width : 0;
    mvaddnstr(rows - 2, 2, Tui_getInput(state) + offset, (int) width);
    const char *status = sessionMode ? (busy ? "OpenCode working | Ctrl-Q cancels locally" : "OpenCode chat | tools denied | Enter sends") : (owo ? "OwO | disconnected | /connect uses Go login" : "Disconnected | /connect uses OpenCode Go login");
    attron(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    mvaddnstr(rows - 1, 0, status, columns - 1);
    attroff(COLOR_PAIR(VIEW_STATUS) | A_DIM);
    curs_set(focused ? 1 : 0);
    move(rows - 2, 2 + (int) (length - offset));
    return refresh() != ERR;
}

const void *VkModuleGetTrampolines(uint32_t *count) {
    static const struct { const char *name; void *fn; } exports[] = {
        {"Primeagen_draw", (void*) Primeagen_draw}, {"Primeagen_command", (void*) Primeagen_command}
    };
    if (count == nullptr)
        return nullptr;
    *count = sizeof(exports) / sizeof(exports[0]);
    return exports;
}
