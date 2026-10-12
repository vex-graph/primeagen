#include "src/json_admit.h"

#include <string.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include "exception/throw.h"

;;DEFINITION
/**
 * DEFINITION: json_admit (bounded JSON admission)
 *
 * Model and provider replies are untrusted text. Admission must reject three
 * hostile shapes without ever recursing on hostile input: nesting deeper than the
 * stack-safe ceiling (JSON_MAX_DEPTH), malformed structure, and duplicate object
 * member names — a later "answer" or tool field must never silently replace an
 * earlier consent-relevant one. The depth scan and the duplicate scan are fully
 * iterative; the only recursion (Vexspoke's reader) is already depth-capped and
 * fails closed, so no admitted input can overflow the C stack.
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: json_admit (src/json_admit.c)
 * ============================================================================
 * No owned class or persistent field; a procedural cold admission entry over
 * caller-owned JsonDoc / JsonNode / scratch storage.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public admission:
 *   - JsonAdmit_parse(doc, nodes, nodeCap, scratch, scratchCap, text)
 *       Depth scan -> Json_parse -> duplicate-key scan. false + THROW on reject.
 *   - JsonAdmit_depthOk(text)
 *       Iterative nesting/balance scan (no recursion) against JSON_MAX_DEPTH.
 *   - JsonAdmit_noDuplicateKeys(doc)
 *       Iterative node-pool scan for a repeated object member key.
 *
 * All input spans must be live; the doc/scratch pool is caller-owned. Rejection
 * mutates nothing outside the doc's own parse state. Cold path only, no
 * concurrency, no allocation.
 * ============================================================================
 */

;;INTENTION("duplicate-member rejection is a consent-safety property and depth is a C-stack safety bound that fails closed, per the Cold-Strict, Hot-Minimal Validation Law")

/**
 * Iterative nesting/balance scan. No recursion: a hostile multi-megabyte nesting
 * bomb is consumed in one pass and rejected, never walked as the stack.
 */
bool JsonAdmit_depthOk(const char *text) {
    if (text == nullptr)
        return false;
    uint32_t depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        char c = *cursor;
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        } else if (c == '"') {
            quoted = true;
        } else if (c == '[' || c == '{') {
            if (++depth > JSON_MAX_DEPTH)
                return false;
        } else if (c == ']' || c == '}') {
            if (depth == 0)
                return false;
            --depth;
        }
    }
    return depth == 0;
}

/** One object's member list must not repeat a key. Iterative over child/next. */
static bool memberKeysUnique(const JsonDoc *doc, JsonRef objectRef) {
    JsonNode *object = &(*doc).nodes[objectRef];
    for (JsonRef a = (*object).child; a >= 0; ) {
        JsonNode *nodeA = &(*doc).nodes[a];
        uint32_t aLen = 0;
        const char *aKey = Json_key(doc, a, &aLen);
        if (aKey == nullptr)
            return false;
        for (JsonRef b = (*nodeA).next; b >= 0; ) {
            JsonNode *nodeB = &(*doc).nodes[b];
            uint32_t bLen = 0;
            const char *bKey = Json_key(doc, b, &bLen);
            if (bKey == nullptr)
                return false;
            if (aLen == bLen && memcmp(aKey, bKey, aLen) == 0)
                return false;
            b = (*nodeB).next;
        }
        a = (*nodeA).next;
    }
    return true;
}

/** Iterative over the whole node pool: no object may carry a duplicate key. */
bool JsonAdmit_noDuplicateKeys(const JsonDoc *doc) {
    if (doc == nullptr || !(*doc).ok)
        return false;
    for (uint32_t i = 0; i < (*doc).nodeCount; ++i) {
        JsonNode *node = &(*doc).nodes[i];
        if ((*node).type != JSON_OBJECT)
            continue;
        if (!memberKeysUnique(doc, (JsonRef) i))
            return false;
    }
    return true;
}

/** Depth scan, parse, then duplicate-key scan. false + one THROW per failure. */
bool JsonAdmit_parse(JsonDoc *doc, JsonNode *nodes, uint32_t nodeCap,
                     char *scratch, uint32_t scratchCap, const char *text) {
    if (doc == nullptr || nodes == nullptr || scratch == nullptr || text == nullptr) {
        THROW("json admit: null admission argument");
        return false;
    }
    if (!JsonAdmit_depthOk(text)) {
        THROW("json admit: nesting safety limit or unbalanced structure");
        return false;
    }
    if (!Json_parse(doc, nodes, nodeCap, scratch, scratchCap, text)) {
        THROW("json admit: malformed, oversized or undecodable document");
        return false;
    }
    if (!JsonAdmit_noDuplicateKeys(doc)) {
        THROW("json admit: duplicate member key rejected");
        return false;
    }
    return true;
}
