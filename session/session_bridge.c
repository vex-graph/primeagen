#include "session/session_bridge.h"
#include "personality/owo.h"
#include "net/json.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include "annotation/what.h"
#include "exception/throw.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

;;DEFINITION
/* SessionBridge borrows two host-created nonblocking pipes and consumes API
 * Haven's actual EngineProvider/Harness contracts. The host owns OpenCode login,
 * model selection, child cleanup and its dedicated remote session. A turn is an
 * injected Harness job, not a new connector or replacement agent engine. Writes
 * and reads advance incrementally on the terminal owner thread; no blocking wait
 * or provider API runs here. Cold admission bounds each frame and escapes JSON
 * through Vexspoke's writer. A malformed/failed/timed-out wire is poisoned rather
 * than risking attributing a late reply to another turn.
 *
 * A turn can pause for a human. The host emits a bounded approval frame when the
 * remote policy requires consent; the bridge records it as a pending question,
 * keeps the turn open, and suspends its local deadline because the wait belongs
 * to the terminal, not the model. SessionBridge_decide queues exactly one
 * once/always/reject reply and restarts the remote watchdog. The bridge never
 * decides by itself and never auto-approves: only an explicit terminal decision
 * leaves here, and the durable project rule stays OpenCode's.
 *
 * A turn is also a stream, not one lump. The host emits a bounded {note:...} line
 * as each tool acts, and the bridge surfaces it through SessionBridge_takeNote so
 * the terminal can append it immediately. Exactly one frame is consumed per poll,
 * so a burst of notes stays ordered and none is dropped. Free cancels
 * admission before the host releases pipes; remote cancellation is best effort,
 * not proof of a killed server operation. Derived buffers/identity have no raw
 * setters per the Conflict Triage Law and Single Class Per File Law (Java Law). */
;;OVERVIEW
/* ============================================================================
 * CLASS: SessionBridge (session/session_bridge)
 * STRUCT FIELDS (Mirroring session/session_bridge.h):
 *   Harness harness : API Haven owner of admitted single in-flight job.
 *   int readFd : borrowed nonblocking host response pipe; -1 unbound.
 *   int writeFd : borrowed nonblocking host request pipe; -1 unbound.
 *   bool owo : selects canonical Owo_instructions for subsequent turns.
 *   uint32_t jobId : current Haven job; zero means idle.
 *   uint64_t startedMs : monotonic clock of the last observed progress.
 *   char request[SESSION_REQUEST_CAP] : bounded outbound JSON frame queue.
 *   size_t requestLength : queued frame bytes, including newline.
 *   size_t requestOffset : bytes already accepted by the pipe.
 *   char response[SESSION_RESPONSE_CAP] : inbound newline JSON frame.
 *   size_t responseLength : accumulated bytes; preserves partial frames.
 *   char text[SESSION_RESPONSE_CAP] : copied answer or safe generic error.
 *   bool failed : permanent poisoned-wire latch until host relaunch.
 *   bool awaiting : an approval frame is displayed and unanswered.
 *   uint32_t approvalSerial : increments per approval frame; display identity.
 *   char approvalId[SESSION_APPROVAL_ID_CAP] : remote permission identity.
 *   char approvalLabel[SESSION_APPROVAL_LABEL_CAP] : bounded display text.
 *   char note[SESSION_NOTE_CAP] : newest streamed activity line.
 *   bool notePending : a note is waiting for the terminal to take it.
 *   char decision[8] : queued reply spelling; empty when nothing is queued.
 * FUNCTION REGISTRY:
 *   SessionBridge SessionBridge_0(void) : unbound.
 *   SessionBridge SessionBridge_2(int readFd, int writeFd) : borrowed pipes.
 *   bool SessionBridge_submit(SessionBridge *, const char *) : cold admission.
 *   int SessionBridge_poll(SessionBridge *) : bounded IO; one frame per call.
 *   void SessionBridge_free(SessionBridge *) : cancel borrowed job, never close fd.
 *   void SessionBridge_setOwo(SessionBridge *, bool), bool SessionBridge_isOwo(const SessionBridge *)
 *   bool SessionBridge_isBusy(const SessionBridge *), const char *SessionBridge_getText(const SessionBridge *)
 *   bool SessionBridge_isAwaiting(const SessionBridge *)
 *   const char *SessionBridge_getApprovalLabel(const SessionBridge *)
 *   uint32_t SessionBridge_getApprovalSerial(const SessionBridge *)
 *   const char *SessionBridge_takeNote(SessionBridge *) : borrow one note, clear it.
 *   bool SessionBridge_decide(SessionBridge *, const char *) : queue once/always/reject.
 *   bool SessionBridge_setTimeout(SessionBridge *, uint64_t), uint64_t SessionBridge_getTimeout(const SessionBridge *) : validated cold current watchdog, 1..120000ms.
 *   bool SessionBridge_toString(const SessionBridge *, char *, size_t, bool *) : bounded value.
 *   bool SessionBridge_toStringStruct(const SessionBridge *, char *, size_t, bool *) : bounded redacted fields.
 * Private static helpers:
 *   uint64_t nowMs(void) : monotonic clock, zero on clock failure.
 *   bool queueText(SessionBridge *, const char *, const char *) : one-field frame.
 *   bool spawn(void *, const EngineProviderSlot *, const char *, size_t, uint64_t, void **) : queue JSON.
 *   HarnessStatus handleFrame(SessionBridge *, JsonDoc *) : classify one reply frame.
 *   HarnessStatus pollJob(void *, void *, uint64_t) : progress pipes, one frame per call.
 *   void cancel(void *, void *) : clear pending IO, poison wire.
 * CONTRACT: trusted live pipes, valid owner address, serialized calls only.
 * Null/default safe, busy rejection preserves current turn; no arbitrary pointer
 * validation. Prompt is printable ASCII/newline/tab, response bytes are escaped
 * by the display. Local helper emits exactly {ok:boolean,text:string},
 * {ask:{id,label}} or {note:string}; decisions emit {decision:string}. Duplicate
 * keys, embedded NUL, trailing junk in one frame and excess fields reject. Error
 * texts are generic and never contain provider diagnostics/tokens. Notes are
 * single bounded display lines, never parsed as instructions. No history storage
 * or secrets.
 * ============================================================================ */
;;INTENTION("derived wire identity/buffers mutate only through admission/decide/poll/free; projections redact transcript buffers and summarize the borrowed Haven driver (which has no bounded projection), preserving privacy under the Conflict Triage Law")
static uint64_t nowMs(void) {
    struct timespec stamp;
    if (clock_gettime(CLOCK_MONOTONIC, &stamp) != 0)
        return 0;
    return (uint64_t) stamp.tv_sec * 1000u + (uint64_t) stamp.tv_nsec / 1000000u;
}
;;WHAT("SessionBridge")
static bool queueText(SessionBridge *self, const char *key, const char *value) {
    JsonWriter writer;
    JsonWriter_init(&writer, (*self).request, sizeof((*self).request) - 1);
    Json_beginObject(&writer);
    Json_addKey(&writer, key);
    Json_stringVal(&writer, value);
    Json_endObject(&writer);
    if (!JsonWriter_ok(&writer))
        return false;
    (*self).requestLength = JsonWriter_used(&writer);
    (*self).request[(*self).requestLength++] = '\n';
    (*self).requestOffset = 0;
    return true;
}
;;WHAT("SessionBridge")
static bool spawn(void *context, const EngineProviderSlot *engine, const char *prompt,
                  size_t length, uint64_t timeoutMs, void **outHandle) {
    (void) engine;
    (void) length;
    (void) timeoutMs;
    SessionBridge *self = context;
    JsonWriter writer;
    JsonWriter_init(&writer, (*self).request, sizeof((*self).request) - 1);
    Json_beginObject(&writer);
    Json_addKey(&writer, "prompt");
    Json_stringVal(&writer, prompt);
    Json_addKey(&writer, "instructions");
    Json_stringVal(&writer, Owo_instructions((*self).owo));
    Json_endObject(&writer);
    if (!JsonWriter_ok(&writer))
        return false;
    (*self).requestLength = JsonWriter_used(&writer);
    (*self).request[(*self).requestLength++] = '\n';
    (*self).requestOffset = 0;
    (*self).responseLength = 0;
    (*self).startedMs = nowMs();
    (*self).text[0] = '\0';
    (*self).awaiting = false;
    (*self).notePending = false;
    (*self).note[0] = '\0';
    (*self).decision[0] = '\0';
    *outHandle = self;
    return (*self).startedMs != 0;
}
;;WHAT("SessionBridge")
static HarnessStatus handleFrame(SessionBridge *self, JsonDoc *doc) {
    JsonRef root = Json_root(doc);
    if (Json_type(doc, root) != JSON_OBJECT)
        return HARNESS_STATUS_TIMEOUT;
    JsonRef ask = Json_member(doc, root, "ask");
    JsonRef note = Json_member(doc, root, "note");
    if (Json_count(doc, root) == 1 && Json_type(doc, ask) == JSON_OBJECT) {
        JsonRef identity = Json_member(doc, ask, "id");
        JsonRef label = Json_member(doc, ask, "label");
        if (Json_count(doc, ask) != 2 || Json_type(doc, identity) != JSON_STRING ||
            Json_type(doc, label) != JSON_STRING)
            return HARNESS_STATUS_TIMEOUT;
        uint32_t idLength, labelLength;
        const char *idValue = Json_string(doc, identity, &idLength);
        const char *labelValue = Json_string(doc, label, &labelLength);
        if (!idValue || idLength == 0 || memchr(idValue, '\0', idLength) || !labelValue ||
            memchr(labelValue, '\0', labelLength) ||
            !Json_getString(doc, identity, (*self).approvalId, sizeof((*self).approvalId)) ||
            !Json_getString(doc, label, (*self).approvalLabel, sizeof((*self).approvalLabel)))
            return HARNESS_STATUS_TIMEOUT;
        (*self).awaiting = true;
        (*self).approvalSerial++;
        return HARNESS_STATUS_RUNNING;
    }
    if (Json_count(doc, root) == 1 && Json_type(doc, note) == JSON_STRING) {
        /* A decoded NUL byte would truncate the display line; reject it instead. */
        uint32_t noteLength;
        const char *noteValue = Json_string(doc, note, &noteLength);
        if (!noteValue || noteLength == 0 || memchr(noteValue, '\0', noteLength) ||
            !Json_getString(doc, note, (*self).note, sizeof((*self).note)))
            return HARNESS_STATUS_TIMEOUT;
        (*self).notePending = true;
        return HARNESS_STATUS_RUNNING;
    }
    JsonRef success = Json_member(doc, root, "ok");
    JsonRef text = Json_member(doc, root, "text");
    if (Json_count(doc, root) != 2 ||
        (Json_type(doc, success) != JSON_TRUE && Json_type(doc, success) != JSON_FALSE) ||
        Json_type(doc, text) != JSON_STRING)
        return HARNESS_STATUS_TIMEOUT;
    uint32_t textLength;
    const char *value = Json_string(doc, text, &textLength);
    if (!value || memchr(value, '\0', textLength) || !Json_bool(doc, success) ||
        !Json_getString(doc, text, (*self).text, sizeof((*self).text)))
        return HARNESS_STATUS_TIMEOUT;
    return HARNESS_STATUS_DONE;
}
;;WHAT("SessionBridge")
static HarnessStatus pollJob(void *context, void *handle, uint64_t timeoutMs) {
    (void) handle;
    SessionBridge *self = context;
    if ((*self).failed)
        goto fail;
    if ((*self).requestOffset < (*self).requestLength) {
        ssize_t sent = write((*self).writeFd, (*self).request + (*self).requestOffset,
                             (*self).requestLength - (*self).requestOffset);
        if (sent > 0)
            (*self).requestOffset += (size_t) sent;
        else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            goto fail;
    }
    /* Exactly one frame per call keeps a burst of streamed notes ordered. A frame
     * already buffered is progress we hold, so it is consumed before the watchdog
     * is consulted; the watchdog only measures silence with nothing in hand. */
    char *newline = memchr((*self).response, '\n', (*self).responseLength);
    if (newline == nullptr) {
        /* The human wait is not the model's budget: while an approval is displayed
         * the watchdog is suspended and SessionBridge_decide restarts it on
         * submission. Otherwise it measures silence since the last frame, not the
         * turn's total duration, so tool-using turns may run for minutes. */
        if (!(*self).awaiting) {
            uint64_t now = nowMs();
            if (!now || now < (*self).startedMs || now - (*self).startedMs >= timeoutMs)
                goto fail;
        }
        if ((*self).responseLength >= sizeof((*self).response) - 1)
            goto fail;
        ssize_t n = read((*self).readFd, (*self).response + (*self).responseLength,
                         sizeof((*self).response) - 1 - (*self).responseLength);
        if (n == 0)
            goto fail;
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return HARNESS_STATUS_RUNNING;
            goto fail;
        }
        if (memchr((*self).response + (*self).responseLength, '\0', (size_t) n))
            goto fail;
        (*self).responseLength += (size_t) n;
        (*self).response[(*self).responseLength] = '\0';
        newline = memchr((*self).response, '\n', (*self).responseLength);
        if (newline == nullptr)
            return HARNESS_STATUS_RUNNING;
    }
    size_t frame = (size_t) (newline - (*self).response);
    *newline = '\0';
    JsonDoc doc;
    JsonNode nodes[8];
    char scratch[SESSION_RESPONSE_CAP];
    if (!Json_parse(&doc, nodes, 8, scratch, sizeof(scratch), (*self).response))
        goto fail;
    HarnessStatus status = handleFrame(self, &doc);
    size_t remaining = (*self).responseLength - frame - 1;
    memmove((*self).response, newline + 1, remaining);
    (*self).responseLength = remaining;
    if (status == HARNESS_STATUS_TIMEOUT)
        goto fail;
    /* Any frame is progress: the watchdog measures remote silence, not total
     * duration, so a long multi-tool turn is not killed for taking minutes. */
    (*self).startedMs = nowMs();
    if (!(*self).startedMs)
        goto fail;
    return status;
fail:
    (*self).failed = true;
    (*self).awaiting = false;
    (*self).notePending = false;
    strcpy((*self).text, "OpenCode turn failed or timed out. Exit and relaunch; remote cancellation is best effort.");
    return HARNESS_STATUS_TIMEOUT;
}
;;WHAT("SessionBridge")
static void cancel(void *context, void *handle) {
    (void) handle;
    SessionBridge *self = context;
    (*self).failed = true;
    (*self).awaiting = false;
    (*self).notePending = false;
    (*self).requestLength = 0;
}
SessionBridge SessionBridge_0(void) {
    SessionBridge self = {0};
    self.harness = Harness_0();
    Harness_setTimeout(&self.harness, SESSION_TIMEOUT_MS_DEFAULT);
    self.readFd = -1;
    self.writeFd = -1;
    return self;
}
SessionBridge SessionBridge_2(int readFd, int writeFd) {
    SessionBridge self = SessionBridge_0();
    self.readFd = readFd;
    self.writeFd = writeFd;
    Harness_setEngine(&self.harness, EngineProvider_get(EngineProvider_shared(), "opencode"));
    return self;
}
bool SessionBridge_submit(SessionBridge *self, const char *prompt) {
    if (!self || !prompt || !*prompt || (*self).readFd < 0 || (*self).writeFd < 0 ||
        (*self).failed || (*self).jobId || (*self).awaiting ||
        strnlen(prompt, SESSION_REQUEST_CAP) >= SESSION_REQUEST_CAP / 2) {
        THROW("session turn rejected: empty, oversized, busy or unbound wire");
        return false;
    }
    for (const unsigned char *p = (const unsigned char*) prompt; *p; ++p)
        if ((*p < ' ' && *p != '\n' && *p != '\t') || *p > '~') {
            THROW("session prompt must use printable ASCII, newline or tab");
            return false;
        }
    HarnessDriverTable table = {spawn, pollJob, cancel};
    Harness *harness = &(*self).harness;
    Harness_setDriver(harness, self, table);
    bool ok = Harness_run(harness, prompt, strlen(prompt), 0, &(*self).jobId);
    if (!ok)
        THROW("session request exceeds wire capacity");
    return ok;
}
int SessionBridge_poll(SessionBridge *self) {
    if (!self || !(*self).jobId)
        return 0;
    HarnessStatus status = Harness_poll(&(*self).harness, (*self).jobId);
    if (status == HARNESS_STATUS_RUNNING)
        return 0;
    (*self).jobId = 0;
    (*self).awaiting = false;
    return status == HARNESS_STATUS_DONE ? 1 : -1;
}
void SessionBridge_free(SessionBridge *self) {
    if (!self)
        return;
    if ((*self).jobId)
        Harness_cancel(&(*self).harness, (*self).jobId);
    (*self).jobId = 0;
    (*self).awaiting = false;
}
void SessionBridge_setOwo(SessionBridge *self, bool enabled) {
    if (self)
        (*self).owo = enabled;
}
bool SessionBridge_isOwo(const SessionBridge *self) { return self && (*self).owo; }
bool SessionBridge_isBusy(const SessionBridge *self) { return self && (*self).jobId; }
bool SessionBridge_isAwaiting(const SessionBridge *self) { return self && (*self).awaiting; }
const char *SessionBridge_getApprovalLabel(const SessionBridge *self) { return self ? (*self).approvalLabel : ""; }
uint32_t SessionBridge_getApprovalSerial(const SessionBridge *self) { return self ? (*self).approvalSerial : 0; }
const char *SessionBridge_takeNote(SessionBridge *self) {
    if (self == nullptr || !(*self).notePending)
        return nullptr;
    (*self).notePending = false;
    return (*self).note;
}
bool SessionBridge_decide(SessionBridge *self, const char *decision) {
    if (!self || !decision || !(*self).jobId || (*self).failed || !(*self).awaiting ||
        (*self).requestOffset != (*self).requestLength) {
        THROW("session decision rejected: idle, not awaiting, unflushed or null");
        return false;
    }
    if (strcmp(decision, "once") != 0 && strcmp(decision, "always") != 0 && strcmp(decision, "reject") != 0) {
        THROW("session decision must be once, always or reject");
        return false;
    }
    if (!queueText(self, "decision", decision)) {
        THROW("session decision exceeds wire capacity");
        return false;
    }
    strcpy((*self).decision, decision);
    (*self).awaiting = false;
    (*self).startedMs = nowMs();
    return (*self).startedMs != 0;
}
bool SessionBridge_setTimeout(SessionBridge *self, uint64_t timeoutMs) {
    if (!self || !timeoutMs || timeoutMs > SESSION_TIMEOUT_MS_MAX || (*self).jobId) {
        THROW("session timeout rejected: null, busy or out of safety range");
        return false;
    }
    Harness_setTimeout(&(*self).harness, timeoutMs);
    return true;
}
uint64_t SessionBridge_getTimeout(const SessionBridge *self) {
    return self ? Harness_getTimeout(&(*self).harness) : 0;
}
const char *SessionBridge_getText(const SessionBridge *self) { return self ? (*self).text : ""; }
bool SessionBridge_toString(const SessionBridge *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (!dest || !cap)
        return false;
    int n = snprintf(dest, cap, "%s", !self ? "nullptr" : SessionBridge_isAwaiting(self) ? "OpenCode approval pending"
                     : SessionBridge_isBusy(self) ? "OpenCode pending" : "OpenCode idle");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated)
        *outTruncated = cut;
    return !cut;
}
bool SessionBridge_toStringStruct(const SessionBridge *self, char *dest, size_t cap, bool *outTruncated) {
    if (!self)
        return SessionBridge_toString(self, dest, cap, outTruncated);
    if (outTruncated)
        *outTruncated = false;
    if (!dest || !cap)
        return false;
    int n = snprintf(dest, cap, "{harness:borrowed-driver,readFd:%d,writeFd:%d,owo:%d,jobId:%u,startedMs:%llu,request:redacted,requestLength:%zu,requestOffset:%zu,response:redacted,responseLength:%zu,text:redacted,failed:%d,awaiting:%d,approvalSerial:%u,approvalId:redacted,approvalLabel:redacted,note:redacted,notePending:%d,decision:redacted}",
                     (*self).readFd, (*self).writeFd, (*self).owo, (*self).jobId,
                     (unsigned long long) (*self).startedMs, (*self).requestLength,
                     (*self).requestOffset, (*self).responseLength, (*self).failed,
                     (*self).awaiting, (*self).approvalSerial, (*self).notePending);
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated)
        *outTruncated = cut;
    return !cut;
}
