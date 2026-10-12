#ifndef PRIMEAGEN_SESSION_BRIDGE_H
#define PRIMEAGEN_SESSION_BRIDGE_H
#include "harness/harness.h"
#include <stddef.h>
#include <stdint.h>

/* Explicit per-turn wire safety bounds, not history or session-count limits. */
#define SESSION_REQUEST_CAP 16384u
#define SESSION_RESPONSE_CAP 65536u
#define SESSION_TIMEOUT_MS_DEFAULT 120000u
#define SESSION_TIMEOUT_MS_MAX 120000u /* Local turn watchdog safety ceiling. */
#define SESSION_APPROVAL_ID_CAP 160u   /* Remote permission identity + NUL. */
#define SESSION_APPROVAL_LABEL_CAP 256u /* Bounded display text; host truncates. */
#define SESSION_NOTE_CAP 512u          /* One bounded streamed activity line. */
/* Owner-affine, borrowed nonblocking pipes. Host owns process and remote session.
 * Storage/status changes through submit/decide/poll/free, never raw identity
 * setters. No copies of live instances; no provider tokens or socket ownership. */
typedef struct SessionBridge {
    Harness harness;
    int readFd;
    int writeFd;
    bool owo;
    uint32_t jobId;
    uint64_t startedMs;
    char request[SESSION_REQUEST_CAP];
    size_t requestLength;
    size_t requestOffset;
    char response[SESSION_RESPONSE_CAP];
    size_t responseLength;
    char text[SESSION_RESPONSE_CAP];
    bool failed;
    bool awaiting;
    uint32_t approvalSerial;
    char approvalId[SESSION_APPROVAL_ID_CAP];
    char approvalLabel[SESSION_APPROVAL_LABEL_CAP];
    char note[SESSION_NOTE_CAP];
    bool notePending;
    char decision[8];
} SessionBridge;
SessionBridge SessionBridge_0(void);
SessionBridge SessionBridge_2(int readFd, int writeFd);
#define SESSION_ARITY_(_0, _1, _2, N, ...) N
#define SESSION_ARITY(...) SESSION_ARITY_(0 __VA_OPT__(,) __VA_ARGS__, 2, 1, 0)
#define SESSION_JOIN_(a, b) a##b
#define SESSION_JOIN(a, b) SESSION_JOIN_(a, b)
#define SessionBridge(...) SESSION_JOIN(SessionBridge_, SESSION_ARITY(__VA_ARGS__))(__VA_ARGS__)
#define SessionBridge_zero() SessionBridge_0()
bool SessionBridge_submit(SessionBridge *self, const char *prompt);
/* Prompt domain is printable ASCII/newline/tab; TUI uses printable ASCII.
 * Live NUL-terminated string required; no arbitrary-address validation. */
/* 0 pending/idle, 1 completed (text available), -1 failed (safe text available).
 * Failure poisons this wire; exit/relaunch required, no stale-response reuse.
 * An approval request is a pending poll: it keeps the turn running and is
 * answered through SessionBridge_decide. The human wait suspends the local
 * deadline and the restarted window begins when the decision is queued. */
int SessionBridge_poll(SessionBridge *self);
void SessionBridge_free(SessionBridge *self);
void SessionBridge_setOwo(SessionBridge *self, bool enabled);
bool SessionBridge_isOwo(const SessionBridge *self);
bool SessionBridge_isBusy(const SessionBridge *self);
/* A human decision is required before the remote turn can continue. Reads only. */
bool SessionBridge_isAwaiting(const SessionBridge *self);
const char *SessionBridge_getApprovalLabel(const SessionBridge *self);
uint32_t SessionBridge_getApprovalSerial(const SessionBridge *self);
/* Borrow the newest streamed activity line once, or nullptr when none is
 * pending. The pointer is valid until the next poll/submit/free; the caller
 * copies it into its own transcript before then. */
const char *SessionBridge_takeNote(SessionBridge *self);
/* Queue exactly one "once"/"always"/"reject" reply; false leaves the wire
 * untouched. The server owns the durable rule; a configured deny always wins. */
bool SessionBridge_decide(SessionBridge *self, const char *decision);
bool SessionBridge_setTimeout(SessionBridge *self, uint64_t timeoutMs);
uint64_t SessionBridge_getTimeout(const SessionBridge *self);
const char *SessionBridge_getText(const SessionBridge *self);
bool SessionBridge_toString(const SessionBridge *self, char *dest, size_t cap, bool *outTruncated);
bool SessionBridge_toStringStruct(const SessionBridge *self, char *dest, size_t cap, bool *outTruncated);
#endif
