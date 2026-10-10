#include "tui/event/mouse_event.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/* Backend-independent cold hit testing translates terminal cell coordinates into
 * interface actions. The frontend owns focus/quit; this module only changes the
 * transcript viewport for wheel input. It owns no mouse device or allocation.
 * Terminal geometry is borrowed from a live owner-thread Tui. Rendering and hit
 * regions share the same fixed chrome layout; resize therefore changes both.
 */
;;OVERVIEW
/* ============================================================================
 * MODULE: Tui mouse dispatch (tui/event/mouse_event.c)
 * ============================================================================
 * OWNED STATE: none. Borrows live Tui for the duration of one event.
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public dispatch:
 *   TuiMouseAction TuiMouse_dispatch(Tui *self, int x, int y, TuiMouseKind kind)
 *     Wheel +/- one row inside transcript; click Clear/Quit header or input.
 * Contracts:
 *   Null, negative, outside, unsupported kind or undersized screen => NONE.
 *   Clear/Quit are returned actions, not executed here. No callback, logging,
 *   allocation, retained pointer, provider operation or permission change.
 *   One normalized click produces one action; backend excludes release events.
 * ============================================================================
 */
TuiMouseAction TuiMouse_dispatch(Tui *self, int x, int y, TuiMouseKind kind) {
    int rows, columns;
    Tui_getSize(self, &rows, &columns);
    if (rows < TUI_MIN_ROWS || columns < TUI_MIN_COLUMNS || x < 0 || y < 0 || x >= columns || y >= rows)
        return TUI_MOUSE_NONE;
    if (kind == TUI_MOUSE_WHEEL_UP || kind == TUI_MOUSE_WHEEL_DOWN) {
        if (y >= 2 && y < rows - 4 && x > 0 && x < columns - 1)
            Tui_scrollBy(self, kind == TUI_MOUSE_WHEEL_UP ? 1 : -1);
        return TUI_MOUSE_NONE;
    }
    if (kind != TUI_MOUSE_CLICK)
        return TUI_MOUSE_NONE;
    if (y == 0 && x >= columns - 7)
        return TUI_MOUSE_QUIT;
    if (y == 0 && x >= columns - 15 && x < columns - 8)
        return TUI_MOUSE_CLEAR;
    if (y == rows - 2)
        return TUI_MOUSE_INPUT;
    return TUI_MOUSE_NONE;
}
