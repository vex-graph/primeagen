#define _XOPEN_SOURCE_EXTENDED 1
#include "cli/screen.h"
#include "cli/appearance.h"

#include <stdlib.h>
#include <string.h>

#include "terminal/terminal_style.h"
#include "terminal/terminal_color.h"
#include "terminal/terminal_frame.h"
#include "terminal/terminal_list_view.h"
#include "terminal/terminal_text_view.h"
#include "terminal/terminal_input_line.h"
#include "terminal/terminal_status_bar.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/** Primeagen's frame as a RENDERER: the whole screen is composed into an owned
 * cell grid with the Determinant toolkit and handed to ncurses in one blit. Two
 * layouts mirror `_notes/_self/tui.txt` — HOME (title + People/Servers/Settings
 * menu beside the app's badge image) and CHAT (left rail, centre chat, right
 * users). The screen owns its four planes; every string, colour and glyph comes
 * from the caller, so the toolkit stays bare and the look stays the app's. */
;;OVERVIEW
/** ========================================================================
 * CLASS: PrimeagenScreen (cli/screen.c)
 * ============================================================================
 * STRUCT FIELDS (Mirroring cli/screen.h):
 *   uint32_t *cells       codepoint plane
 *   uint8_t *styles       attribute plane
 *   uint32_t *fg, *bg     0xRRGGBBAA planes
 *   size_t rows, columns, capacity
 * PRIVATE HELPERS:
 *   screenGrow(self, rows, columns)   (re)allocate the planes
 *   putText(canvas, lx, ly, s, pen)   bounded draw
 *   putCentered(canvas, ly, s, pen)   centre in the region
 *   blobPen(appearance, style)        colour a rule from the appearance
 * FUNCTION REGISTRY (exports):
 *   - PrimeagenScreen_init / _bind / _free / _isBound / _canvas
 *   - PrimeagenScreen_renderHome(self, title, menu, count, selection, badge)
 *   - PrimeagenScreen_renderChat(self, state)
 *   - PrimeagenScreen_present(self, win)
 * CONTRACT: null/too-small screens no-op (the caller keeps its own notice); the
 * blit is one pass and never allocates per cell. Cold owner thread only.
 * ============================================================================
 */

void PrimeagenScreen_init(PrimeagenScreen *self) {
    if (self != nullptr)
        memset(self, 0, sizeof(*self));
}

bool PrimeagenScreen_isBound(const PrimeagenScreen *self) {
    return self != nullptr && (*self).cells != nullptr && (*self).rows > 0 && (*self).columns > 0;
}

bool PrimeagenScreen_bind(PrimeagenScreen *self, size_t rows, size_t columns) {
    if (self == nullptr || rows == 0 || columns == 0 || rows > SIZE_MAX / columns)
        return false;
    size_t want = rows * columns;
    if (want > (*self).capacity) {
        uint32_t *cells = (uint32_t*) realloc((*self).cells, want * sizeof(uint32_t));
        if (cells == nullptr) return false;
        (*self).cells = cells;
        uint8_t *styles = (uint8_t*) realloc((*self).styles, want);
        if (styles == nullptr) return false;
        (*self).styles = styles;
        uint32_t *fg = (uint32_t*) realloc((*self).fg, want * sizeof(uint32_t));
        if (fg == nullptr) return false;
        (*self).fg = fg;
        uint32_t *bg = (uint32_t*) realloc((*self).bg, want * sizeof(uint32_t));
        if (bg == nullptr) return false;
        (*self).bg = bg;
        (*self).capacity = want;
    }
    (*self).rows = rows;
    (*self).columns = columns;
    return true;
}

void PrimeagenScreen_free(PrimeagenScreen *self) {
    if (self == nullptr)
        return;
    free((*self).cells);
    free((*self).styles);
    free((*self).fg);
    free((*self).bg);
    memset(self, 0, sizeof(*self));
}

// The app's badge: borrowed pixels kept as a toolkit image. White paper is
// inverted to blank and the app's palette scatters the ink (see the caller).
static TerminalImage s_badge;
static bool s_hasBadge = false;

void PrimeagenScreen_setBadge(const uint8_t *rgba, uint32_t width, uint32_t height) {
    if (rgba == nullptr || width == 0 || height == 0) {
        s_hasBadge = false;
        s_badge = TerminalImage_0();
        return;
    }
    s_badge = TerminalImage((uint8_t*) (uintptr_t) rgba, (size_t) width, (size_t) height);
    TerminalImage_setInvert(&s_badge, true);          // white paper -> blank cell
    size_t rampCount = 0;
    const uint32_t *ramp = TerminalImage_rampPurple(&rampCount);
    (void) TerminalImage_setColorRamp(&s_badge, ramp, rampCount);
    TerminalImage_setJitter(&s_badge, 30, 11);
    TerminalImage_setStyle(&s_badge, 0);
    s_hasBadge = true;
}

const TerminalImage *PrimeagenScreen_badge(void) { return s_hasBadge ? &s_badge : nullptr; }
bool PrimeagenScreen_hasBadge(void) { return s_hasBadge; }

TerminalCanvas PrimeagenScreen_canvas(PrimeagenScreen *self) {
    if (!PrimeagenScreen_isBound(self))
        return TerminalCanvas_0();
    return TerminalCanvas_4((*self).cells, (*self).styles, (*self).columns, (*self).rows);
}

static void putText(TerminalCanvas *canvas, size_t lx, size_t ly, const char *text, TerminalPen pen) {
    if (text != nullptr)
        TerminalCanvas_text(canvas, lx, ly, text, strlen(text), pen);
}

static void putCentered(TerminalCanvas *canvas, size_t ly, const char *text, TerminalPen pen) {
    if (text == nullptr)
        return;
    size_t width = TerminalCanvas_getWidth(canvas);
    size_t length = strlen(text);
    size_t lx = length < width ? (width - length) / 2 : 0;
    TerminalCanvas_text(canvas, lx, ly, text, length, pen);
}

static TerminalPen lookAccent(const TerminalAppearance *look) {
    return TerminalPen_2(TERMINAL_STYLE_ACCENT | TERMINAL_STYLE_BOLD, (*look).colorAccent);
}
static TerminalPen lookMuted(const TerminalAppearance *look) {
    return TerminalPen_2(TERMINAL_STYLE_MUTED, (*look).colorMuted);
}
static TerminalPen lookText(const TerminalAppearance *look) {
    return TerminalPen_2(TERMINAL_STYLE_NONE, (*look).colorText);
}
static TerminalPen lookRule(const TerminalAppearance *look) {
    return TerminalPen_2(TERMINAL_STYLE_DIM, (*look).colorMuted);
}
static TerminalPen lookError(const TerminalAppearance *look) {
    return TerminalPen_2(TERMINAL_STYLE_ERROR | TERMINAL_STYLE_BOLD, (*look).colorError);
}

// Too small to resolve: one centred notice, re-centred every render (so it stays
// anchored in the middle while the window is dragged). Word-wrapped, never split
// mid-word.
static void renderTooSmall(TerminalCanvas *root, const TerminalAppearance *look) {
    TerminalCanvas_fill(root, ' ', TerminalPen_1(TERMINAL_STYLE_NONE));
    size_t width = TerminalCanvas_getWidth(root);
    size_t height = TerminalCanvas_getHeight(root);
    if (width == 0 || height == 0)
        return;
    const char *text = PRIMEAGEN_SCREEN_TOO_SMALL;
    size_t length = strlen(text);
    enum { MAX_LINES = 8 };
    size_t lineStart[MAX_LINES];
    size_t lineLength[MAX_LINES];
    size_t lines = 0;
    size_t at = 0;
    while (at < length && lines < MAX_LINES) {
        size_t start = at;
        size_t lastSpace = 0;
        size_t used = 0;
        while (at < length && used < width) {
            if (text[at] == ' ')
                lastSpace = at;
            ++at;
            ++used;
        }
        size_t end = at;
        if (end < length && lastSpace > start) {   // break at a word boundary
            end = lastSpace;
            at = lastSpace + 1;
        }
        lineStart[lines] = start;
        lineLength[lines] = end - start;
        ++lines;
    }
    size_t first = lines < height ? (height - lines) / 2 : 0;
    for (size_t i = 0; i < lines && first + i < height; ++i) {
        size_t span = lineLength[i];
        size_t lx = span < width ? (width - span) / 2 : 0;
        TerminalCanvas_text(root, lx, first + i, text + lineStart[i], span, lookError(look));
    }
}

void PrimeagenScreen_renderHome(PrimeagenScreen *self, const char *title,
                                const char *const *menu, size_t menuCount, size_t menuSelection,
                                const TerminalImage *badge) {
    if (!PrimeagenScreen_isBound(self))
        return;
    TerminalCanvas root = PrimeagenScreen_canvas(self);
    TerminalCanvas_fill(&root, ' ', TerminalPen_1(TERMINAL_STYLE_NONE));
    const TerminalAppearance *look = PrimeagenAppearance_current();
    if (TerminalCanvas_getWidth(&root) < PRIMEAGEN_SCREEN_MIN_COLUMNS
        || TerminalCanvas_getHeight(&root) < PRIMEAGEN_SCREEN_MIN_ROWS) {
        renderTooSmall(&root, look);
        return;
    }

    size_t columns = (*self).columns;
    size_t left = PRIMEAGEN_SCREEN_LEFT_DEFAULT < columns / 3 ? PRIMEAGEN_SCREEN_LEFT_DEFAULT : columns / 3;
    TerminalCanvas rail = TerminalCanvas_sub(&root, 0, 0, left, (*self).rows);
    TerminalCanvas panel = TerminalCanvas_sub(&root, left + 1, 0, columns - left - 1, (*self).rows);

    // Left rail: the app name, a rule, then the menu.
    putCentered(&rail, 0, title, lookAccent(look));
    TerminalAppearance_drawRule(look, &rail, 1, lookRule(look));
    size_t row = 3;
    for (size_t i = 0; i < menuCount && row + 2 < TerminalCanvas_getHeight(&rail); ++i, row += 2) {
        TerminalPen pen = (i == menuSelection)
            ? TerminalPen_2(TERMINAL_STYLE_REVERSE, (*look).colorAccent)
            : lookText(look);
        putCentered(&rail, row, menu[i], pen);
        TerminalAppearance_drawRule(look, &rail, row + 1, lookRule(look));
    }
    // Column separator between the rail and the panel.
    TerminalCanvas_vline(&root, left, '|', lookRule(look));

    // The badge is ZOOM-FIT, never stretched: one uniform scale keeps its aspect
    // and orientation, centred in the panel beside the rail.
    if (badge != nullptr && TerminalImage_isValid(badge))
        TerminalImage_renderContain(badge, &panel);
}

bool PrimeagenScreen_renderChat(PrimeagenScreen *self, const PrimeagenScreenState *state) {
    if (!PrimeagenScreen_isBound(self) || state == nullptr || (*state).tui == nullptr)
        return false;
    TerminalCanvas root = PrimeagenScreen_canvas(self);
    TerminalCanvas_fill(&root, ' ', TerminalPen_1(TERMINAL_STYLE_NONE));
    size_t columns = (*self).columns;
    size_t rows = (*self).rows;
    const TerminalAppearance *look = PrimeagenAppearance_current();
    if (columns < PRIMEAGEN_SCREEN_MIN_COLUMNS || rows < PRIMEAGEN_SCREEN_MIN_ROWS) {
        renderTooSmall(&root, look);
        return true;
    }

    size_t left = PRIMEAGEN_SCREEN_LEFT_DEFAULT < columns / 3 ? PRIMEAGEN_SCREEN_LEFT_DEFAULT : columns / 3;
    size_t right = PRIMEAGEN_SCREEN_RIGHT_DEFAULT < columns / 4 ? PRIMEAGEN_SCREEN_RIGHT_DEFAULT : columns / 4;
    if (left + right + 4 >= columns)
        right = 0;

    TerminalCanvas rail = TerminalCanvas_sub(&root, 0, 0, left, rows);
    TerminalCanvas centre = TerminalCanvas_sub(&root, left + 1, 0, columns - left - (right ? right + 1 : 0), rows);
    putCentered(&rail, 0, "Primeagen", lookAccent(look));
    TerminalAppearance_drawRule(look, &rail, 1, lookRule(look));
    putText(&rail, 1, 2, "[<-] Back", lookMuted(look));
    TerminalCanvas_vline(&root, left, '|', lookRule(look));

    // Centre column: channel title, transcript, palette, input, status.
    size_t centreWidth = TerminalCanvas_getWidth(&centre);
    size_t centreHeight = TerminalCanvas_getHeight(&centre);
    putText(&centre, 1, 0, (*look).colorAccent ? "Ecosystem > #general" : "", TerminalPen_1(TERMINAL_STYLE_NONE));
    TerminalAppearance_drawRule(look, &centre, 1, lookRule(look));

    size_t statusRow = centreHeight - 1;
    size_t inputRow = centreHeight - 2;
    size_t sepInput = inputRow > 0 ? inputRow - 1 : 0;
    size_t chatHeight = sepInput > 1 ? sepInput - 1 : 0;

    TerminalCanvas chat = TerminalCanvas_sub(&centre, 0, 2, centreWidth, chatHeight);
    if (TerminalCanvas_isValid(&chat)) {
        size_t first = Tui_getFirstVisibleRow((*state).tui);
        size_t total = Tui_getVisualRowCount((*state).tui);
        size_t height = TerminalCanvas_getHeight(&chat);
        char line[512];
        uint8_t styles[512];
        for (size_t i = 0; i < height && first + i < total; ++i) {
            if (!Tui_copyStyledRow((*state).tui, first + i, line, styles, sizeof line, nullptr))
                break;
            TerminalPen pen = lookText(look);
            if (styles[0] & TEXT_STYLE_USER) pen.style |= TERMINAL_STYLE_BOLD;
            if (styles[0] & TEXT_STYLE_ASSISTANT) pen.style |= TERMINAL_STYLE_ACCENT;
            if (styles[0] & TEXT_STYLE_ERROR) pen.style |= TERMINAL_STYLE_ERROR;
            putText(&chat, 0, i, line, pen);
        }
    }

    TerminalAppearance_drawRule(look, &centre, sepInput, lookRule(look));
    putText(&centre, 0, inputRow, "> ", lookAccent(look));
    putText(&centre, 2, inputRow, Tui_getInput((*state).tui), lookText(look));
    const char *status = (*state).failed ? "session closed | Ctrl-Q and relaunch"
                       : (*state).busy ? "working | Ctrl-Q cancels locally"
                       : (*state).sessionMode ? "OpenCode chat | inspect auto | edit/shell ask"
                       : "disconnected | /connect uses your login";
    putText(&centre, 0, statusRow, status, lookMuted(look));

    // Right rail: the people list.
    if (right > 0) {
        TerminalCanvas people = TerminalCanvas_sub(&root, columns - right, 0, right, rows);
        TerminalCanvas_vline(&root, columns - right - 1, '|', lookRule(look));
        putCentered(&people, 0, "Users", lookText(look));
        TerminalAppearance_drawRule(look, &people, 1, lookRule(look));
        static const char *const names[] = {"Sola (@solasol)", "Muse Writer (@muse...)", "Vex (@vexgraph)"};
        for (size_t i = 0; i < 3 && i + 2 < TerminalCanvas_getHeight(&people); ++i)
            putText(&people, 1, i + 2, names[i], lookMuted(look));
    }
    return true;
}

bool PrimeagenScreen_present(const PrimeagenScreen *self, WINDOW *win) {
    if (!PrimeagenScreen_isBound(self) || win == nullptr)
        return false;
    for (size_t y = 0; y < (*self).rows; ++y) {
        for (size_t x = 0; x < (*self).columns; ++x) {
            size_t index = y * (*self).columns + x;
            uint32_t cp = (*self).cells[index];
            if (cp == 0)
                continue;   // wide-glyph continuation: the wide cell already spans it
            uint8_t style = (*self).styles[index];
            attr_t attr = A_NORMAL;
            if (style & TERMINAL_STYLE_DIM) attr |= A_DIM;
            if (style & TERMINAL_STYLE_BOLD) attr |= A_BOLD;
            if (style & TERMINAL_STYLE_REVERSE) attr |= A_REVERSE;
            if (style & (TERMINAL_STYLE_ACCENT | TERMINAL_STYLE_MUTED)) attr |= A_DIM;
            wchar_t text[2] = { (wchar_t) cp, L'\0' };
            cchar_t glyph;
            if (setcchar(&glyph, text, attr, 0, nullptr) != OK)
                continue;
            (void) mvadd_wch((int) y, (int) x, &glyph);
        }
    }
    return refresh() != ERR;
}
