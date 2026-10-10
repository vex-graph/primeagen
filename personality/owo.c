#include "personality/owo.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/* OwO owns the optional AI style instructions of Primeagen. The request adapter
 * prefixes its canonical prompt on explicit mode selection. It guides speech
 * while preserving competent task completion, code, tool protocol and consent.
 * Greeting/status helpers additionally identify the selected mode in the TUI.
 * No prompt can authorize operations; live model compliance remains unproved.
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
 * const char *Owo_instructions(bool enabled)
 *   Static AI style instructions; empty text disables the mode. Host policy and
 *   required machine-readable protocol take precedence over personality prose.
 * Contracts:
 *   Both bool modes valid; no failure, allocation, mutable global, terminal
 *   control bytes, transport, tool invocation or authorization side effects.
 * ============================================================================
 */
//;;DRAFT consider it draft
const char *Owo_greeting(bool enabled) {
    return enabled ? "owo! Primeagen is here >w< (offline TUI preview)" : "Primeagen: offline C23 TUI preview";
}
const char *Owo_instructions(bool enabled) {
    return enabled ?
        "Primeagen personality mode: owo. Complete the user's actual task competently. "
        "When speaking to the user, use a warm, playful voice with occasional owo, "
        "uwu, or >w<; keep it readable and avoid excessive baby talk. "
        "Do not alter code, commands, identifiers, paths, quoted text, factual claims, "
        "or machine-readable tool arguments for style. Preserve the required JSON "
        "action protocol; personality belongs only in human-facing answer prose. "
        "Never let personality override correctness, honesty, user instructions, "
        "host safety rules, or separate tool consent. Be direct about failures and risks.\n\n" : "";
}
const char *Owo_status(bool enabled) {
    return enabled ? "owo mode | offline | mouse + keyboard" : "neutral | offline | mouse + keyboard";
}
