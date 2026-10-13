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

    // The banner: decode once with a libc buffer and hand it to the screen.
    char bannerPath[1200];
    const char *root = getenv("PRIMEAGEN_ROOT");
    if (root != nullptr)
        (void) snprintf(bannerPath, sizeof bannerPath, "%s/resources/>w<.png", root);
    else
        (void) snprintf(bannerPath, sizeof bannerPath, "resources/>w<.png");
    FILE *bannerFile = fopen(bannerPath, "rb");
    if (bannerFile != nullptr) {
        static uint8_t pngBytes[1u << 20];
        size_t length = fread(pngBytes, 1, sizeof pngBytes, bannerFile);
        (void) fclose(bannerFile);
        uint8_t *rgba = nullptr;
        uint32_t bannerWidth = 0, bannerHeight = 0;
        if (Png_decodeMalloc(pngBytes, length, &rgba, &bannerWidth, &bannerHeight) == PNG_OK)
            PrimeagenScreen_setBanner(rgba, bannerWidth, bannerHeight);
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
    // Poll instead of waiting for a key: a zoom, a fullscreen toggle or a window
    // drag changes the terminal size, and ncurses can consume that change itself
    // (updating its internal screen) without ever handing the app a KEY_RESIZE —
    // so an app that only reacts to keys never revalidates. A 100 ms tick lets the
    // loop notice the new size and repaint; nothing is painted while it is idle.
    (void) wtimeout(stdscr, PRIMEAGEN_SCREEN_POLL_MS);
    if (has_colors()) {
        (void) start_color();
        (void) use_default_colors();
    }

    static const char *menuText[] = { "People", "Servers", "Settings" };
    PrimeagenScreen screen;
    PrimeagenScreen_init(&screen);
    int home = 1;
    int running = 1;
    int focusPanel = 0;
    int paint = 1;
    size_t lastRows = 0, lastColumns = 0;
    while (running) {
        int rows = 0, columns = 0;
        getmaxyx(stdscr, rows, columns);
        // A zoom, fullscreen or drag is a SIZE CHANGE, and it is revalidated here —
        // before painting — rather than trusting a KEY_RESIZE that may never come.
        bool resized = (size_t) rows != lastRows || (size_t) columns != lastColumns;
        (void) PrimeagenScreen_revalidate(&screen, rows, columns);
        if (!PrimeagenScreen_isBound(&screen))
            break;
        if (resized) {
            (void) clear();     // nothing stale survives the change
            paint = 1;
        }
        if (paint) {
            if (home) {
                PrimeagenScreen_renderHome(&screen, "Primeagen", menuText, 3, 0, PrimeagenScreen_banner());
            } else {
                PrimeagenScreenState state = {0};
                state.tui = &tui;
                state.focused = true;
                state.focusPanel = focusPanel;
                (void) PrimeagenScreen_renderChat(&screen, &state);
            }
            if (!PrimeagenScreen_present(&screen, stdscr))
                break;
            paint = 0;
        }
        lastRows = (size_t) rows;
        lastColumns = (size_t) columns;
        int key = getch();
        if (key == ERR)
            continue;           // idle tick: re-check the size, paint nothing
        paint = 1;
        switch (PrimeagenScreen_classify(key)) {
            case PRIMEAGEN_SCREEN_EVENT_QUIT:  running = 0; break;
            case PRIMEAGEN_SCREEN_EVENT_HOME:  home = 1; break;
            case PRIMEAGEN_SCREEN_EVENT_CHAT:  home = 0; break;
            case PRIMEAGEN_SCREEN_EVENT_FOCUS: focusPanel = (focusPanel + 1) % 3; break;
            case PRIMEAGEN_SCREEN_EVENT_RESIZE: break;   // handled by the size check
            default: break;
        }
    }
    endwin();
    PrimeagenScreen_free(&screen);
    Tui_free(&tui);
    return 0;
}
