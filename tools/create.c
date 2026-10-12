#include "tools/create.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
;;DEFINITION
/* Create a new regular file, never overwrite an existing identity. Caller approves
 * the literal path/content; parents must already exist. No implicit directories. */
;;OVERVIEW
/* ============================================================================
 * MODULE: create tool (tools/create.c)
 * OWNED STATE: none. Root/text borrowed for one cold operation.
 * FUNCTION REGISTRY: bool ToolCreate_execute(int root,const char *path,
 *                                          const char *text,size_t length)
 * CONTRACT: canonical no-symlink path must be absent. Common ToolFs stages bytes
 * and publishes create-new via link admission, mode 0600; old paths unchanged on
 * rejection. Cleanup error may follow publication and is explicitly reported.
 * No directory creation, crash-durability or hostile-writer sandbox promised.
 * ============================================================================ */
bool ToolCreate_execute(int root, const char *path, const char *text, size_t length) {
    return ToolFs_publish(root, path, text, length, true);
}
