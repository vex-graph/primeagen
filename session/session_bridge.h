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
/* Owner-affine, borrowed nonblocking pipes. Host owns process and remote session.
 * Storage/status changes through submit/poll/free, never raw identity setters.
 * No copies of live instances; no provider tokens or socket ownership here. */
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
 * Failure poisons this wire; exit/relaunch required, no stale-response reuse. */
int SessionBridge_poll(SessionBridge *self);
void SessionBridge_free(SessionBridge *self);
void SessionBridge_setOwo(SessionBridge *self, bool enabled);
bool SessionBridge_isOwo(const SessionBridge *self);
bool SessionBridge_isBusy(const SessionBridge *self);
bool SessionBridge_setTimeout(SessionBridge *self, uint64_t timeoutMs);
uint64_t SessionBridge_getTimeout(const SessionBridge *self);
const char *SessionBridge_getText(const SessionBridge *self);
bool SessionBridge_toString(const SessionBridge *self, char *dest, size_t cap, bool *outTruncated);
bool SessionBridge_toStringStruct(const SessionBridge *self, char *dest, size_t cap, bool *outTruncated);
#endif
