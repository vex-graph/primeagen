#ifndef PRIMEAGEN_OWO_H
#define PRIMEAGEN_OWO_H
#include <stddef.h>
/* Stateless presentation mode. Returns immutable static prose; no permissions,
 * tools, prompt rewriting or model behavior is controlled by this module. */
const char *Owo_greeting(bool enabled);
const char *Owo_status(bool enabled);
#endif
