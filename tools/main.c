#include "tools/read.h"
#include "tools/list.h"
#include "tools/write.h"
#include "tools/create.h"
#include "tools/edit.h"
#include "tools/delete.h"
#include "tools/bash.h"
#include "tools/websearch.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
;;DEFINITION
/* One approved tool invocation per supervised child. The host validates exact JSON
 * arguments and obtains consent before spawning; C dispatch receives literal argv
 * and bounded stdin, never scans chat for executable markup. This private bridge
 * has no model/approval state and cannot confer consent on itself. */
;;OVERVIEW
/* ============================================================================
 * MODULE: native tool entry (tools/main.c)
 * OWNED STATE: root directory fd and per-invocation data/output, freed on return;
 * successful bash/websearch exec transfers child lifetime to external supervisor.
 * FUNCTION REGISTRY: int main(int argc,char **argv)
 *   Internal argv: tools ROOT BYTE_CAP TAG ARG...; write/create bytes via stdin.
 *   Exact tags: read/list/write/create/edit/delete/bash/websearch. Strict arities;
 *   output only after successful whole operation. Cold failures diagnose/nonzero.
 * CONTRACT: host-only entry, not an independent permission boundary. Caller supplies
 * trusted stable canonical root, budget and literal arguments. Root fd closes on
 * exec. External host owns overall deadline and process-group cleanup; filesystem
 * calls are not realtime. No stdout on rejection, no recursive writes/deletes.
 * ============================================================================ */
int main(int argc, char **argv) {
    if (argc < 5) {
        THROW("tools: expected root, byte capacity, tag and arguments");
        return 1;
    }
    char *end;
    unsigned long budget = strtoul(argv[2], &end, 10);
    if (*argv[2] == '\0' || *end || budget == 0 || budget > TOOL_BYTES_MAX) {
        THROW("tools: invalid byte capacity");
        return 1;
    }
    int root = open(argv[1], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (root < 0) {
        THROW("tools: workspace directory admission failed");
        return 1;
    }
    const char *tag = argv[3];
    char *data = nullptr, *output = nullptr;
    size_t length = 0, used = 0;
    bool ok = false;
    if (argc == 5 && strcmp(tag, "read") == 0)
        ok = ToolRead_execute(root, argv[4], budget, &output, &used);
    else if (argc == 5 && strcmp(tag, "list") == 0)
        ok = ToolList_execute(root, argv[4], budget, &output, &used);
    else if (argc == 5 && (strcmp(tag, "write") == 0 || strcmp(tag, "create") == 0)) {
        if (ToolFs_input(budget, &data, &length))
            ok = strcmp(tag, "create") == 0 ? ToolCreate_execute(root, argv[4], data, length) : ToolWrite_execute(root, argv[4], data, length);
    } else if (argc == 7 && strcmp(tag, "edit") == 0)
        ok = ToolEdit_execute(root, argv[4], argv[5], argv[6], budget);
    else if (argc == 5 && strcmp(tag, "delete") == 0)
        ok = ToolDelete_execute(root, argv[4]);
    else if (argc == 5 && strcmp(tag, "bash") == 0) {
        if (fchdir(root) == 0)
            ok = ToolBash_execute(argv[4]);
    } else if (argc == 6 && strcmp(tag, "websearch") == 0) {
        if (fchdir(root) == 0)
            ok = ToolWebsearch_execute(argv[4], argv[5]);
    } else
        THROW("tools: unknown tag or invalid arity");
    if (ok && output != nullptr)
        ok = fwrite(output, 1, used, stdout) == used;
    else if (ok)
        ok = fputs("completed", stdout) >= 0;
    if (ok)
        ok = fflush(stdout) == 0;
    free(data);
    free(output);
    close(root);
    return ok ? 0 : 1;
}
