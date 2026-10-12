#include "tools/bash.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <unistd.h>
;;DEFINITION
/* Explicitly approved Bash replaces the supervised tool child, never system().
 * The external host owns pipes, whole-job deadlines, process-group kill/reap and
 * consent. Shell commands are trusted after human approval, not sandboxed. */
;;OVERVIEW
/* ============================================================================
 * MODULE: bash tool (tools/bash.c)
 * OWNED STATE: none. Command borrowed until exec; process lifetime transfers to Bash.
 * FUNCTION REGISTRY: bool ToolBash_execute(const char *command)
 * CONTRACT: nonempty command, caller-established workspace cwd; /bin/bash with
 * --noprofile/--norc. Success never returns. Failure emits one diagnostic/false.
 * Bash inherits host environment/privileges and can escape workspace/delete files/
 * contact networks. File-tool confinement is not a Bash sandbox or shell policy.
 * Host must obtain explicit per-command consent and supervise descendants.
 * ============================================================================ */
bool ToolBash_execute(const char *command) {
    if (command != nullptr && *command != '\0')
        execl("/bin/bash", "bash", "--noprofile", "--norc", "-c", command, (char*) nullptr);
    THROW("bash: invalid command or exec failed");
    return false;
}
