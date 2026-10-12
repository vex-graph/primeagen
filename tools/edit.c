#include "tools/edit.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <stdlib.h>
#include <string.h>
;;DEFINITION
/* Exact-match edit replaces one unique nonempty old span in a bounded text file.
 * No fuzzy matching or unchecked line offsets; ambiguity rejects before staging. */
;;OVERVIEW
/* ============================================================================
 * MODULE: edit tool (tools/edit.c)
 * OWNED STATE: local copied old/new file buffers, reclaimed before return.
 * FUNCTION REGISTRY: bool ToolEdit_execute(int root,const char *path,
 *                          const char *oldText,const char *newText,size_t cap)
 * CONTRACT: root/arguments borrowed; canonical regular single-link file, no NUL
 * bytes; exactly one old match including overlapping matches. Missing/ambiguous/
 * overflow rejects unchanged. New bytes stage through ToolFs atomic replacement.
 * Allocation failure is loud. No concurrent editor/merge, undo or metadata parity.
 * ============================================================================ */
bool ToolEdit_execute(int root, const char *path, const char *oldText, const char *newText, size_t cap) {
    if (oldText == nullptr || newText == nullptr || *oldText == '\0' || cap == 0 || cap > TOOL_BYTES_MAX) {
        THROW("edit: invalid replacement arguments");
        return false;
    }
    char *text;
    size_t length;
    if (!ToolFs_read(root, path, cap, &text, &length))
        return false;
    char *match = memchr(text, '\0', length) == nullptr ? strstr(text, oldText) : nullptr;
    size_t oldLength = strlen(oldText), newLength = strlen(newText);
    if (match == nullptr || strstr(match + 1, oldText) != nullptr || newLength > cap - (length - oldLength)) {
        free(text);
        THROW("edit: match absent/ambiguous, binary input or byte capacity exceeded");
        return false;
    }
    size_t nextLength = length - oldLength + newLength;
    char *next = malloc(nextLength + 1);
    if (next == nullptr) {
        free(text);
        THROW("edit: replacement allocation refused");
        return false;
    }
    size_t prefix = (size_t) (match - text);
    memcpy(next, text, prefix);
    memcpy(next + prefix, newText, newLength);
    memcpy(next + prefix + newLength, match + oldLength, length - prefix - oldLength);
    next[nextLength] = '\0';
    bool ok = ToolFs_publish(root, path, next, nextLength, false);
    free(next);
    free(text);
    return ok;
}
