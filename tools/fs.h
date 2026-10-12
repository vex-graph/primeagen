#ifndef PRIMEAGEN_TOOLS_FS_H
#define PRIMEAGEN_TOOLS_FS_H
#include <stddef.h>
#include <sys/types.h>
/* Cold POSIX consumer adapter, not an ecosystem filesystem driver. Caller owns
 * root directory fd and output buffers. Stable trusted directory tree required;
 * descriptor-relative no-symlink admission is not a hostile-writer sandbox.
 * Regular files only, hardlinks reject. No recursive mutations. POSIX filesystem
 * calls may block; the supervising host owns whole-child deadlines/cancellation.
 * Atomic replacement is not crash durability. Killed children may leave a private
 * staging file; no completed mutation is rolled back after later cancellation. */
#define TOOL_BYTES_DEFAULT (128u * 1024u)
#define TOOL_BYTES_MAX (16u * 1024u * 1024u) /* External-input allocation safety bound. */
bool ToolFs_parent(int root, const char *path, int *outParent, char *name, size_t cap);
bool ToolFs_read(int root, const char *path, size_t cap, char **dest, size_t *outLength);
bool ToolFs_input(size_t cap, char **dest, size_t *outLength);
bool ToolFs_publish(int root, const char *path, const char *text, size_t length, bool create);
#endif
