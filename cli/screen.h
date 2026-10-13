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
} PrimeagenScreenState;

// HOME: `title` in the rail, `menu` rows below it, `badge` (nullable) filling
// the panel beside the rail. `menuSelection` highlights one row.
void PrimeagenScreen_renderHome(PrimeagenScreen *self, const char *title,
                                const char *const *menu, size_t menuCount, size_t menuSelection,
                                const TerminalImage *badge);

// CHAT: the three-column layout over the resident Tui state.
bool PrimeagenScreen_renderChat(PrimeagenScreen *self, const PrimeagenScreenState *state);

// Blit the grid to ncurses in ONE pass (wide cells + colour pairs).
bool PrimeagenScreen_present(const PrimeagenScreen *self, WINDOW *win);

#endif
