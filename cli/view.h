#ifndef PRIMEAGEN_CLI_VIEW_H
#define PRIMEAGEN_CLI_VIEW_H
#include "tui/tui.h"
// Cold reload contract: resident host owns Tui, ncurses and borrowed session
// pipes. Views own no state or callbacks. Incompatible layout requires a host
// rebuild and a Hot_cold_abi version bump, not migration by address casting.
typedef bool (*PrimeagenDrawFn)(Tui *, bool, bool, bool, bool, bool, char *, uint8_t *, size_t);
typedef int (*PrimeagenCommandFn)(Tui *, bool);
typedef bool (*PrimeagenPanelsFn)(Tui *, bool, bool, bool, bool, bool, int, int, int, char *, uint8_t *, size_t);
// Presentation defaults, overridden by the host's --list-width option. Below the
// three-column safety floor the view collapses to a full-width chat.
#define PRIMEAGEN_LIST_WIDTH_DEFAULT 32
#define PRIMEAGEN_PANELS_MIN_COLUMNS 64
#define PRIMEAGEN_RAIL_WIDTH 5
#define PRIMEAGEN_PANEL_PEOPLE 0
#define PRIMEAGEN_PANEL_SERVERS 1
#define PRIMEAGEN_PANEL_CHANNELS 2
int Primeagen_chatLeft(int columns, int listWidth);
bool Primeagen_panelsClick(int x, int y, int rows, int columns, int listWidth,
                           int panel, int selection, int *outPanel, int *outSelection);
bool Primeagen_drawPanels(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                          int panel, int selection, int listWidth,
                          char *row, uint8_t *styles, size_t cap);
#define PRIMEAGEN_COMMAND_MESSAGE 1
#define PRIMEAGEN_COMMAND_CONNECT 2
#define PRIMEAGEN_COMMAND_QUIT 3
#define PRIMEAGEN_COMMAND_UPDATE 4
int Primeagen_command(Tui *state, bool sessionMode);
bool Primeagen_draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                    char *row, uint8_t *styles, size_t cap);
#endif
