#include "personality/owo.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/* OwO is the optional personal presentation voice of Primeagen. It supplies
 * immutable ASCII greeting/status strings selected by the CLI mode. It does not
 * rewrite user text or safety decisions. A future agent personality contract must
 * be admitted separately; these strings do not claim model integration.
 */
;;OVERVIEW
/* ============================================================================
 * MODULE: OwO presentation voice (personality/owo.c)
 * ============================================================================
 * OWNED STATE: none; returned strings have static lifetime and are read-only.
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * const char *Owo_greeting(bool enabled)
 *   Select neutral or playful offline-preview welcome.
 * const char *Owo_status(bool enabled)
 *   Select neutral or playful status without concealing offline operation.
 * Contracts:
 *   Both bool modes valid; no failure, allocation, mutable global, terminal
 *   control bytes, transport, tool invocation or authorization side effects.
 * ============================================================================
 */
const char *Owo_greeting(bool enabled) {
    return enabled ? "owo! Primeagen is here >w< (offline TUI preview)" : "Primeagen: offline C23 TUI preview";
}
const char *Owo_status(bool enabled) {
    return enabled ? "owo mode | offline | mouse + keyboard" : "neutral | offline | mouse + keyboard";
}
