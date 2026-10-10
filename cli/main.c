#include "tui/tui.h"
#include "tui/event/mouse_event.h"
#include "personality/owo.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "exception/throw.h"
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

;;DEFINITION
/* The C23 terminal preview owns ncurses screen admission and restoration, not an
 * AI conversation. It renders a borrowed Tui into an alternate screen, keeps the
 * input anchored, and normalizes keys/mouse events into display operations. Text
 * submission appends locally, never contacts providers or executes tools. A
 * signal only requests shutdown; normal control flow restores the terminal.
 */
;;OVERVIEW
/* ============================================================================
 * MODULE: Primeagen terminal entry (cli/main.c)
 * ============================================================================
 * OWNED STATE:
 *   static volatile sig_atomic_t stopped : signal-safe shutdown request only.
 *   main locals own Tui, ncurses SCREEN and growable row projection buffer.
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Private helpers:
 *   static void stop(int signalNumber) : flag shutdown; no terminal API/logging.
 *   static bool draw(Tui *state, bool owo, bool focused, char *row, size_t cap)
 *     Draw header/buttons, wrapped viewport, fixed input/status and cursor.
 * Entry:
 *   int main(int argc, char **argv)
 *     --owo / --help admission; tty/newterm; mouse/key dispatch; restore/free.
 * Contracts:
 *   Requires tty stdin/stdout and usable TERM. Non-tty rejects before ncurses.
 *   Small terminals show resize guidance; quit remains available by Ctrl-Q.
 *   Mouse support depends on terminal; keyboard provides PgUp/PgDn, End,
 *   Ctrl-L clear, Tab focus, Enter append, Backspace, Ctrl-Q quit. Resize recalculates all
 *   bounds. Input is printable ASCII; transcript controls are escaped by Tui.
 *   No worker/provider/tool; getch uses named 100ms cancellation slices. Screen
 *   calls are host terminal IO, not a realtime/bounded-join guarantee. Normal
 *   exits/SIGINT/SIGTERM restore mode; SIGKILL/crashes cannot promise restoration.
 * ============================================================================
 */
#define TUI_POLL_MS 100 /* Shutdown checks between idle terminal reads. */
#define TUI_CTRL_Q 17
#define TUI_CTRL_L 12
static volatile sig_atomic_t stopped;
static void stop(int signalNumber) {
    (void) signalNumber;
    stopped = 1;
}

static bool draw(Tui *state, bool owo, bool focused, char *row, size_t cap) {
    int rows, columns;
    Tui_getSize(state, &rows, &columns);
    erase();
    if (rows < TUI_MIN_ROWS || columns < TUI_MIN_COLUMNS) {
        mvaddnstr(0, 0, "Resize terminal; Ctrl-Q exits", columns > 1 ? columns - 1 : 0);
        curs_set(0);
        return refresh() != ERR;
    }
    mvaddnstr(0, 1, "Primeagen", columns - 16);
    mvaddstr(0, columns - 15, "[Clear]");
    mvaddstr(0, columns - 7, "[Quit]");
    mvhline(1, 0, ACS_HLINE, columns);
    size_t first = Tui_getFirstVisibleRow(state);
    size_t total = Tui_getVisualRowCount(state);
    size_t height = Tui_getViewportHeight(state);
    for (size_t i = 0; i < height && first + i < total; ++i) {
        if (!Tui_copyRow(state, first + i, row, cap, nullptr))
            return false;
        mvaddnstr((int) i + 2, 1, row, columns - 2);
    }
    mvhline(rows - 4, 0, ACS_HLINE, columns);
    mvaddnstr(rows - 3, 1, "PgUp/PgDn scroll | Ctrl-L clear | Ctrl-Q quit", columns - 2);
    mvaddstr(rows - 2, 0, "> ");
    size_t length = Tui_getInputLength(state);
    size_t width = (size_t) columns - 3;
    size_t offset = length > width ? length - width : 0;
    mvaddnstr(rows - 2, 2, Tui_getInput(state) + offset, (int) width);
    mvaddnstr(rows - 1, 0, Owo_status(owo), columns - 1);
    curs_set(focused ? 1 : 0);
    move(rows - 2, 2 + (int) (length - offset));
    return refresh() != ERR;
}

int main(int argc, char **argv) {
    bool owo = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--owo") == 0)
            owo = true;
        else if (strcmp(argv[i], "--help") == 0) {
            puts("primeagen tui [--owo]: offline C23 ncurses preview; Ctrl-Q exits");
            return 0;
        } else {
            THROW("unknown TUI argument");
            return 1;
        }
    }
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        THROW("TUI requires terminal stdin and stdout");
        return 1;
    }
    Tui state = Tui();
    if (!Tui_isReady(&state))
        return 1;
    const char *greeting = Owo_greeting(owo);
    if (!Tui_add(&state, greeting, strlen(greeting))) {
        Tui_free(&state);
        return 1;
    }
    SCREEN *screen = newterm(nullptr, stdout, stdin);
    if (screen == nullptr) {
        Tui_free(&state);
        THROW("terminal initialization failed");
        return 1;
    }
    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    bool ok = cbreak() != ERR && noecho() != ERR && keypad(stdscr, true) != ERR;
    timeout(TUI_POLL_MS);
    mouseinterval(0);
    mmask_t mask = BUTTON1_PRESSED | BUTTON4_PRESSED;
#ifdef BUTTON5_PRESSED
    mask |= BUTTON5_PRESSED;
#endif
    mousemask(mask, nullptr);
    char *row = nullptr;
    size_t capacity = 0;
    bool focused = true;
    while (ok && !stopped) {
        int rows, columns;
        getmaxyx(stdscr, rows, columns);
        int oldRows, oldColumns;
        Tui_getSize(&state, &oldRows, &oldColumns);
        if (oldRows != rows || oldColumns != columns)
            ok = Tui_setSize(&state, rows, columns);
        if ((size_t) columns + 1 > capacity) {
            char *next = realloc(row, (size_t) columns + 1);
            if (next == nullptr) {
                ok = false;
                break;
            }
            row = next;
            capacity = (size_t) columns + 1;
        }
        if (ok && Tui_isDirty(&state)) {
            ok = draw(&state, owo, focused, row, capacity);
            Tui_setDirty(&state, false);
        }
        if (!ok)
            break;
        int key = getch();
        if (key == TUI_CTRL_Q)
            stopped = 1;
        else if (key == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) != OK)
                continue;
            TuiMouseKind kind;
            if (event.bstate & BUTTON4_PRESSED)
                kind = TUI_MOUSE_WHEEL_UP;
#ifdef BUTTON5_PRESSED
            else if (event.bstate & BUTTON5_PRESSED)
                kind = TUI_MOUSE_WHEEL_DOWN;
#endif
            else if (event.bstate & BUTTON1_PRESSED)
                kind = TUI_MOUSE_CLICK;
            else
                continue;
            TuiMouseAction action = TuiMouse_dispatch(&state, event.x, event.y, kind);
            if (action == TUI_MOUSE_QUIT)
                stopped = 1;
            else if (action == TUI_MOUSE_CLEAR)
                Tui_clear(&state);
            else if (action == TUI_MOUSE_INPUT)
                focused = true;
            else if (kind == TUI_MOUSE_CLICK)
                focused = false;
            Tui_setDirty(&state, true);
        } else if (key == KEY_PPAGE)
            Tui_scrollBy(&state, (int64_t) Tui_getViewportHeight(&state));
        else if (key == KEY_NPAGE)
            Tui_scrollBy(&state, -(int64_t) Tui_getViewportHeight(&state));
        else if (key == KEY_END)
            Tui_setScroll(&state, 0);
        else if (key == TUI_CTRL_L)
            Tui_clear(&state);
        else if (key == '\t') {
            focused = true;
            Tui_setDirty(&state, true);
        }
        else if (key == KEY_RESIZE)
            Tui_setDirty(&state, true);
        else if (focused && (key == '\n' || key == KEY_ENTER)) {
            ok = Tui_add(&state, Tui_getInput(&state), Tui_getInputLength(&state));
            if (ok)
                Tui_inputClear(&state);
        } else if (focused && (key == KEY_BACKSPACE || key == 127 || key == '\b'))
            Tui_inputBackspace(&state);
        else if (focused && key >= ' ' && key <= '~')
            ok = Tui_inputAddChar(&state, key);
    }
    endwin();
    delscreen(screen);
    free(row);
    Tui_free(&state);
    if (!ok)
        THROW("TUI operation failed; terminal restored");
    return ok ? 0 : 1;
}
