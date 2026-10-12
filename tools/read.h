#ifndef PRIMEAGEN_TOOLS_READ_H
#define PRIMEAGEN_TOOLS_READ_H
#include "tools/fs.h"
bool ToolRead_execute(int root, const char *path, size_t cap, char **dest, size_t *outLength);
#endif
