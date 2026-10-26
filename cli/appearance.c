#define _XOPEN_SOURCE_EXTENDED 1
#include "cli/appearance.h"

#include <stdlib.h>
#include <string.h>

#include "ui/surface/terminal_canvas.h"
#include "format/terminal_style.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/** The app's chosen look and its rule drawing. The choice is a preset NAME
 * (persisted in Settings, applied by the host through this module); the glyphs
 * and the geometry belong to the toolkit. The module keeps one resolved
 * TerminalAppearance in static state so it lives with whichever view copy is
 * active — the host resolves Primeagen_setAppearance next to the draw pointers,
 * so a reload adopts the same look without touching the terminal. */
;;OVERVIEW
/** ========================================================================
 * MODULE: PrimeagenAppearance (cli/appearance.c)
 * ============================================================================
 * STATE (file-local):
 *   static TerminalAppearance s_current   resolved preset (lazy default)
 *   static bool s_loaded                  false until first use
 *   static char s_name[]                  the chosen preset name
 *   static uint32_t *s_cells; size_t s_cap  rule scratch (cold growth)
 * FUNCTION REGISTRY:
 *   - void PrimeagenAppearance_use(const char *presetName)
 *   - const TerminalAppearance *PrimeagenAppearance_current(void)
 *   - const char *PrimeagenAppearance_name(void)
 *   - bool PrimeagenAppearance_drawRule(WINDOW *win, int row, int left, int width)
 * CONTRACT: cold owner thread; unknown/null names adopt the toolkit default; the
 * rule blit is one pass and clips to `width`; no file, network or model access.
 * ============================================================================
 */

#define PRIMEAGEN_APPEARANCE_NAME_MAX 32

static TerminalAppearance s_current;
static bool s_loaded = false;
static char s_name[PRIMEAGEN_APPEARANCE_NAME_MAX] = "ascii-striped";
static uint32_t *s_cells = nullptr;
static size_t s_cap = 0;
static uint16_t *s_styles = nullptr;

const TerminalAppearance *PrimeagenAppearance_current(void) {
    if (!s_loaded) {
        s_current = TerminalAppearance_0();
        s_loaded = true;
    }
    return &s_current;
}

const char *PrimeagenAppearance_name(void) { return s_name; }

void PrimeagenAppearance_use(const char *presetName) {
    const char *wanted = (presetName != nullptr && presetName[0] != '\0') ? presetName : "ascii-striped";
    s_current = TerminalAppearance_named(wanted);
    size_t n = strlen(wanted);
    if (n >= sizeof s_name)
        n = sizeof s_name - 1;
    memcpy(s_name, wanted, n);
    s_name[n] = '\0';
    s_loaded = true;
}

bool PrimeagenAppearance_drawRule(WINDOW *win, int row, int left, int width) {
    if (win == nullptr || width <= 0 || row < 0 || left < 0)
        return false;
    if ((size_t) width > s_cap) {
        uint32_t *cells = (uint32_t*) realloc(s_cells, (size_t) width * sizeof(uint32_t));
        if (cells == nullptr)
            return false;
        s_cells = cells;
        uint16_t *styles = (uint16_t*) realloc(s_styles, (size_t) width * sizeof(uint16_t));
        if (styles == nullptr)
            return false;
        s_styles = styles;
        s_cap = (size_t) width;
    }
    TerminalCanvas canvas = TerminalCanvas(s_cells, s_styles, (size_t) width, 1);
    if (!TerminalCanvas_isValid(&canvas))
        return false;
    const TerminalAppearance *look = PrimeagenAppearance_current();
    TerminalAppearance_drawRule(look, &canvas, 0, TerminalPen_1(TERMINAL_STYLE_DIM));

    // One blit: wide-cell aware, dimmed, so a box-drawing or ASCII glyph both land.
    for (int x = 0; x < width; ++x) {
        uint32_t cp = TerminalCanvas_glyphAt(&canvas, (size_t) x, 0);
        if (cp == 0)
            continue;   // wide-glyph continuation cell: the wide cell already spans it
        wchar_t text[2] = { (wchar_t) cp, L'\0' };
        cchar_t glyph;
        if (setcchar(&glyph, text, A_DIM, 0, nullptr) == OK)
            (void) mvadd_wch(row, left + x, &glyph);
        else
            (void) mvaddch(row, left + x, (chtype) ' ' | A_DIM);
    }
    return true;
}
