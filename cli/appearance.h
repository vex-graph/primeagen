#ifndef PRIMEAGEN_CLI_APPEARANCE_H
#define PRIMEAGEN_CLI_APPEARANCE_H

#include <stdbool.h>
#include <curses.h>

#include "ui/theme/terminal_appearance.h"

// cli/appearance.h — Primeagen's look, chosen in settings, drawn by Determinant.
//
// Primeagen owns the CHOICE (a persisted preset name); the toolkit owns the
// glyphs. The chosen look lives in this module's static state so it travels with
// the reloadable view: the host resolves Primeagen_setAppearance alongside the
// draw/command pointers, so a hot swap applies the same appearance to the new
// code without re-acquiring the terminal.
//
// (This is the first slice of the toolkit migration: the frame's horizontal
// rules are drawn by the toolkit as a connector run instead of a bare ACS_HLINE.)

// Adopt a preset by name (unknown names fall back to the toolkit default).
void PrimeagenAppearance_use(const char *presetName);
const TerminalAppearance *PrimeagenAppearance_current(void);
const char *PrimeagenAppearance_name(void);

// Draw the configured rule across `width` cells at (row, left) into `win`,
// in one pass. False on a null window or a non-positive width.
bool PrimeagenAppearance_drawRule(WINDOW *win, int row, int left, int width);

#endif
