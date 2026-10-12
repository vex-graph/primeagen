#ifndef PRIMEAGEN_SETTINGS_H
#define PRIMEAGEN_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// src/settings.h — persisted C-TUI settings (provider/model/personality/consent).
//
// One bounded record, loaded from and saved to the manifest app-data root
// (~/Library/Application Support/vexgraph/primeagen/settings). A missing file
// yields defaults; a malformed line rejects WITHOUT applying a partial record.
// No secrets are stored here (keys live in the environment / .env).

#define SETTINGS_PROVIDER_MAX 32
#define SETTINGS_MODEL_MAX 128

typedef enum SettingsConsent {
    SETTINGS_CONSENT_PROMPT = 0,  // ask before every tool
    SETTINGS_CONSENT_DENY = 1,    // refuse every tool
} SettingsConsent;

typedef struct Settings {
    char provider[SETTINGS_PROVIDER_MAX];
    char model[SETTINGS_MODEL_MAX];
    bool owo;
    uint32_t consent;
} Settings;

Settings Settings_0(void);

bool Settings_setProvider(Settings *self, const char *value);
bool Settings_setModel(Settings *self, const char *value);
void Settings_setOwo(Settings *self, bool value);
bool Settings_setConsent(Settings *self, uint32_t value);

const char *Settings_getProvider(const Settings *self);
const char *Settings_getModel(const Settings *self);
bool Settings_isOwo(const Settings *self);
uint32_t Settings_getConsent(const Settings *self);

// Resolve <app-data root>/settings (mounting + ensuring the manifest once).
// false when the manifest cannot mount.
bool Settings_resolvePath(char *dest, size_t cap);

// Load into self. Missing file leaves defaults and returns true; a malformed file
// returns false and applies NOTHING.
bool Settings_load(Settings *self, const char *path);
// Atomic save (temp + rename under the same directory).
bool Settings_save(const Settings *self, const char *path);

#endif
