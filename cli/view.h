#ifndef PRIMEAGEN_CLI_VIEW_H
#define PRIMEAGEN_CLI_VIEW_H
#include "tui/tui.h"
// Cold reload contract: resident host owns Tui, ncurses and borrowed session
// pipes. Views own no state or callbacks. Incompatible layout requires a host
// rebuild and a Hot_cold_abi version bump, not migration by address casting.
typedef bool (*PrimeagenDrawFn)(Tui *, bool, bool, bool, bool, char *, uint8_t *, size_t);
typedef int (*PrimeagenCommandFn)(Tui *, bool);
#define PRIMEAGEN_COMMAND_MESSAGE 1
#define PRIMEAGEN_COMMAND_CONNECT 2
#define PRIMEAGEN_COMMAND_QUIT 3
#define PRIMEAGEN_COMMAND_UPDATE 4
int Primeagen_command(Tui *state, bool sessionMode);
bool Primeagen_draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy,
                    char *row, uint8_t *styles, size_t cap);
#endif
