#include "tui/tui.h"
#include "tui/event/mouse_event.h"
#include "personality/owo.h"
#include "session/session_bridge.h"
#include "cli/view.h"
#include "cli/update.h"
#include "cli/appearance.h"
#include "image/png.h"
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
#include <locale.h>

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
 *   panel/selection/listWidth are resident presentation-only navigation values;
 *   preview examples never become real model/session identities.
 *   sessionHost is a borrowed launcher-supplied path; no credentials are owned.
 * COLOR: draw() applies a host palette including code and error text,
 *   initialized only when has_colors(). A monochrome terminal falls back to the
 *   default attributes (COLOR_PAIR yields 0), so behavior is unchanged there.
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Private helpers:
 *   static void stop(int signalNumber) : flag shutdown; no terminal API/logging.
 *   static bool draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed, char *row, uint8_t *styles, size_t cap)
 *     Draw per-cell styled wrapped viewport, fixed input/status and cursor.
 *     No model-supplied terminal escape sequences; unsupported italic uses underline.
 *   static int command(Tui *, bool) : local slash dispatch; connect/quit actions.
 *   static const char *approvalReply(int key) : once/always/reject key mapping.
 *   static bool mouseReporting(bool enabled) : own X10/SGR capture enable/restore.
 *   static bool readMouse(int key, int *x, int *y, TuiMouseKind *kind)
 *     Bounded protocol decode; coordinate outputs unchanged on malformed input.
 *   static void drawApproval(int rows, int left, int columns)
 *     Fixed decision bar, visible even while scrolling the proposed resources.
 *   static int connectHost(const char *, bool) : exec existing adapter on Go Flash.
 * Entry:
 *   int main(int argc, char **argv)
 *     --owo / --help / --session-host / paired host --bridge-read/write fd admission;
 *     paired --update-payload/--update-checksum enable trusted local cold updates.
 *     --list-width overrides the named list-column default. Ctrl-P/Ctrl-S choose
 *     People/Servers, Ctrl-O returns to the existing OpenCode chat.
 *     --select-text / Ctrl-T disables application mouse reporting and temporarily
 *     opens the current chat full-width for terminal-native drag selection/copy.
 *     Terminal owns selection/clipboard; switching restores panel navigation.
 *     tty/newterm; mouse/key/session polling; restore/free.
 * Contracts:
 *   Requires tty stdin/stdout and usable TERM. Non-tty rejects before ncurses.
 *   Small terminals show resize guidance; quit remains available by Ctrl-Q.
 *   Mouse support depends on terminal; keyboard provides Up/Down,
 *   End, Ctrl-L clear, Tab focus, Enter send, Backspace, Ctrl-Q quit. Resize
 *   recalculates all bounds. Input is printable ASCII; transcript controls are
 *   escaped by Tui. X10/SGR mouse bytes are decoded before the old ncurses mouse
 *   ABI can discard button 5; wheel 64/65 are distinct, never modifier aliases.
 *   Default is disconnected. /connect explicitly starts the existing Python host
 *   using OpenCode login; no pasted keys or tool execution happens here.
 *   The connected session runs under the host's default policy: inspection is
 *   automatic, while edits, shell, web fetch/search, external directories and
 *   `.env` reads wait for an explicit [o]nce/[a]lways/[r]eject decision typed in
 *   this terminal. The native adapter only forwards that decision; OpenCode
 *   enforces the rule and owns any saved project approval.
 *   Session host owns external IO; native
 *   bridge consumes Haven jobs without blocking. Busy Enter preserves input, and
 *   while an approval is pending only the decision keys are accepted.
 *   Clear changes display only, not remote history. getch uses 100ms slices. Screen
 *   calls are host terminal IO, not a realtime/bounded-join guarantee. Normal
 *   exits/SIGINT/SIGTERM restore mode; SIGKILL/crashes cannot promise restoration.
 *   HotModule owns reloadable Primeagen_draw/Primeagen_command code. Tui, bridge,
 *   permission/session host and screen remain resident. /update pauses new turns,
 *   finishes the existing turn, synchronously resolves, then resumes; rollback
 *   recovery-required stops rather than falsely resuming. No remote update.
 * ============================================================================
 */
#define TUI_POLL_MS 100 /* Shutdown checks between idle terminal reads. */
#define TUI_CTRL_Q 17
#define TUI_CTRL_L 12
#define TUI_CTRL_T 20
/* Distinct ncurses user-key codes; not bit masks in the v1 MEVENT ABI. */
#define TUI_MOUSE_SGR (KEY_MAX + 1)
#define TUI_MOUSE_X10 (KEY_MAX + 2)
#define TUI_MOUSE_FIELD_MAX 65535 /* Protocol safety bound, not viewport size. */
#define TUI_MOUSE_READ_MS 20 /* At most 18 suffix bytes, each bounded. */
#define TUI_COMMAND_MESSAGE 1
#define TUI_COMMAND_CONNECT 2
#define TUI_COMMAND_QUIT 3
#define TUI_COMMAND_UPDATE 4
#define TUI_CONNECT_MODEL "opencode-go/deepseek-v4.1-flash"
enum { TUI_PAIR_HEADER = 1, TUI_PAIR_ACCENT = 2, TUI_PAIR_STATUS = 3, TUI_PAIR_INPUT = 4, TUI_PAIR_CODE = 5, TUI_PAIR_ERROR = 6 };
static volatile sig_atomic_t stopped;

/* Own capture explicitly: ncurses v1 cannot represent both wheel directions.
 * Ctrl-T and normal shutdown disable both modes, restoring terminal selection. */
static bool mouseReporting(bool enabled) {
    return fputs(enabled ? "\033[?1000h\033[?1006h" : "\033[?1000l\033[?1006l", stdout) >= 0 && fflush(stdout) == 0;
}

/* Prefixes are recognized by ncurses' key trie; only their bounded suffix is
 * read here. SGR supports large coordinates; legacy X10 is a fallback. No raw
 * mouse byte is entered into the user's draft. Releases/motion are ignored. */
static bool readMouse(int key, int *outX, int *outY, TuiMouseKind *outKind) {
    unsigned fields[3] = {0};
    bool valid = true, pressed = true;
    timeout(TUI_MOUSE_READ_MS);
    if (key == TUI_MOUSE_X10) {
        for (size_t i = 0; i < 3; ++i) {
            int byte = getch();
            if (byte < 32 || byte > 255) {
                valid = false;
                break;
            }
            fields[i] = (unsigned) byte - 32;
        }
    } else if (key == TUI_MOUSE_SGR) {
        for (size_t i = 0; i < 3 && valid; ++i) {
            int byte = getch();
            unsigned digits = 0;
            while (byte >= '0' && byte <= '9' && digits < 5) {
                fields[i] = fields[i] * 10 + (unsigned) (byte - '0');
                ++digits;
                byte = getch();
            }
            if (digits == 0 || fields[i] > TUI_MOUSE_FIELD_MAX ||
                (i < 2 ? byte != ';' : byte != 'M' && byte != 'm'))
                valid = false;
            if (i == 2)
                pressed = byte == 'M';
        }
    } else
        valid = false;
    timeout(TUI_POLL_MS);
    if (!valid || !pressed || fields[1] == 0 || fields[2] == 0)
        return false;
    unsigned button = fields[0] & ~(4u | 8u | 16u); // modifier flags only
    TuiMouseKind kind;
    if (button == 64)
        kind = TUI_MOUSE_WHEEL_UP;
    else if (button == 65)
        kind = TUI_MOUSE_WHEEL_DOWN;
    else if (button == 0)
        kind = TUI_MOUSE_CLICK;
    else
        return false;
    *outX = (int) fields[1] - 1;
    *outY = (int) fields[2] - 1;
    *outKind = kind;
    return true;
}
static void stop(int signalNumber) {
    (void) signalNumber;
    stopped = 1;
}

/* Slash input is local; unimplemented commands are never model prompts. */
static int command(Tui *state, bool sessionMode) {
    return Primeagen_command(state, sessionMode);
}

/* One pending question at a time: o/y approve once, a always, r/n reject. */
static const char *approvalReply(int key) {
    if (key == 'o' || key == 'y')
        return "once";
    if (key == 'a')
        return "always";
    if (key == 'r' || key == 'n')
        return "reject";
    return nullptr;
}

static void drawApproval(int rows, int left, int columns) {
    if (rows < TUI_MIN_ROWS || columns < TUI_MIN_COLUMNS)
        return;
    mvhline(rows - 2, left, ' ', columns - 1);
    attron(A_BOLD);
    mvaddnstr(rows - 2, left, "Accept? [y] Yes  [a] Always yes  [n] No", columns - 1);
    attroff(A_BOLD);
    curs_set(0);
}

/* The OpenCode speaker label appears once per turn; the caller keeps the flag.
 * Only this loop appends transcript entries during a turn, so the live
 * "working..." placeholder stays the newest entry whenever it is shown. */
static bool openTurn(Tui *state, bool *open) {
    if (*open)
        return true;
    *open = Tui_addStyled(state, "OpenCode:", 9, TEXT_STYLE_ASSISTANT | TEXT_STYLE_BOLD, false);
    return *open;
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

static bool draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed, char *row, uint8_t *styles, size_t cap) {
    return Primeagen_draw(state, owo, focused, sessionMode, busy, failed, row, styles, cap);
}

int main(int argc, char **argv) {
    bool owo = false;
    bool selectText = false;
    const char *sessionHost = nullptr;
    const char *updatePayload = nullptr, *updateChecksum = nullptr;
    bool connectRequested = false;
    int panel = PRIMEAGEN_PANEL_PEOPLE, selection = 0;
    int listWidth = PRIMEAGEN_LIST_WIDTH_DEFAULT;
    int readFd = -1, writeFd = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--owo") == 0)
            owo = true;
        else if (strcmp(argv[i], "--select-text") == 0)
            selectText = true;
        else if (strcmp(argv[i], "--session-host") == 0 && i + 1 < argc)
            sessionHost = argv[++i];
        else if (strcmp(argv[i], "--update-payload") == 0 && i + 1 < argc)
            updatePayload = argv[++i];
        else if (strcmp(argv[i], "--update-checksum") == 0 && i + 1 < argc)
            updateChecksum = argv[++i];
        else if (strcmp(argv[i], "--list-width") == 0 && i + 1 < argc) {
            char *end;
            long value = strtol(argv[++i], &end, 10);
            if (*end || value < 12 || value > 4096) {
                THROW("list width must be 12..4096 terminal cells");
                return 1;
            }
            listWidth = (int) value;
        }
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
            puts("primeagen tui [--owo] [--list-width CELLS] [--select-text]: offline three-column browser; Ctrl-T toggles native text selection; Ctrl-P People, Ctrl-S Servers, Ctrl-O OpenCode; /connect uses existing login; approvals use o/a/r; Ctrl-Q exits");
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
    PrimeagenPanelsFn panelsView = Primeagen_drawPanels;
    PrimeagenCommandFn localCommand = command;
    PrimeagenAppearanceFn appearanceView = Primeagen_setAppearance;
    PrimeagenBadgeFn badgeView = Primeagen_setBadge;
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
        panelsView = (PrimeagenPanelsFn) Hot_get_symbol(hot, "Primeagen_drawPanels");
        appearanceView = (PrimeagenAppearanceFn) Hot_get_symbol(hot, "Primeagen_setAppearance");
        {
            PrimeagenBadgeFn refreshedBadge = (PrimeagenBadgeFn) Hot_get_symbol(hot, "Primeagen_setBadge");
            if (refreshedBadge != nullptr)
                badgeView = refreshedBadge;
        }
        if (view == nullptr || localCommand == nullptr || panelsView == nullptr) {
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
        const char *notice = "OpenCode session | read/glob/grep auto | edit/shell/web ask: [o]nce [a]lways [r]eject | Ctrl-Q exits";
        if (!Tui_add(&state, notice, strlen(notice))) {
            HotShutdown(hot);
            Tui_free(&state);
            return 1;
        }
    }
#include "src/settings.h"

    // Apply the persisted look before the first frame: the appearance lives in the
    // view module, so it is set through the same pointer the draws use.
    Settings settings = Settings_0();
    char settingsPath[600];
    if (Settings_resolvePath(settingsPath, sizeof settingsPath))
        (void) Settings_load(&settings, settingsPath);
    // Consent is loaded with the record; the C-TUI tool gate consumes it when
    // the consent prompt lands. Referencing the type keeps the read explicit.
    SettingsConsent consent = (SettingsConsent) Settings_getConsent(&settings);
    (void) consent;
    (void) appearanceView(Settings_getAppearance(&settings));

    // The app's badge: decode the PNG once with a libc-backed buffer (this lean
    // host owns no engine arena), then hand the pixels to the view. The view
    // rasterises them into terminal cells on every render, so a resize simply
    // re-samples — the picture is never a cached bitmap.
    uint8_t *badgePixels = nullptr;
    uint32_t badgeWidth = 0, badgeHeight = 0;
    {
        char badgePath[1200];
        const char *badgeRoot = getenv("PRIMEAGEN_ROOT");
        if (badgeRoot != nullptr) {
            (void) snprintf(badgePath, sizeof badgePath, "%s/resources/>w<.png", badgeRoot);
        } else {
            char dir[1000];
            (void) snprintf(dir, sizeof dir, "%s", argv[0]);
            char *slash = strrchr(dir, '/');
            if (slash != nullptr)
                *slash = '\0';
            (void) snprintf(badgePath, sizeof badgePath, "%s/../resources/>w<.png", dir);
        }
        FILE *badgeFile = fopen(badgePath, "rb");
        if (badgeFile != nullptr) {
            static uint8_t pngBytes[1u << 20];
            size_t pngLength = fread(pngBytes, 1, sizeof pngBytes, badgeFile);
            (void) fclose(badgeFile);
            if (Png_decodeMalloc(pngBytes, pngLength, &badgePixels, &badgeWidth, &badgeHeight) == PNG_OK)
                (void) badgeView(badgePixels, badgeWidth, badgeHeight);
        }
    }

    setlocale(LC_CTYPE, ""); // text symbols, not emoji; ASCII fallback if unavailable
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
    mousemask(0, nullptr); // do not feed wheel-down into ncurses' v1 mouse ABI
    ok = ok && define_key("\033[<", TUI_MOUSE_SGR) != ERR &&
         define_key("\033[M", TUI_MOUSE_X10) != ERR && mouseReporting(!selectText);
    char *row = nullptr;
    uint8_t *styles = nullptr;
    size_t capacity = 0;
    bool focused = true;
    bool sessionFailed = false;
    bool updateRequested = false;
    bool turnOpen = false;
    bool workingShown = false;
    uint32_t shownApproval = 0;
    while (ok && !stopped) {
        int completed = SessionBridge_poll(&bridge);
        if (workingShown) {
            (void) Tui_popLast(&state); // always the trailing entry while shown
            workingShown = false;
        }
        if (completed) {
            const char *text = SessionBridge_getText(&bridge);
            ok = openTurn(&state, &turnOpen);
            if (ok && completed > 0) {
                // A turn that streamed its conclusion ends without an answer block.
                if (*text != '\0')
                    ok = Tui_addStyled(&state, "", 0, 0, false) &&
                         Tui_addStyled(&state, "answer:", 7, TEXT_STYLE_ASSISTANT | TEXT_STYLE_BOLD, false) &&
                         Tui_addStyled(&state, text, strlen(text), 0, true);
                if (ok)
                    ok = Tui_addStyled(&state, "", 0, 0, false);
            } else if (ok)
                ok = Tui_addStyled(&state, "Connection error:", 17, TEXT_STYLE_ERROR | TEXT_STYLE_BOLD, false) &&
                     Tui_addStyled(&state, text, strlen(text), TEXT_STYLE_ERROR, false) &&
                     Tui_addStyled(&state, "", 0, 0, false);
            sessionFailed = completed < 0;
            turnOpen = false;
            shownApproval = 0;
        }
        // Tool activity streams in as it happens, not as one lump at the end.
        for (const char *note = SessionBridge_takeNote(&bridge); ok && note != nullptr; note = SessionBridge_takeNote(&bridge))
            ok = openTurn(&state, &turnOpen) && Tui_addStyled(&state, note, strlen(note), TEXT_STYLE_CODE, false);
        if (SessionBridge_isAwaiting(&bridge) && SessionBridge_getApprovalSerial(&bridge) != shownApproval) {
            shownApproval = SessionBridge_getApprovalSerial(&bridge);
            if (workingShown) {
                (void) Tui_popLast(&state); // keep the live placeholder last: restore it below
                workingShown = false;
            }
            char question[SESSION_APPROVAL_LABEL_CAP + 96];
            snprintf(question, sizeof(question), "Do you accept %s?  [y] Yes  [a] Always yes  [n] No",
                      SessionBridge_getApprovalLabel(&bridge));
            ok = openTurn(&state, &turnOpen) &&
                  Tui_addStyled(&state, question, strlen(question), TEXT_STYLE_ERROR | TEXT_STYLE_BOLD, false);
            panel = PRIMEAGEN_PANEL_PEOPLE;
            selection = 0;
            Tui_setScroll(&state, 0); // approval must be visible, not above the viewport
            Tui_setDirty(&state, true);
        }
        if (ok && !workingShown && (SessionBridge_isBusy(&bridge) || SessionBridge_isAwaiting(&bridge)))
            ok = openTurn(&state, &turnOpen) &&
                 Tui_addStyled(&state, "working...", 10, TEXT_STYLE_CODE, false) && (workingShown = true);
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
                panelsView = (PrimeagenPanelsFn) Hot_get_symbol(hot, "Primeagen_drawPanels");
                PrimeagenAppearanceFn refreshed = (PrimeagenAppearanceFn) Hot_get_symbol(hot, "Primeagen_setAppearance");
                if (refreshed != nullptr)
                    appearanceView = refreshed;
                (void) appearanceView(PrimeagenAppearance_name());
            }
            char notice[192];
            snprintf(notice, sizeof(notice), updated ? "Updated running CLI: generation %llu -> %llu. Resumed." : "Update rejected; running generation %llu retained. Resumed.",
                     (unsigned long long) before, (unsigned long long) Hot_get_generation(hot));
            ok = view != nullptr && localCommand != nullptr && panelsView != nullptr && Tui_add(&state, notice, strlen(notice));
            updateRequested = false;
            Tui_setDirty(&state, true);
        }
        int rows, columns;
        getmaxyx(stdscr, rows, columns);
        int chatLeft = selectText ? 0 : Primeagen_chatLeft(columns, listWidth);
        int chatColumns = columns - chatLeft;
        int oldRows, oldColumns;
        Tui_getSize(&state, &oldRows, &oldColumns);
        if (oldRows != rows || oldColumns != chatColumns) {
            ok = Tui_setSize(&state, rows, chatColumns);
            // A zoom, a fullscreen toggle or a drag changes the size; ncurses can
            // consume that change itself, so clear before repainting and never let
            // stale cells survive a resize.
            (void) clear();
        }
        size_t needed = Tui_getRowCapacity(&state);
        if ((size_t) columns + 1 > needed)
            needed = (size_t) columns + 1;
        if (needed > capacity) {
            char *next = realloc(row, needed);
            if (next == nullptr) {
                ok = false;
                break;
            }
            row = next;
            uint8_t *nextStyles = realloc(styles, needed);
            if (nextStyles == nullptr) {
                ok = false;
                break;
            }
            styles = nextStyles;
            capacity = needed;
        }
        if (ok && Tui_isDirty(&state)) {
            if (selectText) {
                ok = view(&state, owo, focused, sessionMode, SessionBridge_isBusy(&bridge), sessionFailed, row, styles, capacity);
                if (ok && rows >= TUI_MIN_ROWS && columns >= TUI_MIN_COLUMNS) {
                    int caretY, caretX;
                    getyx(stdscr, caretY, caretX);
                    mvhline(rows - 1, 0, ' ', columns - 1);
                    mvaddnstr(rows - 1, 0, "Select text: drag, then terminal Copy | Ctrl-T returns", columns - 1);
                    move(caretY, caretX);
                    ok = refresh() != ERR;
                }
            } else
                ok = panelsView(&state, owo, focused, sessionMode, SessionBridge_isBusy(&bridge), sessionFailed, panel, selection, listWidth, row, styles, capacity);
            if (ok && SessionBridge_isAwaiting(&bridge)) {
                drawApproval(rows, chatLeft, chatColumns);
                ok = refresh() != ERR;
            }
            Tui_setDirty(&state, false);
        }
        if (!ok)
            break;
        int key = getch();
        if (key == TUI_CTRL_Q)
            stopped = 1;
        else if (key == TUI_CTRL_T) {
            selectText = !selectText;
            ok = mouseReporting(!selectText);
            Tui_setDirty(&state, true);
        }
        else if (SessionBridge_isAwaiting(&bridge) && approvalReply(key) != nullptr) {
            // One pending question at a time: only o/y, a and r/n answer it.
            const char *reply = approvalReply(key);
            if (!SessionBridge_decide(&bridge, reply))
                ok = false;
            else {
                char notice[128];
                snprintf(notice, sizeof(notice), "Decision sent: %s.", reply);
                ok = Tui_add(&state, notice, strlen(notice));
            }
            Tui_setDirty(&state, true);
        }
        else if (key == 16 || key == 19 || key == 15) {
            if (key == 16)
                panel = PRIMEAGEN_PANEL_PEOPLE;
            else if (key == 19)
                panel = PRIMEAGEN_PANEL_SERVERS;
            else {
                panel = PRIMEAGEN_PANEL_PEOPLE;
                selection = 0;
                focused = true;
            }
            Tui_setDirty(&state, true);
        }
        else if (key == TUI_MOUSE_SGR || key == TUI_MOUSE_X10) {
            int x, y;
            TuiMouseKind kind;
            if (!readMouse(key, &x, &y, &kind))
                continue;
            if (selectText)
                continue; // terminal, not application, owns selection/copy
            if (kind == TUI_MOUSE_CLICK && Primeagen_panelsClick(x, y, rows, columns, listWidth, panel, selection, &panel, &selection)) {
                focused = selection == 0;
                Tui_setDirty(&state, true);
                continue;
            }
            if (!sessionMode && selection == 0 && kind == TUI_MOUSE_CLICK && y == rows - 3 && x >= chatLeft + 1 && x <= chatLeft + 10) {
                connectRequested = true;
                stopped = 1;
                continue;
            }
            if (x < chatLeft)
                continue; // the list/rail never scroll the chat viewport
            // Scrolling the chat is always available, whatever the list selection.
            if (selection != 0 && kind != TUI_MOUSE_CLICK && kind != TUI_MOUSE_WHEEL_UP && kind != TUI_MOUSE_WHEEL_DOWN)
                continue;
            TuiMouseAction action = TuiMouse_dispatch(&state, x - chatLeft, y, kind);
            if (action == TUI_MOUSE_QUIT)
                stopped = 1;
            else if (action == TUI_MOUSE_CLEAR && selection == 0) {
                Tui_clear(&state);
                workingShown = false; // the placeholder was cleared with everything else
                turnOpen = false;
            }
            else if (action == TUI_MOUSE_INPUT && selection == 0)
                focused = true;
            else if (kind == TUI_MOUSE_CLICK)
                focused = false;
            Tui_setDirty(&state, true);
        } else if (key == KEY_UP)
            Tui_scrollBy(&state, 1);
        else if (key == KEY_DOWN)
            Tui_scrollBy(&state, -1);
        else if (key == KEY_END)
            Tui_setScroll(&state, 0);
        else if (key == TUI_CTRL_L && selection == 0) {
            Tui_clear(&state);
            workingShown = false; // the placeholder was cleared with everything else
            turnOpen = false;
        }
        else if (key == '\t' && selection == 0) {
            focused = true;
            Tui_setDirty(&state, true);
        }
        else if (key == KEY_RESIZE)
            Tui_setDirty(&state, true);
        else if (SessionBridge_isAwaiting(&bridge))
            continue; // scroll/navigation allowed, draft/submission stays frozen
        else if (focused && selection == 0 && (key == '\n' || key == KEY_ENTER)) {
            int action = localCommand(&state, sessionMode);
            if (action != 0) {
                // The command may have appended or cleared output, so the live
                // placeholder is no longer guaranteed to be the trailing entry.
                // Drop the flag and let the loop append a fresh one instead of
                // popping a line the placeholder no longer owns.
                workingShown = false;
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
            if (sessionMode && sessionFailed) {
                // The wire is poisoned; say so instead of silently ignoring Enter.
                const char *stuck = "This session is closed after a failure. Ctrl-Q, relaunch, and reconnect to continue.";
                ok = Tui_addStyled(&state, stuck, strlen(stuck), TEXT_STYLE_ERROR | TEXT_STYLE_BOLD, false);
                continue;
            }
            if (sessionMode && (SessionBridge_isBusy(&bridge) || Tui_getInputLength(&state) == 0))
                continue;
            if (sessionMode && !SessionBridge_submit(&bridge, Tui_getInput(&state)))
                continue;
            ok = Tui_addStyled(&state, "You:", 4, TEXT_STYLE_USER | TEXT_STYLE_BOLD, false) &&
                 Tui_add(&state, Tui_getInput(&state), Tui_getInputLength(&state)) &&
                 Tui_add(&state, "", 0);
            if (ok)
                Tui_inputClear(&state);
            turnOpen = false; // the speaker label streams with the first output
            workingShown = false;
        } else if (focused && selection == 0 && (key == KEY_BACKSPACE || key == 127 || key == '\b'))
            Tui_inputBackspace(&state);
        else if (focused && selection == 0 && key >= ' ' && key <= '~')
            ok = Tui_inputAddChar(&state, key);
    }
    SessionBridge_free(&bridge);
    (void) mouseReporting(false);
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
