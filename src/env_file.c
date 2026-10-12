#include "src/env_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "exception/throw.h"

;;DEFINITION
/**
 * DEFINITION: env_file (private key-file admission)
 *
 * The checkout-root .env supplies OPENAI_API_KEY / ANTHROPIC_API_KEY, but only
 * two literal names are accepted and only when the environment does not already
 * carry them (the shell always wins, even with an empty value). The file is read
 * once into a bounded buffer and fully validated before anything is published, so
 * a bad file never leaves a half-applied environment. Reads are no-follow and
 * non-blocking, and a non-regular target (symlink, directory, FIFO) is rejected.
 * Error text never includes file contents, values or offending line text.
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: env_file (src/env_file.c)
 * ============================================================================
 * No owned class or persistent field; a procedural cold admission entry.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public admission:
 *   - EnvFile_load(path)  : open/validate/parse, then publish only if all valid
 * Private helpers (file-local):
 *   - isKeyName(name, length)             : exact two-name allowlist
 *   - valueCharsValid(value, length)      : literal printable-ASCII value check
 *   - stripQuotes(&value, &length)        : one matching quote pair, if present
 *   - parseAll(buffer, length, out, cap)  : whole-file validation -> pending set
 *
 * Atomic: pending entries are applied (setenv, existing-wins) only after the
 * entire file validates. Cold, synchronous, owner-thread; no allocation.
 * ============================================================================
 */

#define ENV_FILE_BYTES_MAX 16384u  // explicit private-key-file safety bound
#define ENV_FILE_KEYS_MAX 2u

typedef struct PendingKey {
    const char *name;    // NUL-terminated, in-place within the buffer
    const char *value;   // NUL-terminated, in-place within the buffer
} PendingKey;

static bool isKeyName(const char *name, size_t length) {
    static const char *const KEYS[ENV_FILE_KEYS_MAX] = {"OPENAI_API_KEY", "ANTHROPIC_API_KEY"};
    for (size_t i = 0; i < ENV_FILE_KEYS_MAX; ++i) {
        if (strlen(KEYS[i]) == length && memcmp(KEYS[i], name, length) == 0)
            return true;
    }
    return false;
}

static bool valueCharsValid(const char *value, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char) value[i];
        if (c < 33 || c > 126 || c == '"' || c == '\'')
            return false;
    }
    return true;
}

/** Strip one matching quote pair when the value opens with a quote. */
static bool stripQuotes(const char **value, size_t *length) {
    const char *text = *value;
    size_t size = *length;
    if (size > 0 && (text[0] == '\'' || text[0] == '"')) {
        if (size < 2 || text[size - 1] != text[0])
            return false;
        *value = text + 1;
        *length = size - 2;
    }
    return true;
}

/**
 * Validate the whole file into `out`. Mutates only the owned buffer (NUL
 * termination). Returns the number of keys, or -1 on any rejection.
 */
static int parseAll(char *buffer, size_t length, PendingKey *out, size_t cap) {
    size_t count = 0;
    size_t at = 0;
    while (at < length) {
        size_t end = at;
        while (end < length && buffer[end] != '\n')
            ++end;
        size_t lineStart = at;
        size_t lineEnd = end;                       // exclusive of '\n'
        if (lineEnd > lineStart && buffer[lineEnd - 1] == '\r')
            --lineEnd;                              // permit CRLF endings
        while (lineStart < lineEnd && (buffer[lineStart] == ' ' || buffer[lineStart] == '\t'))
            ++lineStart;
        while (lineEnd > lineStart && (buffer[lineEnd - 1] == ' ' || buffer[lineEnd - 1] == '\t'))
            --lineEnd;
        at = end + 1;
        if (lineStart == lineEnd || buffer[lineStart] == '#')
            continue;

        size_t equals = lineStart;
        while (equals < lineEnd && buffer[equals] != '=')
            ++equals;
        if (equals == lineEnd)
            return -1;                              // no assignment
        size_t nameStart = lineStart;
        size_t nameEnd = equals;
        while (nameEnd > nameStart && (buffer[nameEnd - 1] == ' ' || buffer[nameEnd - 1] == '\t'))
            --nameEnd;
        if (!isKeyName(buffer + nameStart, nameEnd - nameStart))
            return -1;
        for (size_t i = 0; i < count; ++i) {
            if (strlen(out[i].name) == nameEnd - nameStart &&
                memcmp(out[i].name, buffer + nameStart, nameEnd - nameStart) == 0)
                return -1;                          // duplicate assignment
        }

        size_t valueStart = equals + 1;
        size_t valueEnd = lineEnd;
        while (valueStart < valueEnd && (buffer[valueStart] == ' ' || buffer[valueStart] == '\t'))
            ++valueStart;
        while (valueEnd > valueStart && (buffer[valueEnd - 1] == ' ' || buffer[valueEnd - 1] == '\t'))
            --valueEnd;
        const char *value = buffer + valueStart;
        size_t valueLength = valueEnd - valueStart;
        if (!stripQuotes(&value, &valueLength))
            return -1;
        if (!valueCharsValid(value, valueLength))
            return -1;
        if (count >= cap)
            return -1;

        buffer[nameEnd] = '\0';
        char *valueCopy = (char *) value;
        valueCopy[valueLength] = '\0';
        out[count].name = buffer + nameStart;
        out[count].value = valueCopy;
        ++count;
    }
    return (int) count;
}

bool EnvFile_load(const char *path) {
    if (path == nullptr || path[0] == '\0')
        return false;
    int descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) {
        if (errno == ENOENT)
            return true;                            // missing file: no-op
        THROW("env file: cannot open private key file");
        return false;
    }
    struct stat info;
    if (fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(descriptor);
        THROW("env file: private key file must be a regular file");
        return false;
    }
    char buffer[ENV_FILE_BYTES_MAX + 1];
    size_t total = 0;
    while (total < ENV_FILE_BYTES_MAX + 1) {
        ssize_t got = read(descriptor, buffer + total, (ENV_FILE_BYTES_MAX + 1) - total);
        if (got < 0) {
            close(descriptor);
            THROW("env file: cannot read private key file");
            return false;
        }
        if (got == 0)
            break;
        total += (size_t) got;
    }
    close(descriptor);
    if (total > ENV_FILE_BYTES_MAX) {
        THROW("env file: exceeds safety size limit");
        return false;
    }
    buffer[total] = '\0';

    PendingKey pending[ENV_FILE_KEYS_MAX];
    int count = parseAll(buffer, total, pending, ENV_FILE_KEYS_MAX);
    if (count < 0) {
        THROW("env file: invalid or duplicate key assignment");
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (getenv(pending[i].name) == nullptr)     // existing environment wins
            setenv(pending[i].name, pending[i].value, 1);
    }
    return true;
}
