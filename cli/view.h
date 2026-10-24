#ifndef PRIMEAGEN_CLI_VIEW_H
#define PRIMEAGEN_CLI_VIEW_H
#include "tui/tui.h"
#include "terminal/terminal_tree_view.h"
// Cold reload contract: resident host owns Tui, ncurses and borrowed session
// pipes. Views own no state or callbacks. Incompatible layout requires a host
// rebuild and a Hot_cold_abi version bump, not migration by address casting.
typedef bool (*PrimeagenDrawFn)(Tui *, bool, bool, bool, bool, bool, char *, uint8_t *, size_t);
typedef int (*PrimeagenCommandFn)(Tui *, bool);
typedef bool (*PrimeagenAppearanceFn)(const char *);
typedef bool (*PrimeagenBannerFn)(const uint8_t *, uint32_t, uint32_t);
typedef bool (*PrimeagenPanelsFn)(Tui *, bool, bool, bool, bool, bool, int, int, int, char *, uint8_t *, size_t);
typedef bool (*PrimeagenTreePanelsFn)(Tui *, TerminalTreeView *, bool, bool, bool, bool, bool,
                                      int, int, int, char *, uint8_t *, size_t);
// Presentation defaults, overridden by the host's --list-width option. Below the
// rail/list safety floor the view collapses to a full-width chat.
#define PRIMEAGEN_LIST_WIDTH_DEFAULT 32
#define PRIMEAGEN_PANELS_MIN_COLUMNS 64
#define PRIMEAGEN_PANEL_PEOPLE 0
#define PRIMEAGEN_PANEL_SERVERS 1
#define PRIMEAGEN_PANEL_CHANNELS 2
#define PRIMEAGEN_PANEL_CHATS 3
#define PRIMEAGEN_PANEL_PRIVATE 4
enum {
    PRIMEAGEN_PAIR_TREE_BACKGROUND = 7,
    PRIMEAGEN_PAIR_TREE_SELECTED = 8,
    PRIMEAGEN_PAIR_TREE_FOLDER = 9,
    PRIMEAGEN_PAIR_TREE_CHAT = 10,
    PRIMEAGEN_PAIR_TREE_USER = 11,
    PRIMEAGEN_PAIR_TREE_SERVER = 12
};
int Primeagen_chatLeft(int columns, int listWidth);
bool Primeagen_panelsClick(int x, int y, int rows, int columns, int listWidth,
                           int panel, int selection, int *outPanel, int *outSelection);
bool Primeagen_drawPanels(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                          int panel, int selection, int listWidth,
                          char *row, uint8_t *styles, size_t cap);
bool Primeagen_drawTreePanels(Tui *state, TerminalTreeView *tree, bool owo, bool focused,
                              bool sessionMode, bool busy, bool failed, int panel, int selection,
                              int listWidth, char *row, uint8_t *styles, size_t cap);
#define PRIMEAGEN_COMMAND_MESSAGE 1
#define PRIMEAGEN_COMMAND_CONNECT 2
#define PRIMEAGEN_COMMAND_QUIT 3
#define PRIMEAGEN_COMMAND_UPDATE 4
int Primeagen_command(Tui *state, bool sessionMode);
// The look travels with the reloadable view: the host resolves this next to the
// draw/command pointers so a hot swap adopts the same appearance.
bool Primeagen_setAppearance(const char *presetName);
// The app's banner image: the host decodes its own PNG and pushes the pixels.
bool Primeagen_setBanner(const uint8_t *rgba, uint32_t width, uint32_t height);
bool Primeagen_draw(Tui *state, bool owo, bool focused, bool sessionMode, bool busy, bool failed,
                    char *row, uint8_t *styles, size_t cap);
#endif
