#include "src/settings.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include "exception/throw.h"
#include "hot/manifest.h"

;;DEFINITION
/**
 * DEFINITION: settings (persisted C-TUI settings)
 *
 * The terminal persists provider/model/personality/consent so a launch knows how
 * to behave without re-typing flags. The record is a small key=value file under
 * the manifest app-data root (~/Library/Application Support/vexgraph/primeagen,
 * the same MANIFEST((MANIFEST_APP_DATA, "vexgraph", "primeagen")) root the R1
 * host owns). Load is all-or-nothing: a malformed file applies nothing and keeps
 * defaults. Save is atomic (temp + rename) so a crash never leaves a torn file.
 * No secrets are stored here.
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Settings (src/settings.c)
 * ============================================================================
 * STRUCT FIELDS (Mirroring src/settings.h):
 * ----------------------------------------------------------------------------
 *   Settings {
 *     char provider[SETTINGS_PROVIDER_MAX]; // openai / anthropic / local ("" = unset)
 *     char model[SETTINGS_MODEL_MAX];       // selected model id ("" = unset)
 *     bool owo;                             // canonical C personality prompt on
 *     uint32_t consent;                     // SettingsConsent (prompt / deny)
 *     char appearance[SETTINGS_APPEARANCE_MAX]; // TerminalAppearance preset name
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Settings_0()                       : defaults (unset provider/model, owo off, prompt, striped)
 * Setters / Getters (bounded, reject-overflow keeps prior):
 *   - Settings_setProvider/getProvider, setModel/getModel,
 *     setOwo/isOwo, setConsent/getConsent, setAppearance/getAppearance
 * Path:
 *   - Settings_resolvePath(dest, cap)    : <app-data root>/settings (manifest)
 * Persistence:
 *   - Settings_load(self, path)          : missing -> defaults; malformed -> false, nothing applied
 *   - Settings_save(self, path)          : atomic temp + rename
 *
 * Cold, synchronous, owner-thread; bounded file (SETTINGS_BYTES_MAX). Rejection is
 * loud (THROW) and preserves the previous record.
 * ============================================================================
 */

;;INTENTION("settings persist under the R1 manifest app-data root, not a private path; the manifest owns the root per the No Hardcoding Law")

#define SETTINGS_BYTES_MAX 4096u

static bool copyBounded(char *dest, size_t cap, const char *value) {
    if (value == nullptr)
        return false;
    size_t length = strlen(value);
    if (length >= cap)
        return false;
    memcpy(dest, value, length + 1);
    return true;
}

Settings Settings_0(void) {
    Settings settings = {0};  // provider/model unset, owo off, consent = PROMPT (0)
    copyBounded(settings.appearance, SETTINGS_APPEARANCE_MAX, SETTINGS_APPEARANCE_DEFAULT);
    return settings;
}

bool Settings_setAppearance(Settings *self, const char *value) {
    if (self == nullptr)
        return false;
    return copyBounded((*self).appearance, SETTINGS_APPEARANCE_MAX, value);
}

const char *Settings_getAppearance(const Settings *self) {
    return (self && (*self).appearance[0] != '\0') ? (*self).appearance : SETTINGS_APPEARANCE_DEFAULT;
}

bool Settings_setProvider(Settings *self, const char *value) {
    if (self == nullptr)
        return false;
    return copyBounded((*self).provider, SETTINGS_PROVIDER_MAX, value);
}

bool Settings_setModel(Settings *self, const char *value) {
    if (self == nullptr)
        return false;
    return copyBounded((*self).model, SETTINGS_MODEL_MAX, value);
}

void Settings_setOwo(Settings *self, bool value) {
    if (self != nullptr)
        (*self).owo = value;
}

bool Settings_setConsent(Settings *self, uint32_t value) {
    if (self == nullptr || value > SETTINGS_CONSENT_DENY)
        return false;
    (*self).consent = value;
    return true;
}

const char *Settings_getProvider(const Settings *self) {
    return self == nullptr ? "" : (*self).provider;
}

const char *Settings_getModel(const Settings *self) {
    return self == nullptr ? "" : (*self).model;
}

bool Settings_isOwo(const Settings *self) {
    return self != nullptr && (*self).owo;
}

uint32_t Settings_getConsent(const Settings *self) {
    return self == nullptr ? SETTINGS_CONSENT_PROMPT : (*self).consent;
}

static bool ensureManifest(void) {
    static int state = 0;  // 0 = not tried, 1 = mounted, -1 = failed
    if (state == 0)
        state = MANIFEST(MANIFEST_APP_DATA, "vexgraph", "primeagen") && MANIFEST_ENSURE() ? 1 : -1;
    return state == 1;
}

bool Settings_resolvePath(char *dest, size_t cap) {
    if (dest == nullptr || cap == 0 || !ensureManifest())
        return false;
    const char *root = MANIFEST_ROOT();
    if (root == nullptr)
        return false;
    int written = snprintf(dest, cap, "%s/settings", root);
    return written > 0 && (size_t) written < cap;
}

/** Apply one "name=value" line to `parsed`. False on unknown key / bad value. */
static bool applyLine(Settings *parsed, const char *name, size_t nameLength, const char *value) {
    if (nameLength == 8 && memcmp(name, "provider", 8) == 0)
        return Settings_setProvider(parsed, value);
    if (nameLength == 5 && memcmp(name, "model", 5) == 0)
        return Settings_setModel(parsed, value);
    if (nameLength == 3 && memcmp(name, "owo", 3) == 0) {
        if (strcmp(value, "1") == 0 || strcmp(value, "true") == 0) {
            Settings_setOwo(parsed, true);
            return true;
        }
        if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0) {
            Settings_setOwo(parsed, false);
            return true;
        }
        return false;
    }
    if (nameLength == 7 && memcmp(name, "consent", 7) == 0) {
        if (strcmp(value, "prompt") == 0)
            return Settings_setConsent(parsed, SETTINGS_CONSENT_PROMPT);
        if (strcmp(value, "deny") == 0)
            return Settings_setConsent(parsed, SETTINGS_CONSENT_DENY);
        return false;
    }
    if (nameLength == 10 && memcmp(name, "appearance", 10) == 0)
        return Settings_setAppearance(parsed, value);
    return false;
}

bool Settings_load(Settings *self, const char *path) {
    if (self == nullptr || path == nullptr || path[0] == '\0')
        return false;
    int descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) {
        if (errno == ENOENT)
            return true;  // no file: keep defaults
        THROW("settings: cannot open file");
        return false;
    }
    struct stat info;
    if (fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(descriptor);
        THROW("settings: file must be a regular file");
        return false;
    }
    char buffer[SETTINGS_BYTES_MAX + 1];
    size_t total = 0;
    while (total < SETTINGS_BYTES_MAX + 1) {
        ssize_t got = read(descriptor, buffer + total, (SETTINGS_BYTES_MAX + 1) - total);
        if (got < 0) {
            close(descriptor);
            THROW("settings: cannot read file");
            return false;
        }
        if (got == 0)
            break;
        total += (size_t) got;
    }
    close(descriptor);
    if (total > SETTINGS_BYTES_MAX) {
        THROW("settings: file exceeds safety size limit");
        return false;
    }
    buffer[total] = '\0';

    Settings parsed = Settings_0();
    size_t at = 0;
    while (at < total) {
        size_t end = at;
        while (end < total && buffer[end] != '\n')
            ++end;
        size_t start = at;
        size_t lineEnd = end;
        if (lineEnd > start && buffer[lineEnd - 1] == '\r')
            --lineEnd;
        at = end + 1;
        while (start < lineEnd && (buffer[start] == ' ' || buffer[start] == '\t'))
            ++start;
        while (lineEnd > start && (buffer[lineEnd - 1] == ' ' || buffer[lineEnd - 1] == '\t'))
            --lineEnd;
        buffer[lineEnd] = '\0';  // terminate the value at the line end
        if (start == lineEnd || buffer[start] == '#')
            continue;
        size_t equals = start;
        while (equals < lineEnd && buffer[equals] != '=')
            ++equals;
        if (equals == lineEnd) {
            THROW("settings: malformed line");
            return false;
        }
        buffer[equals] = '\0';
        if (!applyLine(&parsed, buffer + start, equals - start, buffer + equals + 1)) {
            THROW("settings: unknown key or invalid value");
            return false;
        }
    }
    *self = parsed;
    return true;
}

bool Settings_save(const Settings *self, const char *path) {
    if (self == nullptr || path == nullptr || path[0] == '\0')
        return false;
    char temporary[600];
    int written = snprintf(temporary, sizeof temporary, "%s.tmp", path);
    if (written <= 0 || (size_t) written >= sizeof temporary)
        return false;
    int descriptor = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        THROW("settings: cannot open temporary file");
        return false;
    }
    char body[SETTINGS_BYTES_MAX];
    int length = snprintf(body, sizeof body, "provider=%s\nmodel=%s\nowo=%d\nconsent=%s\nappearance=%s\n",
                          (*self).provider, (*self).model, (*self).owo ? 1 : 0,
                          (*self).consent == SETTINGS_CONSENT_DENY ? "deny" : "prompt",
                          Settings_getAppearance(self));
    if (length <= 0 || (size_t) length >= sizeof body) {
        close(descriptor);
        unlink(temporary);
        return false;
    }
    size_t offset = 0;
    while (offset < (size_t) length) {
        ssize_t put = write(descriptor, body + offset, (size_t) length - offset);
        if (put < 0) {
            close(descriptor);
            unlink(temporary);
            THROW("settings: cannot write file");
            return false;
        }
        offset += (size_t) put;
    }
    if (fsync(descriptor) != 0 || close(descriptor) != 0) {
        unlink(temporary);
        THROW("settings: cannot flush file");
        return false;
    }
    if (rename(temporary, path) != 0) {
        unlink(temporary);
        THROW("settings: cannot publish file");
        return false;
    }
    return true;
}
