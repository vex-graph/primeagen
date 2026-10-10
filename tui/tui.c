#include "tui/tui.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include "exception/throw.h"
#include "net/json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

;;DEFINITION
/**
 * Tui owns the cold, flat display state of a terminal interface: copied transcript
 * entries, a growable ASCII command line, live terminal geometry and a viewport
 * offset measured from the bottom. It owns neither an agent nor a terminal screen.
 * Ncurses consumes its row projections; later agent events can append text without
 * printing into terminal scrollback. The UI thread excludes mutation/read overlap.
 *
 * Input and entries grow geometrically from caller-selected capacities. Rejection
 * preserves initialized text/count and ownership; partial constructors free their
 * buffers. Transcript bytes are made terminal-safe at admission, so rendering
 * never executes escape sequences. Wrapping is derived from current terminal-cell
 * width, not a guessed fixed screen size. Following the tail and reading older
 * output are distinct states: appends preserve a scrolled viewport's old content.
 *
 * This preview owns ordinary display buffers, not an ecosystem allocator or
 * persistent history backend. Shallow copying a live Tui is outside the contract.
 * No JSON parser is duplicated: cold structure projection uses Vexspoke's actual
 * Json_writeString for the quoted input field. Unicode cell editing and provider
 * integration are separate future work, not inferred from the ncurses frontend.
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Tui (tui/tui.c)
 * ============================================================================
 * STRUCT FIELDS (Mirroring tui/tui.h):
 * ----------------------------------------------------------------------------
 *   Tui {
 *     TuiEntry *entries;       // owned flat directory of copied display entries
 *     size_t count;            // initialized entries, never a rejection ceiling
 *     size_t capacity;         // current directory allocation; grows cold
 *     char *input;             // owned NUL-terminated printable-ASCII input
 *     size_t inputLength;      // initialized input bytes, excluding terminator
 *     size_t inputCapacity;    // current input allocation, including terminator
 *     size_t scroll;           // wrapped display rows above the bottom viewport
 *     int rows;                // current terminal rows; zero valid before attach
 *     int columns;             // current terminal columns; zero before attach
 *     bool dirty;              // repaint demand; ncurses clears after success
 *   }
 *
 * SLOT RECORD (behaviorless TuiEntry, owned only by Tui):
 * ----------------------------------------------------------------------------
 *   TuiEntry {
 *     char *text;              // owned terminal-safe ASCII/newline bytes
 *     size_t length;           // byte extent excluding terminator
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Tui Tui_0(void)
 *   - Tui Tui_1(size_t inputCapacity)
 *   - Tui Tui_2(size_t inputCapacity, size_t entryCapacity)
 *   - Tui(...) / Tui_zero()
 *
 * Core operations:
 *   - bool Tui_add(Tui *self, const char *text, size_t length)
 *   - bool Tui_entriesReserve(Tui *self, size_t capacity)
 *   - bool Tui_inputReserve(Tui *self, size_t capacity)
 *   - bool Tui_inputAddChar(Tui *self, int character)
 *   - bool Tui_inputBackspace(Tui *self)
 *   - void Tui_inputClear(Tui *self)
 *   - void Tui_clear(Tui *self)
 *
 * Setters:
 *   - bool Tui_setInput(Tui *self, const char *text, size_t length)
 *   - bool Tui_setSize(Tui *self, int rows, int columns)
 *   - void Tui_setScroll(Tui *self, size_t rowsFromBottom)
 *   - void Tui_scrollBy(Tui *self, int64_t delta)
 *   - void Tui_setDirty(Tui *self, bool dirty)
 *
 * Getters:
 *   - void Tui_getSize(const Tui *self, int *outRows, int *outColumns)
 *   - bool Tui_isReady(const Tui *self) / Tui_isDirty(const Tui *self)
 *   - size_t Tui_getCount / getCapacity / getInputLength / getInputCapacity /
 *            getScroll / getViewportHeight / getViewportWidth /
 *            getVisualRowCount / getFirstVisibleRow(const Tui *self)
 *   - const char *Tui_getInput(const Tui *self)
 *
 * Projections:
 *   - bool Tui_copyRow(const Tui *self, size_t rowIndex,
 *                     char *dest, size_t cap, bool *outTruncated)
 *   - bool Tui_toString(const Tui *self, char *dest, size_t cap,
 *                       bool *outTruncated)
 *   - bool Tui_toStringStruct(const Tui *self, char *dest, size_t cap,
 *                             bool *outTruncated)
 *
 * Lifecycle:
 *   - void Tui_free(Tui *self)
 *
 * Private static helpers:
 *   - reject(reason) / growth(current, needed, elementSize, dest)
 *   - wrappedRows(length, width)
 *   - format(dest, cap, outTruncated, pattern, ...)
 *
 * CONSTRUCTION / FAILURE / OWNERSHIP CONTRACT:
 * ----------------------------------------------------------------------------
 *   Invalid capacities or allocation failure return an all-zero unready value.
 *   Invalid input, overflow and truncated projections reject loudly once.
 *   Setters reject rather than partially apply invalid input; scroll clamps to
 *   current content. Getters return safe defaults on nullptr. Backspace on an
 *   empty input returns false as an ordinary no-change condition, not rejection.
 *   Row projections do not escape directory pointers. getInput is borrowed only
 *   until the next input mutation/free. Clear releases entries but retains
 *   backing capacity; free releases all state and is repeatable. No worker,
 *   per-frame reflection, generic pointer validation or concurrency is promised.
 * ============================================================================
 */

;;INTENTION("derived count/capacity/backing/inputLength change only through admitted operations; raw setters would forge ownership per the Conflict Triage Law + Single Class Per File Law (Java Law)")

/* Report one cold rejection; no error text is supplied by model output here. */
static bool reject(const char *reason) {
    THROW("tui: %s", reason);
    return false;
}

/* Select geometric cold growth without wrapping capacity or allocation bytes. */
static bool growth(size_t current, size_t needed, size_t elementSize, size_t *dest) {
    if (needed > PTRDIFF_MAX / elementSize)
        return reject("capacity overflow");
    size_t capacity = current == 0 ? 1 : current;
    while (capacity < needed) {
        if (capacity > (size_t) PTRDIFF_MAX / elementSize / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    *dest = capacity;
    return true;
}

/* Construct an empty owner or reclaim every partially allocated buffer. */
Tui Tui_2(size_t inputCapacity, size_t entryCapacity) {
    Tui self = {0};
    if (inputCapacity == 0 || entryCapacity == 0 || inputCapacity > PTRDIFF_MAX || entryCapacity > PTRDIFF_MAX / sizeof(TuiEntry)) {
        reject("invalid initial capacities");
        return self;
    }
    self.input = malloc(inputCapacity);
    self.entries = calloc(entryCapacity, sizeof(TuiEntry));
    if (self.input == nullptr || self.entries == nullptr) {
        Tui_free(&self);
        reject("construction allocation failed");
        return self;
    }
    self.input[0] = '\0';
    self.inputCapacity = inputCapacity;
    self.capacity = entryCapacity;
    self.dirty = true;
    return self;
}

Tui Tui_0(void) {
    return Tui_2(TUI_INPUT_CAPACITY_DEFAULT, TUI_ENTRY_CAPACITY_DEFAULT);
}

Tui Tui_1(size_t inputCapacity) {
    return Tui_2(inputCapacity, TUI_ENTRY_CAPACITY_DEFAULT);
}

/* Grow backing without moving or freeing any entry's copied text. */
bool Tui_entriesReserve(Tui *self, size_t capacity) {
    if (!Tui_isReady(self))
        return reject("unready entry owner");
    if (capacity <= (*self).capacity)
        return true;
    size_t next;
    if (!growth((*self).capacity, capacity, sizeof(TuiEntry), &next))
        return false;
    TuiEntry *entries = realloc((*self).entries, next * sizeof(TuiEntry));
    if (entries == nullptr)
        return reject("entry growth allocation failed");
    (*self).entries = entries;
    (*self).capacity = next;
    return true;
}

/* Reserve more input room; caller must not retain a prior borrowed input view. */
bool Tui_inputReserve(Tui *self, size_t capacity) {
    if (!Tui_isReady(self))
        return reject("unready input owner");
    if (capacity <= (*self).inputCapacity)
        return true;
    size_t next;
    if (!growth((*self).inputCapacity, capacity, sizeof(char), &next))
        return false;
    char *input = realloc((*self).input, next);
    if (input == nullptr)
        return reject("input growth allocation failed");
    (*self).input = input;
    (*self).inputCapacity = next;
    return true;
}

/* Sanitize before publishing: even ESC, NUL and UTF-8 bytes are inert display. */
bool Tui_add(Tui *self, const char *text, size_t length) {
    if (!Tui_isReady(self) || text == nullptr || length > (size_t) PTRDIFF_MAX / 4 - 1 || (*self).count == SIZE_MAX)
        return reject("invalid transcript span");
    size_t previousRows = Tui_getVisualRowCount(self);
    char *copy = malloc(length * 4 + 1);
    if (copy == nullptr)
        return reject("transcript allocation failed");
    static const char HEX[] = "0123456789abcdef";
    size_t used = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char) text[i];
        if (c == '\n' || (c >= ' ' && c <= '~'))
            copy[used++] = (char) c;
        else {
            copy[used++] = '\\';
            copy[used++] = 'x';
            copy[used++] = HEX[c >> 4];
            copy[used++] = HEX[c & 15];
        }
    }
    copy[used] = '\0';
    if (!Tui_entriesReserve(self, (*self).count + 1)) {
        free(copy);
        return false;
    }
    TuiEntry *entry = &(*self).entries[(*self).count];
    (*entry).text = copy;
    (*entry).length = used;
    ++(*self).count;
    if ((*self).scroll != 0) {
        size_t added = Tui_getVisualRowCount(self) - previousRows;
        (*self).scroll = added > SIZE_MAX - (*self).scroll ? SIZE_MAX : (*self).scroll + added;
    }
    Tui_setScroll(self, (*self).scroll);
    (*self).dirty = true;
    return true;
}

/* Copy validated ASCII before growth, so self/interior input aliases stay safe. */
bool Tui_setInput(Tui *self, const char *text, size_t length) {
    if (!Tui_isReady(self) || text == nullptr || length >= PTRDIFF_MAX)
        return reject("invalid input span");
    for (size_t i = 0; i < length; ++i)
        if ((unsigned char) text[i] < ' ' || (unsigned char) text[i] > '~')
            return reject("input editing is printable ASCII only");
    char *copy = malloc(length + 1);
    if (copy == nullptr)
        return reject("input copy allocation failed");
    memcpy(copy, text, length);
    copy[length] = '\0';
    if (!Tui_inputReserve(self, length + 1)) {
        free(copy);
        return false;
    }
    memcpy((*self).input, copy, length + 1);
    free(copy);
    (*self).inputLength = length;
    (*self).dirty = true;
    return true;
}

bool Tui_inputAddChar(Tui *self, int character) {
    if (!Tui_isReady(self) || character < ' ' || character > '~' || (*self).inputLength >= PTRDIFF_MAX - 1)
        return reject("invalid input character");
    if (!Tui_inputReserve(self, (*self).inputLength + 2))
        return false;
    (*self).input[(*self).inputLength++] = (char) character;
    (*self).input[(*self).inputLength] = '\0';
    (*self).dirty = true;
    return true;
}

bool Tui_inputBackspace(Tui *self) {
    if (!Tui_isReady(self))
        return reject("unready input owner");
    if ((*self).inputLength == 0)
        return false;
    (*self).input[--(*self).inputLength] = '\0';
    (*self).dirty = true;
    return true;
}

void Tui_inputClear(Tui *self) {
    if (self == nullptr || (*self).input == nullptr)
        return;
    (*self).input[0] = '\0';
    (*self).inputLength = 0;
    (*self).dirty = true;
}

/* Reclaim copied entries, retain growable backing, and return to tail following. */
void Tui_clear(Tui *self) {
    if (self == nullptr)
        return;
    for (size_t i = 0; i < (*self).count; ++i) {
        TuiEntry *entry = &(*self).entries[i];
        free((*entry).text);
        *entry = (TuiEntry) {0};
    }
    (*self).count = 0;
    (*self).scroll = 0;
    (*self).dirty = true;
}

void Tui_free(Tui *self) {
    if (self == nullptr)
        return;
    Tui_clear(self);
    free((*self).entries);
    free((*self).input);
    *self = (Tui) {0};
}

bool Tui_setSize(Tui *self, int rows, int columns) {
    if (self == nullptr || rows < 0 || columns < 0)
        return reject("invalid terminal geometry");
    (*self).rows = rows;
    (*self).columns = columns;
    Tui_setScroll(self, (*self).scroll);
    (*self).dirty = true;
    return true;
}

void Tui_getSize(const Tui *self, int *outRows, int *outColumns) {
    if (outRows != nullptr)
        *outRows = self == nullptr ? 0 : (*self).rows;
    if (outColumns != nullptr)
        *outColumns = self == nullptr ? 0 : (*self).columns;
}

/* Scroll is clamped to actual current wrapped content, not transcript entries. */
void Tui_setScroll(Tui *self, size_t rowsFromBottom) {
    if (self == nullptr)
        return;
    size_t total = Tui_getVisualRowCount(self);
    size_t height = Tui_getViewportHeight(self);
    size_t maximum = total > height ? total - height : 0;
    (*self).scroll = rowsFromBottom < maximum ? rowsFromBottom : maximum;
    (*self).dirty = true;
}

void Tui_scrollBy(Tui *self, int64_t delta) {
    if (self == nullptr)
        return;
    size_t current = (*self).scroll;
    if (delta >= 0) {
        uint64_t magnitude = (uint64_t) delta;
        size_t next = magnitude > SIZE_MAX - current ? SIZE_MAX : current + (size_t) magnitude;
        Tui_setScroll(self, next);
    } else {
        uint64_t magnitude = (uint64_t) (-(delta + 1)) + 1;
        Tui_setScroll(self, magnitude > current ? 0 : current - (size_t) magnitude);
    }
}

void Tui_setDirty(Tui *self, bool dirty) {
    if (self != nullptr)
        (*self).dirty = dirty;
}

bool Tui_isReady(const Tui *self) { return self != nullptr && (*self).input != nullptr && (*self).entries != nullptr; }
bool Tui_isDirty(const Tui *self) { return self != nullptr && (*self).dirty; }
size_t Tui_getCount(const Tui *self) { return self == nullptr ? 0 : (*self).count; }
size_t Tui_getCapacity(const Tui *self) { return self == nullptr ? 0 : (*self).capacity; }
const char *Tui_getInput(const Tui *self) { return self == nullptr ? nullptr : (*self).input; }
size_t Tui_getInputLength(const Tui *self) { return self == nullptr ? 0 : (*self).inputLength; }
size_t Tui_getInputCapacity(const Tui *self) { return self == nullptr ? 0 : (*self).inputCapacity; }
size_t Tui_getScroll(const Tui *self) { return self == nullptr ? 0 : (*self).scroll; }
size_t Tui_getViewportHeight(const Tui *self) { return self == nullptr || (*self).rows < TUI_MIN_ROWS ? 0 : (size_t) ((*self).rows - TUI_CHROME_ROWS); }
size_t Tui_getViewportWidth(const Tui *self) { return self == nullptr || (*self).columns < TUI_MIN_COLUMNS ? 0 : (size_t) ((*self).columns - 2); }

static size_t wrappedRows(size_t length, size_t width) {
    return length == 0 ? 1 : length / width + (length % width != 0);
}

/* Measure logical lines and wrap chunks without allocating a flattened copy. */
size_t Tui_getVisualRowCount(const Tui *self) {
    size_t width = Tui_getViewportWidth(self);
    if (self == nullptr || width == 0)
        return 0;
    size_t total = 0;
    for (size_t i = 0; i < (*self).count; ++i) {
        const TuiEntry *entry = &(*self).entries[i];
        size_t start = 0;
        for (size_t at = 0; at <= (*entry).length; ++at)
            if (at == (*entry).length || (*entry).text[at] == '\n') {
                size_t rows = wrappedRows(at - start, width);
                if (rows > SIZE_MAX - total)
                    return SIZE_MAX;
                total += rows;
                start = at + 1;
            }
    }
    return total;
}

size_t Tui_getFirstVisibleRow(const Tui *self) {
    size_t total = Tui_getVisualRowCount(self);
    size_t height = Tui_getViewportHeight(self);
    if (self == nullptr || total <= height)
        return 0;
    size_t bottom = total - height;
    return (*self).scroll > bottom ? 0 : bottom - (*self).scroll;
}

/* Copy one full visual row or flag the partial buffer; absence leaves empty text. */
bool Tui_copyRow(const Tui *self, size_t rowIndex, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        return reject("missing row destination");
    }
    dest[0] = '\0';
    size_t width = Tui_getViewportWidth(self);
    if (self == nullptr || width == 0)
        return reject("row unavailable");
    for (size_t i = 0; i < (*self).count; ++i) {
        const TuiEntry *entry = &(*self).entries[i];
        size_t start = 0;
        for (size_t at = 0; at <= (*entry).length; ++at)
            if (at == (*entry).length || (*entry).text[at] == '\n') {
                size_t length = at - start;
                size_t rows = wrappedRows(length, width);
                if (rowIndex < rows) {
                    size_t offset = rowIndex * width;
                    size_t remaining = length - offset;
                    size_t count = remaining < width ? remaining : width;
                    size_t copied = count < cap - 1 ? count : cap - 1;
                    memcpy(dest, (*entry).text + start + offset, copied);
                    dest[copied] = '\0';
                    if (copied != count) {
                        if (outTruncated != nullptr)
                            *outTruncated = true;
                        return reject("row projection truncated");
                    }
                    return true;
                }
                rowIndex -= rows;
                start = at + 1;
            }
    }
    return reject("row index out of range");
}

/* Common bounded cold formatter; callers never project an unvalidated row. */
static bool format(char *dest, size_t cap, bool *outTruncated, const char *pattern, ...) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        return reject("missing projection destination");
    }
    va_list args;
    va_start(args, pattern);
    int length = vsnprintf(dest, cap, pattern, args);
    va_end(args);
    if (length < 0 || (size_t) length >= cap) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        return reject("projection truncated");
    }
    return true;
}

bool Tui_toString(const Tui *self, char *dest, size_t cap, bool *outTruncated) {
    if (self == nullptr)
        return format(dest, cap, outTruncated, "nullptr");
    return format(dest, cap, outTruncated, "tui: %zu entries, %zu input bytes, %zu rows above tail", (*self).count, (*self).inputLength, (*self).scroll);
}

/* Declaration-order field dump, with real Vexspoke JSON string escaping. */
bool Tui_toStringStruct(const Tui *self, char *dest, size_t cap, bool *outTruncated) {
    if (self == nullptr)
        return format(dest, cap, outTruncated, "nullptr");
    if (!format(dest, cap, outTruncated, "Tui{entries=%p,count=%zu,capacity=%zu,input=", (void*) (*self).entries, (*self).count, (*self).capacity))
        return false;
    size_t used = strlen(dest);
    int64_t length = Json_writeString(dest + used, cap - used, (*self).input == nullptr ? "" : (*self).input);
    if (length < 0) {
        dest[used] = '\0';
        if (outTruncated != nullptr)
            *outTruncated = true;
        return reject("input projection truncated");
    }
    used += (size_t) length;
    return format(dest + used, cap - used, outTruncated, ",inputLength=%zu,inputCapacity=%zu,scroll=%zu,rows=%d,columns=%d,dirty=%s}", (*self).inputLength, (*self).inputCapacity, (*self).scroll, (*self).rows, (*self).columns, (*self).dirty ? "true" : "false");
}
