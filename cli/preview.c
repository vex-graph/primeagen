#define _XOPEN_SOURCE_EXTENDED 1
// cli/preview.c — run the NEW Primeagen frame (cli/screen.c) in a real terminal,
// without touching the live view or the reload path. A development preview: it
// owns the terminal, decodes resources/>w<.png with a libc-backed buffer (this
// host has no engine arena), and re-renders on every resize so the "always
// revalidate" behaviour is visible. Keys: 1 = HOME, 2 = CHAT, r = resize, q = quit.
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include "cli/screen.h"
#include "image/png.h"

int main(void) {
    setlocale(LC_CTYPE, "");

    // The badge: decode once with a libc buffer and hand it to the screen.
    char badgePath[1200];
    const char *root = getenv("PRIMEAGEN_ROOT");
    if (root != nullptr)
        (void) snprintf(badgePath, sizeof badgePath, "%s/resources/>w<.png", root);
    else
        (void) snprintf(badgePath, sizeof badgePath, "resources/>w<.png");
    FILE *badgeFile = fopen(badgePath, "rb");
    if (badgeFile != nullptr) {
        static uint8_t pngBytes[1u << 20];
        size_t length = fread(pngBytes, 1, sizeof pngBytes, badgeFile);
        (void) fclose(badgeFile);
        uint8_t *rgba = nullptr;
        uint32_t badgeWidth = 0, badgeHeight = 0;
        if (Png_decodeMalloc(pngBytes, length, &rgba, &badgeWidth, &badgeHeight) == PNG_OK)
            PrimeagenScreen_setBadge(rgba, badgeWidth, badgeHeight);
    }

    Tui tui = Tui();
    (void) Tui_add(&tui, "Sola:", 5);
    (void) Tui_add(&tui, "placeholder text", 16);
    (void) Tui_add(&tui, "Muse Writer:", 12);
    (void) Tui_add(&tui, "im on it sir!", 13);
    (void) Tui_add(&tui, "[Muse Writer: read] preferences.md", 34);
    (void) Tui_add(&tui, "[User: edit] vex/vex/filename.c  [+32][-9]", 43);
    (void) Tui_setInput(&tui, "please tell muse to do the thing rn!", 34);

    (void) initscr();
    (void) cbreak();
    (void) noecho();
    (void) keypad(stdscr, true);
    (void) curs_set(0);

    static const char *menu[] = { "People", "Servers", "Settings" };
    PrimeagenScreen screen;
    PrimeagenScreen_init(&screen);
    int home = 1;
    int running = 1;
    while (running) {
        int rows = 0, columns = 0;
        getmaxyx(stdscr, rows, columns);
        if (!PrimeagenScreen_bind(&screen, (size_t) rows, (size_t) columns))
            break;
        if (home) {
            PrimeagenScreen_renderHome(&screen, "Primeagen", menu, 3, 0, PrimeagenScreen_badge());
        } else {
            PrimeagenScreenState state = {0};
            state.tui = &tui;
            state.focused = true;
            (void) PrimeagenScreen_renderChat(&screen, &state);
        }
        if (!PrimeagenScreen_present(&screen, stdscr))
            break;
        int key = getch();
        if (key == 'q' || key == 'Q')
            running = 0;
        else if (key == '1')
            home = 1;
        else if (key == '2')
            home = 0;
    }
    endwin();
    PrimeagenScreen_free(&screen);
    Tui_free(&tui);
    return 0;
}
