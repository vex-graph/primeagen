#include "tools/delete.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
;;DEFINITION
/* Remove exactly one approved regular-file name. This irreversible cold action
 * never recursively removes a directory or follows a link. No undo is implied. */
;;OVERVIEW
/* ============================================================================
 * MODULE: delete tool (tools/delete.c)
 * OWNED STATE: local parent fd closes before return; root fd borrowed.
 * FUNCTION REGISTRY: bool ToolDelete_execute(int root,const char *path)
 * CONTRACT: ToolFs canonical path, existing regular single-link target, stable
 * trusted tree. Missing/directory/symlink/hardlink rejects loudly unchanged;
 * successful unlink is permanent. Admission checks are not a racing-writer guard.
 * ============================================================================ */
bool ToolDelete_execute(int root, const char *path) {
    int parent;
    char name[NAME_MAX + 1];
    if (!ToolFs_parent(root, path, &parent, name, sizeof(name)))
        return false;
    struct stat info;
    bool ok = fstatat(parent, name, &info, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(info.st_mode) && info.st_nlink == 1 && unlinkat(parent, name, 0) == 0;
    close(parent);
    if (!ok)
        THROW("delete: regular single-link file required or unlink failed");
    return ok;
}
