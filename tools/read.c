#include "tools/read.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
;;DEFINITION
/* Read is a cold, whole-file operation over the common admitted filesystem seam.
 * The host approves before invoking it; returned bytes belong to the caller. */
;;OVERVIEW
/* ============================================================================
 * MODULE: read tool (tools/read.c)
 * OWNED STATE: none. Root directory fd borrowed; successful bytes transferred.
 * FUNCTION REGISTRY: bool ToolRead_execute(int root,const char *path,size_t cap,
 *                                         char **dest,size_t *outLength)
 * CONTRACT: regular/single-link/no-symlink canonical relative path; bounded whole
 * file or loud rejection, never success with truncation. ToolFs owns validation
 * and descriptor cleanup; caller excludes writers and frees successful bytes.
 * ============================================================================ */
bool ToolRead_execute(int root, const char *path, size_t cap, char **dest, size_t *outLength) {
    return ToolFs_read(root, path, cap, dest, outLength);
}
