#ifndef PRIMEAGEN_TUI_UTF8_H
#define PRIMEAGEN_TUI_UTF8_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

/* MODULE: bounded UTF-8 scalar decoding and terminal-cell measurement.
 * TuiUtf8_decode(bytes,len,outScalar) returns consumed bytes, or 0 without
 * modifying output for malformed/truncated/overlong/surrogate/out-of-range input.
 * TuiUtf8_width(scalar) uses the owner's stable LC_CTYPE/wcwidth; unsupported
 * characters use one fallback cell. No allocation, terminal IO or Unicode
 * grapheme/ZWJ shaping. The host sets locale before admitting display entries.
 * TuiUtf8_safe excludes terminal controls and bidi direction overrides/isolates;
 * malformed/unsafe bytes are escaped by Tui, not passed to the terminal.
 * Bounds below are fixed Unicode/UTF-8 encoding constants, not user settings. */
static inline size_t TuiUtf8_decode(const char *text, size_t length, uint32_t *outScalar) {
    if (text == nullptr || length == 0 || outScalar == nullptr)
        return 0;
    const unsigned char *bytes = (const unsigned char*) text;
    uint32_t scalar = bytes[0];
    size_t count;
    uint32_t minimum;
    if (scalar < 0x80) {
        *outScalar = scalar;
        return 1;
    } else if (scalar >= 0xc2 && scalar <= 0xdf) {
        count = 2; minimum = 0x80; scalar &= 0x1f;
    } else if (scalar >= 0xe0 && scalar <= 0xef) {
        count = 3; minimum = 0x800; scalar &= 0x0f;
    } else if (scalar >= 0xf0 && scalar <= 0xf4) {
        count = 4; minimum = 0x10000; scalar &= 0x07;
    } else
        return 0;
    if (count > length)
        return 0;
    for (size_t i = 1; i < count; ++i) {
        if ((bytes[i] & 0xc0) != 0x80)
            return 0;
        scalar = (scalar << 6) | (bytes[i] & 0x3f);
    }
    if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
        return 0;
    *outScalar = scalar;
    return count;
}

static inline bool TuiUtf8_safe(uint32_t scalar) {
    return scalar == '\n' || (scalar >= 0x20 && scalar != 0x7f &&
        !(scalar >= 0x80 && scalar <= 0x9f) &&
        !(scalar >= 0x202a && scalar <= 0x202e) &&
        !(scalar >= 0x2066 && scalar <= 0x2069));
}

static inline size_t TuiUtf8_width(uint32_t scalar) {
    int width = wcwidth((wchar_t) scalar);
    return width < 0 ? 1 : (size_t) width;
}
#endif
