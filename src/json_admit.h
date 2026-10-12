#ifndef PRIMEAGEN_JSON_ADMIT_H
#define PRIMEAGEN_JSON_ADMIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "net/json.h"

// src/json_admit.h — bounded JSON admission for untrusted model/provider text.
//
// Admission runs three passes, two of them fully ITERATIVE so hostile input can
// never recurse past the stack-safe ceiling:
//   (1) JsonAdmit_depthOk        — iterative character scan; rejects nesting beyond
//                                  JSON_MAX_DEPTH and unbalanced brackets.
//   (2) Json_parse               — Vexspoke's fail-closed recursive reader (capped).
//   (3) JsonAdmit_noDuplicateKeys— iterative node-pool scan; a repeated object member
//                                  key is rejected so a later field cannot replace an
//                                  earlier consent-relevant one.
// Every rejection is cold and loud (THROW); caller storage stays caller-owned.

bool JsonAdmit_parse(JsonDoc *doc, JsonNode *nodes, uint32_t nodeCap,
                     char *scratch, uint32_t scratchCap, const char *text);

bool JsonAdmit_depthOk(const char *text);
bool JsonAdmit_noDuplicateKeys(const JsonDoc *doc);

#endif
