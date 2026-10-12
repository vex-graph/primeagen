#ifndef PRIMEAGEN_ENV_FILE_H
#define PRIMEAGEN_ENV_FILE_H

#include <stdbool.h>

// src/env_file.h — literal admission of the checkout-root private key file.
//
// Reads OPENAI_API_KEY / ANTHROPIC_API_KEY only, with existing process
// environment entries (including empty ones) taking precedence. Parsing is
// literal (no shell, no expansion, no interpolation). The WHOLE file is
// validated before ANY environment change, so a rejected file leaves the
// environment untouched. Diagnostics never echo file contents. A missing file
// is a no-op (true). Not a secure store, encryption or hostile-parent sandbox.

bool EnvFile_load(const char *path);

#endif
