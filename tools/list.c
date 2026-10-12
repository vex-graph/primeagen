#include "tools/list.h"
#include "net/json.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
;;DEFINITION
/* List one directory's immediate names, sorted and JSON-escaped. No recursion,
 * following of listed links or inferred file permissions. Admission is rooted
 * at the same borrowed directory descriptor as the other native file tools. */
;;OVERVIEW
/* ============================================================================
 * MODULE: list tool (tools/list.c)
 * OWNED STATE: local directory stream and growable flat string-pointer directory;
 * each copied name reclaimed before return. Successful output ownership transfers.
 * FUNCTION REGISTRY: bool ToolList_execute(int root,const char *path,size_t cap,
 *                                         char **dest,size_t *outLength)
 * PRIVATE: compare(left,right): qsort byte-order oracle, no locale dependency.
 * CONTRACT: '.' is root; other paths are canonical no-symlink directories. JSON
 * array of names excludes '.'/'..'; no partial successful listing on cap/OOM/IO
 * refusal. Array capacity grows cold; cap is caller-selected output safety budget.
 * Root fd borrowed; callers serialize and free successful bytes. No filesystem
 * snapshot, recursive traversal or hostile concurrent writer guarantee.
 * ============================================================================ */
#define TOOL_LIST_CAPACITY_DEFAULT 16u /* Growable initial directory, not a ceiling. */
static int compare(const void *left, const void *right) {
    const char *a = *(const char* const*) left;
    const char *b = *(const char* const*) right;
    return strcmp(a, b);
}

bool ToolList_execute(int root, const char *path, size_t cap, char **dest, size_t *outLength) {
    if (root < 0 || path == nullptr || dest == nullptr || outLength == nullptr || cap < 2 || cap > TOOL_BYTES_MAX) {
        THROW("list: invalid arguments or capacity");
        return false;
    }
    int fd;
    if (strcmp(path, ".") == 0)
        fd = openat(root, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    else {
        int parent;
        char name[NAME_MAX + 1];
        if (!ToolFs_parent(root, path, &parent, name, sizeof(name)))
            return false;
        fd = openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        close(parent);
    }
    DIR *directory = fd < 0 ? nullptr : fdopendir(fd);
    if (directory == nullptr) {
        if (fd >= 0)
            close(fd);
        THROW("list: directory admission failed");
        return false;
    }
    char **names = nullptr;
    size_t count = 0, capacity = 0, bytes = 0;
    bool ok = true;
    while (ok) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (entry == nullptr) {
            ok = errno == 0;
            break;
        }
        const char *name = (*entry).d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        size_t length = strlen(name);
        if (length + 1 > cap - bytes) {
            ok = false;
            break;
        }
        bytes += length + 1;
        if (count == capacity) {
            size_t next = capacity == 0 ? TOOL_LIST_CAPACITY_DEFAULT : capacity * 2;
            char **grown = realloc(names, next * sizeof(*names));
            if (grown == nullptr) {
                ok = false;
                break;
            }
            names = grown;
            capacity = next;
        }
        char *copy = strdup(name);
        if (copy == nullptr) {
            ok = false;
            break;
        }
        names[count++] = copy;
    }
    if (closedir(directory) != 0)
        ok = false;
    char *output = ok ? malloc(cap + 1) : nullptr;
    if (output == nullptr)
        ok = false;
    size_t used = 0;
    if (ok) {
        if (count > 1)
            qsort(names, count, sizeof(*names), compare);
        JsonWriter writer;
        JsonWriter_init(&writer, output, cap + 1);
        Json_beginArray(&writer);
        for (size_t i = 0; i < count; ++i)
            Json_stringVal(&writer, names[i]);
        Json_endArray(&writer);
        ok = JsonWriter_ok(&writer);
        used = JsonWriter_used(&writer);
    }
    for (size_t i = 0; i < count; ++i)
        free(names[i]);
    free(names);
    if (!ok) {
        free(output);
        THROW("list: IO/allocation failure or listing would truncate (byte bound %zu)", cap);
        return false;
    }
    *dest = output;
    *outLength = used;
    return true;
}
