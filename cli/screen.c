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
 * cell grid with the Determinant toolkit and handed to ncurses in one blit.
 * Two layouts mirror `_notes/_self/tui.txt` — HOME (title rail with the menu, the
 * app's picture in the panel beside it) and CHAT (left rail, centre chat, right
 * people). The frame draws its OWN border and corners, so it reads as a TUI
 * inside the window rather than borrowing the window's edges.
 *
 * FOCUS: exactly one panel owns the keyboard. Its selected row is HIGHLIGHTED
 * (reverse video); a panel that does not own the keyboard still marks its
 * selected row, but only with a COLOUR change — never a highlight. */
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
 *   putText / putCentered(canvas, ..., pen)   bounded draws
 *   lookAccent/lookMuted/lookText/lookRule/lookError(appearance)  named pens
 *   selectionPen(appearance, focused)  HIGHLIGHT when focused, COLOUR when not
 *   reshuffleBannerNoise()             fresh decor scatter, one per render
 *   renderTooSmall(root, look)         centred, word-wrapped resolution notice
 *   drawBorder(root, look)             the frame's own box (real corners)
 *   pairFor(rgb)                       packed colour -> ncurses pair (cached)
 *   navItems(panel, &count)            the rail's rows per panel
 * FUNCTION REGISTRY (exports):
 *   - PrimeagenScreen_init / _bind / _free / _isBound / _canvas
 *   - PrimeagenScreen_setBanner / _banner / _hasBanner
 *   - PrimeagenScreen_renderHome(self, title, menu, count, selection, banner)
 *   - PrimeagenScreen_renderChat(self, state)
 *   - PrimeagenScreen_present(self, win)
 * CONTRACT: null/too-small screens show the notice; the blit is one pass and
 * allocates nothing per cell. Cold owner thread only.
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

// The app's BANNER: borrowed pixels kept as a toolkit image. White paper is
// inverted to blank and the app's palette scatters the ink.
static TerminalImage s_banner;
static bool s_hasBanner = false;
static uint32_t s_noiseSeed = 1;
// The banner's "paper" is NOT blank: it is a gray field of random glyphs — the
// app's decor. Primeagen owns the alphabet and the gray; the toolkit ships no art.
#define PRIMEAGEN_SCREEN_NOISE_ALPHABET \
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%@#$&*+=/<>"
#define PRIMEAGEN_SCREEN_NOISE_COLOR TERMINAL_RGB(0x70, 0x70, 0x86)
// How much of the band the decor covers. The field fills the band edge to
// edge; this only decides how solid it looks.
#define PRIMEAGEN_SCREEN_NOISE_DENSITY 80

// Re-scatter the banner's paper texture. Called on EVERY render, so a revalidate (which
// re-renders) draws a fresh random field while a fixed seed stays reproducible.
static void reshuffleBannerNoise(void) {
    if (!s_hasBanner)
        return;
    s_noiseSeed = s_noiseSeed * 1103515245u + 12345u;
    TerminalImage_setNoiseSeed(&s_banner, s_noiseSeed);
}

// Which tone is the picture's "paper"? Sample it on a coarse grid: a BRIGHT
// average means a light background (so invert — the paper falls to blank and the
// ink becomes glyphs), a DARK average is already oriented that way. This is why
// one banner can be white-on-black and another black-on-white with no per-image
// setting: the paper is whatever covers the frame, and it becomes the decor.
static bool paperIsBright(const uint8_t *rgba, uint32_t width, uint32_t height) {
    TerminalImage probe = TerminalImage((uint8_t*) (uintptr_t) rgba, (size_t) width, (size_t) height);
    uint64_t sum = 0;
    size_t count = 0;
    for (size_t ly = 0; ly < 16; ++ly) {
        for (size_t lx = 0; lx < 16; ++lx) {
            TerminalImageSample sample = TerminalImage_sampleCell(&probe, lx, ly, 16, 16);
            if (!sample.valid || sample.alpha == 0)
                continue;
            sum += TerminalImage_luminance(sample.red, sample.green, sample.blue);
            ++count;
        }
    }
    return count == 0 ? true : (sum / count) > 128;
}

void PrimeagenScreen_setBanner(const uint8_t *rgba, uint32_t width, uint32_t height) {
    if (rgba == nullptr || width == 0 || height == 0) {
        s_hasBanner = false;
        s_banner = TerminalImage_0();
        return;
    }
    s_banner = TerminalImage((uint8_t*) (uintptr_t) rgba, (size_t) width, (size_t) height);
    TerminalImage_setInvert(&s_banner, paperIsBright(rgba, width, height));
    TerminalImageColorRamp ramp = TerminalImage_rampPurple();
    (void) TerminalImage_setColorRamp(&s_banner, ramp.data, ramp.count);
    // Which letter reads light and which reads dark is randomized per cell, not
    // luminance-ordered — the palette's whole range (≈30% up to near-white).
    TerminalImage_setPaletteScatter(&s_banner, (uint8_t) ramp.count);
    TerminalImage_setJitter(&s_banner, 20, 11);
    TerminalImage_setStyle(&s_banner, 0);
    (void) TerminalImage_setNoise(&s_banner, PRIMEAGEN_SCREEN_NOISE_ALPHABET, PRIMEAGEN_SCREEN_NOISE_COLOR);
    TerminalImage_setNoiseStyle(&s_banner, TERMINAL_STYLE_DIM);
    TerminalImage_setNoiseDensity(&s_banner, PRIMEAGEN_SCREEN_NOISE_DENSITY);
    s_hasBanner = true;
}

const TerminalImage *PrimeagenScreen_banner(void) { return s_hasBanner ? &s_banner : nullptr; }
bool PrimeagenScreen_hasBanner(void) { return s_hasBanner; }

TerminalCanvas PrimeagenScreen_canvas(PrimeagenScreen *self) {
    if (!PrimeagenScreen_isBound(self))
        return TerminalCanvas_0();
    // All FOUR planes: the colour planes are what make a "colour change" (an
    // unfocused panel's selection) distinguishable from a highlight.
    return TerminalCanvas_6((*self).cells, (*self).styles, (*self).fg, (*self).bg,
                            (*self).columns, (*self).rows);
}

PrimeagenScreenEvent PrimeagenScreen_classify(int key) {
    switch (key) {
        case KEY_RESIZE: return PRIMEAGEN_SCREEN_EVENT_RESIZE;
        case '1':        return PRIMEAGEN_SCREEN_EVENT_HOME;
        case '2':        return PRIMEAGEN_SCREEN_EVENT_CHAT;
        case 'f':        return PRIMEAGEN_SCREEN_EVENT_FOCUS;
        case 'q':
        case 'Q':        return PRIMEAGEN_SCREEN_EVENT_QUIT;
        default:         return PRIMEAGEN_SCREEN_EVENT_NONE;
    }
}

bool PrimeagenScreen_revalidate(PrimeagenScreen *self, int rows, int columns) {
    if (rows <= 0 || columns <= 0)
        return false;
    if (!PrimeagenScreen_bind(self, (size_t) rows, (size_t) columns))
        return false;
    return (size_t) rows >= PRIMEAGEN_SCREEN_MIN_ROWS
        && (size_t) columns >= PRIMEAGEN_SCREEN_MIN_COLUMNS;
}

// --- pens -------------------------------------------------------------------
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
// The focused panel HIGHLIGHTS its selection; an unfocused panel only recolours it.
static TerminalPen selectionPen(const TerminalAppearance *look, bool focused) {
    if (focused)
        return TerminalPen_2(TERMINAL_STYLE_REVERSE | TERMINAL_STYLE_ACCENT, (*look).colorAccent);
    return TerminalPen_2(TERMINAL_STYLE_ACCENT, (*look).colorAccent);
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

// Too small to resolve: one centred notice, re-centred every render so it stays
// anchored while the window is dragged. Word-wrapped, never split mid-word.
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
        if (end < length && lastSpace > start) {
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

// The frame's OWN box, with real corners, so the picture reads as a terminal
// instrument set inside the window rather than content pressed against it.
static void drawBorder(TerminalCanvas *root, const TerminalAppearance *look) {
    TerminalRule_drawBox(&(*look).rule, root, lookRule(look));
}

void PrimeagenScreen_renderHome(PrimeagenScreen *self, const char *title,
                                const char *const *menu, size_t menuCount, size_t menuSelection,
                                const TerminalImage *banner) {
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
    drawBorder(&root, look);
    TerminalCanvas inner = TerminalCanvas_sub(&root, 1, 1,
                                              TerminalCanvas_getWidth(&root) - 2,
                                              TerminalCanvas_getHeight(&root) - 2);
    if (!TerminalCanvas_isValid(&inner))
        return;

    // The tui.txt basics: a title rail with the menu stacked down it, the app's
    // picture filling the panel beside it.
    size_t columns = TerminalCanvas_getWidth(&inner);
    size_t rows = TerminalCanvas_getHeight(&inner);
    size_t left = PRIMEAGEN_SCREEN_LEFT_DEFAULT < columns / 3 ? PRIMEAGEN_SCREEN_LEFT_DEFAULT : columns / 3;
    TerminalCanvas rail = TerminalCanvas_sub(&inner, 0, 0, left, rows);
    TerminalCanvas panel = TerminalCanvas_sub(&inner, left + 1, 0, columns - left - 1, rows);
    putCentered(&rail, 0, title, lookAccent(look));
    TerminalRule_drawCut(&(*look).rule, &rail, 1, 1, lookRule(look));
    size_t row = 3;
    for (size_t i = 0; i < menuCount && row + 2 < TerminalCanvas_getHeight(&rail); ++i, row += 2) {
        TerminalPen pen = (i == menuSelection) ? selectionPen(look, true) : lookText(look);
        putCentered(&rail, row, menu[i], pen);
        TerminalRule_drawCut(&(*look).rule, &rail, row + 1, 1, lookRule(look));
    }
    TerminalCanvas_vline(&inner, left, '|', lookRule(look));

    // Zoom-fit into the panel: one uniform scale (cell-aspect corrected), centred,
    // so the picture is never squashed. The decor is re-scattered first, so every
    // revalidate draws a fresh random field behind it.
    if (banner != nullptr && TerminalImage_isValid(banner)) {
        reshuffleBannerNoise();
        TerminalImage_renderContain(banner, &panel);
    }
}

// The rail's rows per panel. Static sample content; the app owns real data.
static void navItems(int panel, const char *const **outItems, size_t *outCount) {
    static const char *const people[] = {"Sola (@solasol)", "Muse Writer (@muse...)", "Vex (@vexgraph)"};
    static const char *const servers[] = {"Ecosystem", "Other Server", "Another Server"};
    static const char *const channels[] = {"# general", "# whatever-this", "# hotcwaphsdf"};
    switch (panel) {
        case 1: *outItems = servers; *outCount = 3; break;
        case 2: *outItems = channels; *outCount = 3; break;
        default: *outItems = people; *outCount = 3; break;
    }
}

bool PrimeagenScreen_renderChat(PrimeagenScreen *self, const PrimeagenScreenState *state) {
    if (!PrimeagenScreen_isBound(self) || state == nullptr || (*state).tui == nullptr)
        return false;
    TerminalCanvas root = PrimeagenScreen_canvas(self);
    TerminalCanvas_fill(&root, ' ', TerminalPen_1(TERMINAL_STYLE_NONE));
    const TerminalAppearance *look = PrimeagenAppearance_current();
    if ((*self).columns < PRIMEAGEN_SCREEN_MIN_COLUMNS || (*self).rows < PRIMEAGEN_SCREEN_MIN_ROWS) {
        renderTooSmall(&root, look);
        return true;
    }
    drawBorder(&root, look);
    TerminalCanvas inner = TerminalCanvas_sub(&root, 1, 1, (*self).columns - 2, (*self).rows - 2);
    if (!TerminalCanvas_isValid(&inner))
        return false;

    size_t columns = TerminalCanvas_getWidth(&inner);
    size_t rows = TerminalCanvas_getHeight(&inner);
    size_t left = PRIMEAGEN_SCREEN_LEFT_DEFAULT < columns / 3 ? PRIMEAGEN_SCREEN_LEFT_DEFAULT : columns / 3;
    size_t right = PRIMEAGEN_SCREEN_RIGHT_DEFAULT < columns / 4 ? PRIMEAGEN_SCREEN_RIGHT_DEFAULT : columns / 4;
    if (left + right + 4 >= columns)
        right = 0;

    // Left rail: the focused list. Selection highlights only when it owns focus.
    TerminalCanvas rail = TerminalCanvas_sub(&inner, 0, 0, left, rows);
    putCentered(&rail, 0, "Primeagen", lookAccent(look));
    TerminalRule_drawCut(&(*look).rule, &rail, 1, 1, lookRule(look));
    putText(&rail, 0, 2, "[<-] Back", lookMuted(look));
    const char *const *items = nullptr;
    size_t itemCount = 0;
    navItems((*state).panel, &items, &itemCount);
    for (size_t i = 0; i < itemCount && i + 4 < TerminalCanvas_getHeight(&rail); ++i) {
        bool selected = (int) i == (*state).selection;
        TerminalPen pen = selected ? selectionPen(look, (*state).focusPanel == 0) : lookText(look);
        if (selected)
            putText(&rail, 0, i + 4, (*state).focusPanel == 0 ? "> " : "* ", pen);
        putText(&rail, 2, i + 4, items[i], pen);
    }
    if (TerminalCanvas_getHeight(&rail) > 0)
        putText(&rail, 0, TerminalCanvas_getHeight(&rail) - 1, "[+] Add", lookMuted(look));
    TerminalCanvas_vline(&inner, left, '|', lookRule(look));

    // Centre: channel title, transcript, palette, input, status.
    size_t centreWidth = right > 0 ? columns - left - right - 2 : columns - left - 1;
    TerminalCanvas centre = TerminalCanvas_sub(&inner, left + 1, 0, centreWidth, rows);
    putText(&centre, 1, 0, "Ecosystem > #general", lookText(look));
    TerminalRule_drawCut(&(*look).rule, &centre, 1, 1, lookRule(look));

    size_t statusRow = rows > 0 ? rows - 1 : 0;
    size_t inputRow = rows > 1 ? rows - 2 : 0;
    size_t sepInput = inputRow > 0 ? inputRow - 1 : 0;
    size_t chatHeight = sepInput > 2 ? sepInput - 2 : 0;
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
    TerminalRule_drawCut(&(*look).rule, &centre, sepInput, 1, lookRule(look));
    putText(&centre, 0, inputRow, "> ", lookAccent(look));
    putText(&centre, 2, inputRow, Tui_getInput((*state).tui), lookText(look));
    const char *status = (*state).failed ? "session closed | Ctrl-Q and relaunch"
                       : (*state).busy ? "working | Ctrl-Q cancels locally"
                       : (*state).sessionMode ? "OpenCode chat | inspect auto | edit/shell ask"
                       : "disconnected | /connect uses your login";
    putText(&centre, 0, statusRow, status, lookMuted(look));

    // Right rail: the people list — same focus rule as the left rail.
    if (right > 0) {
        TerminalCanvas people = TerminalCanvas_sub(&inner, columns - right, 0, right, rows);
        TerminalCanvas_vline(&inner, columns - right - 1, '|', lookRule(look));
        putCentered(&people, 0, "Users", lookText(look));
        TerminalRule_drawCut(&(*look).rule, &people, 1, 1, lookRule(look));
        for (size_t i = 0; i < itemCount && i + 2 < TerminalCanvas_getHeight(&people); ++i) {
            bool selected = (int) i == (*state).selection;
            TerminalPen pen = selected ? selectionPen(look, (*state).focusPanel == 2) : lookMuted(look);
            putText(&people, 1, i + 2, items[i], pen);
        }
    }
    return true;
}

// Map a packed 0xRRGGBBAA foreground onto a ncurses colour pair, quantised to
// the terminal's palette and cached. TERMINAL_COLOR_DEFAULT keeps pair 0 (the
// host default). This is what makes a colour change distinguishable from
// reverse-video highlighting.
static short pairFor(uint32_t rgb) {
    static uint32_t keys[512];
    static short pairs[512];
    static int count = 0;
    if (rgb == TERMINAL_COLOR_DEFAULT || !has_colors())
        return 0;
    for (int i = 0; i < count; ++i)
        if (keys[i] == rgb)
            return pairs[i];
    if (count >= 511 || COLORS < 8)
        return 0;
    int colour;
    if (COLORS >= 256) {
        int r = TERMINAL_COLOR_RED(rgb) * 5 / 255;
        int g = TERMINAL_COLOR_GREEN(rgb) * 5 / 255;
        int b = TERMINAL_COLOR_BLUE(rgb) * 5 / 255;
        colour = 16 + 36 * r + 6 * g + b;
    } else {
        colour = (TERMINAL_COLOR_RED(rgb) > 127 ? 1 : 0)
               | (TERMINAL_COLOR_GREEN(rgb) > 127 ? 2 : 0)
               | (TERMINAL_COLOR_BLUE(rgb) > 127 ? 4 : 0);
    }
    short pair = (short) (count + 1);
    if (init_pair(pair, (short) colour, -1) != OK)
        return 0;
    keys[count] = rgb;
    pairs[count] = pair;
    ++count;
    return pair;
}

bool PrimeagenScreen_present(const PrimeagenScreen *self, WINDOW *win) {
    if (!PrimeagenScreen_isBound(self) || win == nullptr)
        return false;
    for (size_t y = 0; y < (*self).rows; ++y) {
        for (size_t x = 0; x < (*self).columns; ++x) {
            size_t index = y * (*self).columns + x;
            uint32_t cp = (*self).cells[index];
            if (cp == 0)
                continue;
            uint8_t style = (*self).styles[index];
            attr_t attr = A_NORMAL;
            if (style & TERMINAL_STYLE_DIM) attr |= A_DIM;
            if (style & TERMINAL_STYLE_BOLD) attr |= A_BOLD;
            if (style & TERMINAL_STYLE_REVERSE) attr |= A_REVERSE;
            if (style & TERMINAL_STYLE_MUTED) attr |= A_DIM;
            if (style & TERMINAL_STYLE_ACCENT) attr |= A_BOLD;
            short pair = pairFor((*self).fg[index]);
            wchar_t text[2] = { (wchar_t) cp, L'\0' };
            cchar_t glyph;
            if (setcchar(&glyph, text, attr, pair, nullptr) != OK)
                continue;
            (void) mvadd_wch((int) y, (int) x, &glyph);
        }
    }
    return refresh() != ERR;
}
