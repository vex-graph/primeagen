#include "tui/tui.h"
#include "tui/event/mouse_event.h"
#include "personality/owo.h"
#include "session/session_bridge.h"
#include "cli/view.h"
#include "cli/update.h"
#include "hot/manifest.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "exception/throw.h"
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

;;DEFINITION
/* The C23 terminal preview owns ncurses screen admission and restoration, not an
 * AI conversation. It renders a borrowed Tui into an alternate screen, keeps the
 * input anchored, and normalizes keys/mouse events into display operations. Text
 * submission requires connection, not silent local echo. /connect restores the
 * terminal before replacing this process with the existing session host.
 * Explicit host-provided bridge pipes
 * route admitted turns through API Haven to an OpenCode session host. A
 * signal only requests shutdown; normal control flow restores the terminal.
 */
;;OVERVIEW
/* ============================================================================
 * MODULE: Primeagen terminal entry (cli/main.c)
 * ============================================================================
 * OWNED STATE:
 *   static volatile sig_atomic_t stopped : signal-safe shutdown request only.
 *   main locals own Tui, SessionBridge, ncurses SCREEN and growable row buffer.
 *   sessionHost is a borrowed launcher-supplied path; no credentials are owned.
 * COLOR: draw() applies a host palette including code and error text,
 *   initialized only when has_colors(). A monochrome terminal falls back to the
 *   default attributes (COLOR_PAIR yields 0), so behavior is unchanged there.
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Private helpers:
 *   static void stop(int signalNumber) : flag shutdown; no terminal API/logging.
 *   static bool draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, char *row, uint8_t *styles, size_t cap)
 *     Draw per-cell styled wrapped viewport, fixed input/status and cursor.
 *     No model-supplied terminal escape sequences; unsupported italic uses underline.
 *   static int command(Tui *, bool) : local slash dispatch; connect/quit actions.
 *   static int connectHost(const char *, bool) : exec existing adapter on Go Flash.
 * Entry:
 *   int main(int argc, char **argv)
 *     --owo / --help / --session-host / paired host --bridge-read/write fd admission;
 *     paired --update-payload/--update-checksum enable trusted local cold updates.
 *     tty/newterm; mouse/key/session polling; restore/free.
 * Contracts:
 *   Requires tty stdin/stdout and usable TERM. Non-tty rejects before ncurses.
 *   Small terminals show resize guidance; quit remains available by Ctrl-Q.
 *   Mouse support depends on terminal; keyboard provides PgUp/PgDn, End,
 *   Ctrl-L clear, Tab focus, Enter send, Backspace, Ctrl-Q quit. Resize recalculates all
 *   bounds. Input is printable ASCII; transcript controls are escaped by Tui.
 *   Default is disconnected. /connect explicitly starts the existing Python host
 *   using OpenCode login; no pasted keys, approval calls or tool execution.
 *   Session host owns external IO; native
 *   bridge consumes Haven jobs without blocking. Busy Enter preserves input;
 *   Clear changes display only, not remote history. getch uses 100ms slices. Screen
 *   calls are host terminal IO, not a realtime/bounded-join guarantee. Normal
 *   exits/SIGINT/SIGTERM restore mode; SIGKILL/crashes cannot promise restoration.
 *   HotModule owns reloadable Primeagen_draw/Primeagen_command code. Tui, bridge,
 *   permission/session host and screen remain resident. /update pauses new turns,
 *   finishes the existing turn, synchronously resolves, then resumes; rollback
 *   recovery-required stops rather than falsely resuming. No remote update/tools.
 * ============================================================================
 */
#define TUI_POLL_MS 100 /* Shutdown checks between idle terminal reads. */
#define TUI_CTRL_Q 17
#define TUI_CTRL_L 12
#define TUI_COMMAND_MESSAGE 1
#define TUI_COMMAND_CONNECT 2
#define TUI_COMMAND_QUIT 3
#define TUI_COMMAND_UPDATE 4
#define TUI_CONNECT_MODEL "opencode-go/deepseek-v4.1-flash"
enum { TUI_PAIR_HEADER = 1, TUI_PAIR_ACCENT = 2, TUI_PAIR_STATUS = 3, TUI_PAIR_INPUT = 4, TUI_PAIR_CODE = 5, TUI_PAIR_ERROR = 6 };
static volatile sig_atomic_t stopped;
static void stop(int signalNumber) {
    (void) signalNumber;
    stopped = 1;
}

/* Slash input is local; unimplemented commands are never model prompts. */
static int command(Tui *state, bool sessionMode) {
    return Primeagen_command(state, sessionMode);
}

/* Terminal must be restored first. Never transfer keys or copy auth files. */
static int connectHost(const char *path, bool owo) {
    if (path == nullptr || *path == '\0') {
        THROW("connect requires the primeagen launcher; run ./primeagen tui");
        return 1;
    }
    if (owo)
        execlp("python3", "python3", path, "--engine", "opencode", "--model", TUI_CONNECT_MODEL, "--owo", (char*) nullptr);
    else
        execlp("python3", "python3", path, "--engine", "opencode", "--model", TUI_CONNECT_MODEL, (char*) nullptr);
    THROW("could not start existing OpenCode session host");
    return 1;
}

static bool draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, char *row, uint8_t *styles, size_t cap) {
    return Primeagen_draw(state, owo, focused, sessionMode, busy, row, styles, cap);
}

int main(int argc, char **argv) {
    bool owo = false;
    const char *sessionHost = nullptr;
    const char *updatePayload = nullptr, *updateChecksum = nullptr;
    bool connectRequested = false;
    int readFd = -1, writeFd = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--owo") == 0)
            owo = true;
        else if (strcmp(argv[i], "--session-host") == 0 && i + 1 < argc)
            sessionHost = argv[++i];
        else if (strcmp(argv[i], "--update-payload") == 0 && i + 1 < argc)
            updatePayload = argv[++i];
        else if (strcmp(argv[i], "--update-checksum") == 0 && i + 1 < argc)
            updateChecksum = argv[++i];
        else if ((strcmp(argv[i], "--bridge-read") == 0 || strcmp(argv[i], "--bridge-write") == 0) && i + 1 < argc) {
            bool reading = strcmp(argv[i], "--bridge-read") == 0;
            char *end;
            long value = strtol(argv[++i], &end, 10);
            if (*end || value < 3 || value > 65535) {
                THROW("invalid host bridge descriptor");
                return 1;
            }
            if (reading)
                readFd = (int) value;
            else
                writeFd = (int) value;
        }
        else if (strcmp(argv[i], "--help") == 0) {
            puts("primeagen tui [--owo]: starts offline; /connect uses existing OpenCode Go login; /help; Ctrl-Q exits");
            return 0;
        } else {
            THROW("unknown TUI argument");
            return 1;
        }
    }
    bool sessionMode = readFd >= 0 && writeFd >= 0;
    int readFlags = sessionMode ? fcntl(readFd, F_GETFL) : 0;
    int writeFlags = sessionMode ? fcntl(writeFd, F_GETFL) : 0;
    if ((readFd >= 0) != (writeFd >= 0) || (sessionMode &&
        (readFd == writeFd || readFlags < 0 || writeFlags < 0 ||
         !(readFlags & O_NONBLOCK) || !(writeFlags & O_NONBLOCK)))) {
        THROW("host bridge requires paired nonblocking descriptors");
        return 1;
    }
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        THROW("TUI requires terminal stdin and stdout");
        return 1;
    }
    Tui state = Tui();
    SessionBridge bridge = SessionBridge(readFd, writeFd);
    SessionBridge_setOwo(&bridge, owo);
    if (!Tui_isReady(&state))
        return 1;
    HotModule *hot = nullptr;
    PrimeagenDrawFn view = draw;
    PrimeagenCommandFn localCommand = command;
    if (updatePayload != nullptr || updateChecksum != nullptr) {
        uint32_t loaded;
        if (updatePayload == nullptr || updateChecksum == nullptr ||
            !MANIFEST(MANIFEST_APP_DATA, "vexgraph", "primeagen") ||
            !MANIFEST_LIBRARY("primeagen", (const char*) nullptr) || !MANIFEST_ENSURE() ||
            !MANIFEST_REFLECT("primeagen", updatePayload) ||
            (hot = Hot_init("primeagen")) == nullptr ||
            !Primeagen_update(hot, updatePayload, updateChecksum, &loaded)) {
            HotShutdown(hot);
            Tui_free(&state);
            return 1;
        }
        view = (PrimeagenDrawFn) Hot_get_symbol(hot, "Primeagen_draw");
        localCommand = (PrimeagenCommandFn) Hot_get_symbol(hot, "Primeagen_command");
        if (view == nullptr || localCommand == nullptr) {
            HotShutdown(hot);
            Tui_free(&state);
            return 1;
        }
    }
    const char *greeting = sessionMode ? "Primeagen | OpenCode-backed chat; no model request until Enter" : "Primeagen | Type /connect or click /connect below to chat with OpenCode Go. No key is copied; uses your existing login.";
    if (!Tui_add(&state, greeting, strlen(greeting))) {
        HotShutdown(hot);
        Tui_free(&state);
        return 1;
    }
    if (sessionMode) {
        const char *notice = "OpenCode session | tools denied | Enter sends | Clear is display-only | Ctrl-Q exits";
        if (!Tui_add(&state, notice, strlen(notice))) {
            HotShutdown(hot);
            Tui_free(&state);
            return 1;
        }
    }
    SCREEN *screen = newterm(nullptr, stdout, stdin);
    if (screen == nullptr) {
        HotShutdown(hot);
        Tui_free(&state);
        THROW("terminal initialization failed");
        return 1;
    }
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(TUI_PAIR_HEADER, COLOR_CYAN, -1);
        init_pair(TUI_PAIR_ACCENT, COLOR_MAGENTA, -1);
        init_pair(TUI_PAIR_STATUS, COLOR_WHITE, -1);
        init_pair(TUI_PAIR_INPUT, COLOR_GREEN, -1);
        init_pair(TUI_PAIR_CODE, COLOR_YELLOW, -1);
        init_pair(TUI_PAIR_ERROR, COLOR_RED, -1);
    }
    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    signal(SIGPIPE, SIG_IGN);
    bool ok = cbreak() != ERR && noecho() != ERR && keypad(stdscr, true) != ERR;
    // Ctrl-Q is the application's quit key, not terminal XON. ncurses saved
    // the original mode before this change and restores it in endwin().
    struct termios mode;
    if (ok && tcgetattr(STDIN_FILENO, &mode) == 0) {
        mode.c_iflag &= ~(IXON | IXOFF);
        ok = tcsetattr(STDIN_FILENO, TCSANOW, &mode) == 0;
    }
    timeout(TUI_POLL_MS);
    mouseinterval(0);
    mmask_t mask = BUTTON1_PRESSED | BUTTON4_PRESSED;
#ifdef BUTTON5_PRESSED
    mask |= BUTTON5_PRESSED;
#endif
    mousemask(mask, nullptr);
    char *row = nullptr;
    uint8_t *styles = nullptr;
    size_t capacity = 0;
    bool focused = true;
    bool sessionFailed = false;
    bool updateRequested = false;
    while (ok && !stopped) {
        int completed = SessionBridge_poll(&bridge);
        if (completed) {
            const char *text = SessionBridge_getText(&bridge);
            const char *label = completed > 0 ? "OpenCode:" : "Connection error:";
            uint8_t role = completed > 0 ? TEXT_STYLE_ASSISTANT : TEXT_STYLE_ERROR;
            ok = Tui_addStyled(&state, label, strlen(label), role | TEXT_STYLE_BOLD, false) &&
                 Tui_addStyled(&state, text, strlen(text), 0, completed > 0);
            sessionFailed = completed < 0;
        }
        if (updateRequested && !SessionBridge_isBusy(&bridge)) {
            uint64_t before = Hot_get_generation(hot);
            uint32_t loaded = 0;
            bool updated = Primeagen_update(hot, updatePayload, updateChecksum, &loaded);
            if (Hot_requiresRecovery(hot)) {
                ok = false; // unresolved is not success/failure resolution; stop
                break;
            }
            if (updated) {
                view = (PrimeagenDrawFn) Hot_get_symbol(hot, "Primeagen_draw");
                localCommand = (PrimeagenCommandFn) Hot_get_symbol(hot, "Primeagen_command");
            }
            char notice[192];
            snprintf(notice, sizeof(notice), updated ? "Updated running CLI: generation %llu -> %llu. Resumed." : "Update rejected; running generation %llu retained. Resumed.",
                     (unsigned long long) before, (unsigned long long) Hot_get_generation(hot));
            ok = view != nullptr && localCommand != nullptr && Tui_add(&state, notice, strlen(notice));
            updateRequested = false;
            Tui_setDirty(&state, true);
        }
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
            uint8_t *nextStyles = realloc(styles, (size_t) columns + 1);
            if (nextStyles == nullptr) {
                ok = false;
                break;
            }
            styles = nextStyles;
            capacity = (size_t) columns + 1;
        }
        if (ok && Tui_isDirty(&state)) {
            ok = view(&state, owo, focused, sessionMode, SessionBridge_isBusy(&bridge), row, styles, capacity);
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
            if (!sessionMode && kind == TUI_MOUSE_CLICK && event.y == rows - 3 && event.x >= 1 && event.x <= 10) {
                connectRequested = true;
                stopped = 1;
                continue;
            }
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
            int action = localCommand(&state, sessionMode);
            if (action != 0) {
                if (action < 0)
                    ok = false;
                else if (action == TUI_COMMAND_CONNECT) {
                    connectRequested = true;
                    stopped = 1;
                } else if (action == TUI_COMMAND_QUIT)
                    stopped = 1;
                else if (action == TUI_COMMAND_UPDATE) {
                    const char *notice = hot != nullptr ? "Update requested: new submissions paused; waiting for active turn, then validate/resolve/continue." : "Update unavailable: run the primeagen launcher after sh cli/build.sh.";
                    ok = Tui_add(&state, notice, strlen(notice));
                    updateRequested = hot != nullptr;
                    Tui_inputClear(&state);
                }
                continue;
            }
            if (updateRequested)
                continue;
            if (!sessionMode) {
                const char *notice = "Not sent: disconnected. Use /connect first (existing Go login), then type your message. No local echo is a model reply.";
                ok = Tui_add(&state, notice, strlen(notice));
                continue;
            }
            if (sessionMode && (sessionFailed || SessionBridge_isBusy(&bridge) || Tui_getInputLength(&state) == 0))
                continue;
            if (sessionMode && !SessionBridge_submit(&bridge, Tui_getInput(&state)))
                continue;
            ok = Tui_addStyled(&state, "You:", 4, TEXT_STYLE_USER | TEXT_STYLE_BOLD, false) &&
                 Tui_add(&state, Tui_getInput(&state), Tui_getInputLength(&state));
            if (ok)
                Tui_inputClear(&state);
        } else if (focused && (key == KEY_BACKSPACE || key == 127 || key == '\b'))
            Tui_inputBackspace(&state);
        else if (focused && key >= ' ' && key <= '~')
            ok = Tui_inputAddChar(&state, key);
    }
    SessionBridge_free(&bridge);
    endwin();
    delscreen(screen);
    free(row);
    free(styles);
    Tui_free(&state);
    HotShutdown(hot);
    if (ok && connectRequested)
        return connectHost(sessionHost, owo);
    if (!ok)
        THROW("TUI operation failed; terminal restored");
    return ok ? 0 : 1;
}
