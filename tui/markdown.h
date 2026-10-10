#ifndef PRIMEAGEN_MARKDOWN_H
#define PRIMEAGEN_MARKDOWN_H

#include <stddef.h>
#include <stdint.h>
#include "tui/text_style.h"

/* Cold flat Markdown subset, not CommonMark: double-star/underscore bold,
 * single-star/underscore italics, inline backticks, ATX headings and fences. */
/* Inline spans are flat (no nested span syntax); unmatched delimiters stay */
/* literal. ASCII/newline source must already be terminal-safe. Controls and */
/* non-ASCII reject before writes. Live disjoint source/dest/style spans required. */
/* cap must cover length+1 even if rendering removes markers. styles parallels */
/* rendered bytes; prose blank runs compact to one blank line, code is literal. */
/* Four input-length units bound closing-marker lookahead; excess remains literal. */
bool Markdown_render(const char *source, size_t length, char *dest,
                     uint8_t *styles, size_t cap, size_t *outLength, bool *outTruncated);

#endif
