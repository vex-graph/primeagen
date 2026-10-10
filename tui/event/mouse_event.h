#ifndef PRIMEAGEN_MOUSE_EVENT_H
#define PRIMEAGEN_MOUSE_EVENT_H
#include "tui/tui.h"

/* Terminal cells, not native-window pixels. Backend normalizes events; no curses
 * types escape. Release does not activate; click activates exactly once. Wheel
 * scrolls only inside the transcript. Outside/small-layout events are no-ops. */
typedef enum TuiMouseAction {
    TUI_MOUSE_NONE, TUI_MOUSE_INPUT, TUI_MOUSE_CLEAR, TUI_MOUSE_QUIT
} TuiMouseAction;
typedef enum TuiMouseKind {
    TUI_MOUSE_CLICK, TUI_MOUSE_WHEEL_UP, TUI_MOUSE_WHEEL_DOWN
} TuiMouseKind;
TuiMouseAction TuiMouse_dispatch(Tui *self, int x, int y, TuiMouseKind kind);
#endif
