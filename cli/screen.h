#ifndef PRIMEAGEN_CLI_SCREEN_H
#define PRIMEAGEN_CLI_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <curses.h>

#include "tui/tui.h"
#include "terminal/terminal_canvas.h"
#include "terminal/terminal_image.h"
#include "terminal/terminal_appearance.h"

// cli/screen.h — Primeagen's frame, composed by the Determinant toolkit.
//
// The old view drove ncurses directly (mvaddnstr/attron per row), so it was a
// screen DRIVER: a layout change needed a host rebuild. The screen is a
// RENDERER: it composes the whole frame into an owned cell grid with the toolkit
// (region frame, list/text/input/status widgets, appearance rules, an image) and
// hands ncurses ONE blit. That is what makes the look swappable.
//
// Two layouts, matching `_notes/_self/tui.txt`:
//   HOME  — the landing screen: title + People/Servers/Settings menu in the left
//           rail, and the app's badge image filling the panel beside it.
//   CHAT  — three columns: left rail (back + channels + add), centre (title,
//           transcript, palette, input, status) and right users rail.
//
// Primeagen owns every string, colour and glyph choice; the toolkit owns cells,
// geometry and widgets. The screen owns its four planes and nothing else.

#define PRIMEAGEN_SCREEN_LEFT_DEFAULT 22
#define PRIMEAGEN_SCREEN_RIGHT_DEFAULT 24
// Below this the frame cannot resolve its columns/rows honestly, so it shows a
// centred notice instead of a half-drawn screen.
#define PRIMEAGEN_SCREEN_MIN_COLUMNS 90
#define PRIMEAGEN_SCREEN_MIN_ROWS 20
#define PRIMEAGEN_SCREEN_TOO_SMALL "Rendering error! Must be at least 90 x 20 to resolve the whole thing"
// How often a terminal host re-checks the size while idle (ms). A zoom, a
// fullscreen toggle or a window drag must revalidate; ncurses can consume the
// size change itself and never deliver KEY_RESIZE, so the host must poll.
#define PRIMEAGEN_SCREEN_POLL_MS 100

typedef struct PrimeagenScreen {
    uint32_t *cells;    // codepoint plane
    uint8_t *styles;    // attribute plane
    uint32_t *fg;       // 0xRRGGBBAA foreground plane
    uint32_t *bg;       // 0xRRGGBBAA background plane
    size_t rows;
    size_t columns;
    size_t capacity;    // admitted rows*columns
} PrimeagenScreen;

void PrimeagenScreen_init(PrimeagenScreen *self);
bool PrimeagenScreen_bind(PrimeagenScreen *self, size_t rows, size_t columns);
void PrimeagenScreen_free(PrimeagenScreen *self);
TerminalCanvas PrimeagenScreen_canvas(PrimeagenScreen *self);
bool PrimeagenScreen_isBound(const PrimeagenScreen *self);

// --- Terminal events (zoom, fullscreen, window drag) ------------------------
// A terminal size change is its OWN event, never folded into "repaint when
// something else happens": a zoom (font size), a fullscreen toggle and a window
// drag all arrive as a resize, and the frame must be re-bound and re-validated —
// including the minimum-size notice — the moment it happens.
typedef enum PrimeagenScreenEvent {
    PRIMEAGEN_SCREEN_EVENT_NONE = 0,   // not a terminal event (an ordinary key)
    PRIMEAGEN_SCREEN_EVENT_RESIZE,     // zoom / fullscreen / drag: revalidate now
    PRIMEAGEN_SCREEN_EVENT_HOME,
    PRIMEAGEN_SCREEN_EVENT_CHAT,
    PRIMEAGEN_SCREEN_EVENT_FOCUS,
    PRIMEAGEN_SCREEN_EVENT_QUIT,
} PrimeagenScreenEvent;

// Classify one ncurses key. Pure: the host can decide what to do before acting.
PrimeagenScreenEvent PrimeagenScreen_classify(int key);

// Apply a size change: re-bind to the new extent and report whether the frame is
// still renderable (>= the advertised minimum). Always honoured — never deferred
// to a later "settle". The caller repaints afterwards.
bool PrimeagenScreen_revalidate(PrimeagenScreen *self, int rows, int columns);

// Resident state the screen reads (all borrowed).
typedef struct PrimeagenScreenState {
    Tui *tui;
    bool owo;
    bool focused;
    bool sessionMode;
    bool busy;
    bool failed;
    int panel;        // PRIMEAGEN_PANEL_*
    int selection;
    int listWidth;
    int focusPanel;   // which panel owns the keyboard: 0 rail, 1 centre, 2 people
} PrimeagenScreenState;

// HOME: `title` in the rail, `menu` rows below it, `badge` (nullable) filling
// the panel beside the rail. `menuSelection` highlights one row.
void PrimeagenScreen_renderHome(PrimeagenScreen *self, const char *title,
                                const char *const *menu, size_t menuCount, size_t menuSelection,
                                const TerminalImage *badge);

// CHAT: the three-column layout over the resident Tui state.
bool PrimeagenScreen_renderChat(PrimeagenScreen *self, const PrimeagenScreenState *state);

// The app's badge image (white paper + dark ink): the host decodes its own PNG
// and hands the pixels here; the screen inverts the paper and lets the caller's
// palette treat it. NULL/zero clears it.
void PrimeagenScreen_setBadge(const uint8_t *rgba, uint32_t width, uint32_t height);
const TerminalImage *PrimeagenScreen_badge(void);
bool PrimeagenScreen_hasBadge(void);

// Blit the grid to ncurses in ONE pass (wide cells + colour pairs).
bool PrimeagenScreen_present(const PrimeagenScreen *self, WINDOW *win);

#endif
