#include "tui/markdown.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "exception/throw.h"
#include <string.h>

;;DEFINITION
/* A cold terminal adapter converts a deliberately small Markdown subset to
 * visible ASCII and parallel display flags. It never emits ANSI or invokes a
 * terminal API. The owning Tui sanitizes untrusted model bytes before this seam;
 * ncurses consumes host-selected flags afterward. Flat spans and an explicit
 * lookahead budget prevent recursive or quadratic hostile parsing. This is not
 * CommonMark, HTML, a link executor or a Vexspoke API. Code whitespace is literal;
 * repeated prose blank lines compact without changing the remote conversation. */
;;OVERVIEW
/* ============================================================================
 * MODULE: Markdown terminal adapter (tui/markdown.c)
 * OWNED STATE: none; caller retains disjoint live source/output/style spans.
 * FUNCTION REGISTRY:
 *   bool Markdown_render(const char *source, size_t length, char *dest,
 *       uint8_t *styles, size_t cap, size_t *outLength, bool *outTruncated)
 *     Validate complete terminal-safe source/capacity before writes; strip flat
 *     paired inline syntax and heading/fence markers; emit style per visible byte.
 * PRIVATE STATIC HELPERS:
 *   size_t closing(source, from, end, marker, markerLength, budget)
 *     Match on one logical line, skip escaped bytes, spend bounded lookahead.
 *   size_t line(source, start, end, base, dest, styles, budget)
 *     Emit one logical line; code spans take precedence; unmatched markup literal.
 * CONTRACT: cold, synchronous, serialized/disjoint spans; no allocation, IO,
 * callbacks or recursion. Source is ASCII/newline, not raw terminal controls.
 * cap >= length+1; truncation false on success, true on insufficient destination.
 * Invalid input preserves destination/styles; output length resets to zero.
 * Four-times-length lookahead budget is a safety bound, not a text-size ceiling.
 * Exhausted lookahead preserves remaining markup literally, never loses text.
 * All flags are host-owned constants from text_style.h. No arbitrary pointers,
 * UTF-8 cell layout, nested spans, links, tables or syntax highlighting promised.
 * ============================================================================ */
#define MARKDOWN_LOOKAHEAD_FACTOR 4u /* Bounds total scans of hostile delimiters. */
#define MARKDOWN_HEADING_MAX 6u /* Markdown's fixed ATX heading levels. */
#define MARKDOWN_FENCE_LENGTH 3u

static size_t closing(const char *source, size_t from, size_t end,
                      const char *marker, size_t markerLength, size_t *budget) {
    for (size_t at = from; at < end; ++at) {
        if (*budget == 0)
            return end;
        --*budget;
        if (source[at] == '\\' && at + 1 < end) {
            ++at;
            continue;
        }
        if (markerLength <= end - at && memcmp(source + at, marker, markerLength) == 0)
            return at;
    }
    return end;
}

static size_t line(const char *source, size_t start, size_t end, uint8_t base,
                   char *dest, uint8_t *styles, size_t *budget) {
    size_t used = 0;
    size_t at = start;
    while (at < end) {
        char c = source[at];
        if (c == '\\' && at + 1 < end && strchr("\\*_`#", source[at + 1]) != nullptr) {
            dest[used] = source[at + 1];
            styles[used++] = base;
            at += 2;
            continue;
        }
        size_t markerLength = 0;
        uint8_t style = 0;
        if (c == '`') {
            markerLength = 1;
            style = TEXT_STYLE_CODE;
        } else if ((c == '*' || c == '_') && at + 1 < end && source[at + 1] == c) {
            markerLength = 2;
            style = TEXT_STYLE_BOLD;
        } else if (c == '*' || (c == '_' && (at == start || source[at - 1] == ' '))) {
            markerLength = 1;
            style = TEXT_STYLE_ITALIC;
        }
        if (markerLength != 0) {
            size_t close = closing(source, at + markerLength, end, source + at, markerLength, budget);
            if (close > at + markerLength && close < end) {
                for (size_t i = at + markerLength; i < close; ++i) {
                    dest[used] = source[i];
                    styles[used++] = base | style;
                }
                at = close + markerLength;
                continue;
            }
        }
        dest[used] = c;
        styles[used++] = base;
        ++at;
    }
    return used;
}

bool Markdown_render(const char *source, size_t length, char *dest,
                     uint8_t *styles, size_t cap, size_t *outLength, bool *outTruncated) {
    if (outLength != nullptr)
        *outLength = 0;
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (source == nullptr || dest == nullptr || styles == nullptr || outLength == nullptr || length > SIZE_MAX / MARKDOWN_LOOKAHEAD_FACTOR) {
        THROW("markdown: invalid source or output span");
        return false;
    }
    if (cap <= length) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("markdown: output capacity must cover input plus terminator");
        return false;
    }
    for (size_t i = 0; i < length; ++i)
        if (source[i] != '\n' && ((unsigned char) source[i] < ' ' || (unsigned char) source[i] > '~')) {
            THROW("markdown: input must be terminal-safe ASCII or newline");
            return false;
        }
    size_t budget = length * MARKDOWN_LOOKAHEAD_FACTOR;
    size_t used = 0;
    size_t at = 0;
    size_t blankRun = 0;
    bool fenced = false;
    while (at < length) {
        size_t end = at;
        while (end < length && source[end] != '\n')
            ++end;
        bool fence = end - at >= MARKDOWN_FENCE_LENGTH && memcmp(source + at, "```", MARKDOWN_FENCE_LENGTH) == 0;
        if (fence && fenced)
            for (size_t i = at + MARKDOWN_FENCE_LENGTH; i < end; ++i)
                if (source[i] != ' ')
                    fence = false;
        if (fence) {
            fenced = !fenced;
            blankRun = 0;
        } else {
            bool blank = true;
            for (size_t i = at; i < end; ++i)
                if (source[i] != ' ')
                    blank = false;
            if (fenced || !blank || blankRun == 0) {
                uint8_t base = fenced ? TEXT_STYLE_CODE : 0;
                size_t start = at;
                if (!fenced) {
                    size_t hashes = 0;
                    while (start + hashes < end && source[start + hashes] == '#')
                        ++hashes;
                    if (hashes > 0 && hashes <= MARKDOWN_HEADING_MAX && start + hashes < end && source[start + hashes] == ' ') {
                        start += hashes + 1;
                        base = TEXT_STYLE_HEADING | TEXT_STYLE_BOLD;
                    }
                }
                if (fenced) {
                    memcpy(dest + used, source + start, end - start);
                    memset(styles + used, base, end - start);
                    used += end - start;
                } else
                    used += line(source, start, end, base, dest + used, styles + used, &budget);
                if (end < length) {
                    dest[used] = '\n';
                    styles[used++] = base;
                }
            }
            blankRun = blank ? blankRun + 1 : 0;
        }
        at = end < length ? end + 1 : end;
    }
    while (used > 0 && dest[used - 1] == '\n' && !(styles[used - 1] & TEXT_STYLE_CODE))
        --used;
    dest[used] = '\0';
    styles[used] = 0;
    *outLength = used;
    return true;
}
