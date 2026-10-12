#include "tools/fs.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
;;DEFINITION
;;INTENTION("standalone cold tool consumer adapter exception per the Conflict Triage Law; engine IO/NIO ownership unchanged, host consent/deadlines required")
/* Cold standalone tool-host adapter. All path traversal is relative to an admitted
 * directory fd, with no parent hops or symlink traversal. Each call owns temporary
 * descriptors/buffers until return. File operations require a stable trusted tree;
 * they are neither a race-proof sandbox nor bounded-latency storage. The external
 * host supervises this process with a whole-job deadline and output safety bounds. */
;;OVERVIEW
/* ============================================================================
 * MODULE: tool filesystem admission (tools/fs.c)
 * OWNED STATE: no persistent state; each operation owns local descriptors/buffers.
 * FUNCTION REGISTRY:
 *   bool ToolFs_parent(int root,const char *path,int *outParent,char *name,size_t cap)
 *     Canonical relative path; final name + owned parent fd. Outputs on success only.
 *   bool ToolFs_read(int root,const char *path,size_t cap,char **dest,size_t *outLength)
 *     Whole bounded regular-file bytes, heap output transferred only on success.
 *   bool ToolFs_input(size_t cap,char **dest,size_t *outLength): bounded stdin bytes.
 *   bool ToolFs_publish(int root,const char *path,const char *text,size_t length,bool create)
 *     Create-new by link admission or atomic existing-file replacement by rename.
 * PRIVATE: failure(reason): one cold diagnostic and false; readFd(fd,cap,dest,len)
 *     allocates cap+1, drains until EOF, rejects overflow, preserves output pointers.
 * CONTRACT: externally serialized stable tree; root fd borrowed; all local fds
 * close before return. Existing files must be regular/single-link. Rejected staged
 * publication retains target and removes stage when cleanup succeeds. Cleanup
 * failure rejects explicitly. No implicit parents, recursive deletion, durability,
 * hostile concurrent writers, power-loss or SIGKILL-stage cleanup guarantee.
 * ============================================================================ */
static bool failure(const char *reason) {
    THROW("tool fs: %s", reason);
    return false;
}

bool ToolFs_parent(int root, const char *path, int *outParent, char *name, size_t cap) {
    if (root < 0 || path == nullptr || outParent == nullptr || name == nullptr || cap == 0 ||
        *path == '\0' || *path == '/' || strlen(path) >= PATH_MAX)
        return failure("invalid relative path");
    for (const unsigned char *at = (const unsigned char*) path; *at; ++at)
        if (*at < 32 || *at == 127)
            return failure("control byte in path");
    char copy[PATH_MAX];
    strcpy(copy, path);
    int parent = dup(root);
    if (parent < 0)
        return failure("directory descriptor refused");
    char *part = copy;
    while (true) {
        char *slash = strchr(part, '/');
        if (slash != nullptr)
            *slash = '\0';
        if (*part == '\0' || strcmp(part, ".") == 0 || strcmp(part, "..") == 0 || strlen(part) > NAME_MAX) {
            close(parent);
            return failure("noncanonical path component");
        }
        if (slash == nullptr) {
            if (strlen(part) >= cap) {
                close(parent);
                return failure("final name capacity exceeded");
            }
            strcpy(name, part);
            *outParent = parent;
            return true;
        }
        int next = openat(parent, part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        close(parent);
        if (next < 0)
            return failure("parent missing or symlink refused");
        parent = next;
        part = slash + 1;
    }
}

static bool readFd(int fd, size_t cap, char **dest, size_t *outLength) {
    if (cap == 0 || cap > TOOL_BYTES_MAX || dest == nullptr || outLength == nullptr)
        return failure("invalid byte capacity");
    char *bytes = malloc(cap + 1);
    if (bytes == nullptr)
        return failure("read allocation refused");
    size_t length = 0;
    while (true) {
        ssize_t count = read(fd, bytes + length, cap + 1 - length);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 || (size_t) count > cap - length) {
            free(bytes);
            THROW("tool fs: read failed or would truncate (byte bound %zu)", cap);
            return false;
        }
        if (count == 0)
            break;
        length += (size_t) count;
    }
    bytes[length] = '\0';
    *dest = bytes;
    *outLength = length;
    return true;
}

bool ToolFs_input(size_t cap, char **dest, size_t *outLength) {
    return readFd(STDIN_FILENO, cap, dest, outLength);
}

bool ToolFs_read(int root, const char *path, size_t cap, char **dest, size_t *outLength) {
    if (dest == nullptr || outLength == nullptr || cap == 0 || cap > TOOL_BYTES_MAX)
        return failure("invalid read outputs or byte capacity");
    int parent;
    char name[NAME_MAX + 1];
    if (!ToolFs_parent(root, path, &parent, name, sizeof(name)))
        return false;
    int fd = openat(parent, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    close(parent);
    struct stat info;
    if (fd < 0 || fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1) {
        if (fd >= 0)
            close(fd);
        return failure("read requires regular single-link file");
    }
    char *bytes = nullptr;
    size_t length = 0;
    bool ok = readFd(fd, cap, &bytes, &length);
    if (close(fd) != 0 && ok) {
        free(bytes);
        return failure("read descriptor close failed");
    }
    if (ok) {
        *dest = bytes;
        *outLength = length;
    }
    return ok;
}

bool ToolFs_publish(int root, const char *path, const char *text, size_t length, bool create) {
    if (text == nullptr || length > TOOL_BYTES_MAX)
        return failure("invalid write span");
    int parent;
    char name[NAME_MAX + 1];
    if (!ToolFs_parent(root, path, &parent, name, sizeof(name)))
        return false;
    struct stat info;
    int found = fstatat(parent, name, &info, AT_SYMLINK_NOFOLLOW);
    if ((create && (found == 0 || errno != ENOENT)) ||
        (!create && (found != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1))) {
        close(parent);
        return failure("create requires absent path; write requires regular single-link file");
    }
    char stage[NAME_MAX + 1];
    snprintf(stage, sizeof(stage), ".primeagen-%ld-%08x", (long) getpid(), arc4random());
    int fd = openat(parent, stage, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        close(parent);
        return failure("staging file admission refused");
    }
    bool ok = true;
    if (!create && fchmod(fd, info.st_mode & 0777) != 0)
        ok = false;
    size_t offset = 0;
    while (ok && offset < length) {
        ssize_t count = write(fd, text + offset, length - offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            ok = false;
        else
            offset += (size_t) count;
    }
    if (close(fd) != 0)
        ok = false;
    if (ok)
        ok = create ? linkat(parent, stage, parent, name, 0) == 0 : renameat(parent, stage, parent, name) == 0;
    bool cleaned = true;
    if (create || !ok)
        cleaned = unlinkat(parent, stage, 0) == 0;
    close(parent);
    if (!ok || !cleaned)
        return failure(cleaned ? "publication failed; previous target preserved" : "staging cleanup failed; publication may already have completed");
    return true;
}
