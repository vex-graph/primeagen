#ifndef PRIMEAGEN_CLI_UPDATE_H
#define PRIMEAGEN_CLI_UPDATE_H
#include "hot/hot.h"
// Trusted local build descriptor: exactly 64 lowercase hexadecimal SHA256 bytes
// followed by newline/EOF. No URL/model-supplied release or remote download.
bool Primeagen_update(HotModule *hot, const char *payload, const char *checksum, uint32_t *loaded);
#endif
