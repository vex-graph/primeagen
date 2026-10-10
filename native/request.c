#include "ai/ai_chat.h"
#include "ai/ai_chat_anthropic.h"
#include "space/message.h"
#include "personality/owo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

;;DEFINITION
/* This procedural R5 binding consumes real R4 Message admission and API Haven
 * request builders. It renders only: no network, credentials, vendor SDK,
 * implicit tool execution or competing agent core lives in this process.
 * Python owns the retained transcript, transport supervision and tool consent. */
;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Primeagen native request binding (native/request.c)
 * ============================================================================
 * This procedural client owns no public class, fields or private record type.
 * Harness Message and API Haven chat objects remain their owners' classes;
 * no foreign fields are presented as this module's STRUCT FIELDS.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public CLI entry:
 *   - main(argc, argv)
 *       request openai|anthropic model base [--owo] < transcript
 *       Emits exactly URL + newline + request JSON on successful admission.
 *       Optional Owo_instructions prefixes host style guidance to user content;
 *       this is not a vendor system-role priority or model-compliance claim.
 *
 * Consumed public seams (not declarations owned by this module):
 *   - Message(...) / Message_getText(message)
 *       R4 borrowed text admission; local event never escapes this invocation
 *   - AiChat_1 / AiChat_setBaseUrl / AiChat_buildRequest
 *       API Haven OpenAI-compatible envelope into caller-owned storage
 *   - AiChatAnthropic_1 / setBaseUrl / buildRequest
 *       API Haven Anthropic envelope; base omits the appended /v1/messages
 *
 * OWNERSHIP / FAILURE CONTRACT:
 * ----------------------------------------------------------------------------
 *   main owns input/body buffers and reclaims both after Message detaches.
 *   The Python host retains history; this binding retains no session state.
 *   Empty/NUL/oversized text and invalid families reject loudly and exit nonzero.
 *   Allocation/builder failure emits no successful request. Model/base are
 *   trusted host configuration, validated before child launch by Provider.
 *   INPUT_CAP is a fixed external-input safety ceiling; worst-case JSON escaping
 *   reserves six bytes per input byte plus named envelope space. No credentials,
 *   network, implicit tools, workers or hot-path guarantees are offered.
 * ============================================================================
 */

#define INPUT_CAP (1024u * 1024u)
#define ENVELOPE_CAP 8192u

/* Own a bounded transcript through the duration of request rendering. */
int main(int argc, char **argv) {
    if ((argc != 4 && argc != 5) || (argc == 5 && strcmp(argv[4], "--owo") != 0) || (strcmp(argv[1], "openai") != 0 && strcmp(argv[1], "anthropic") != 0)) {
        THROW("primeagen request: invalid arguments");
        return 1;
    }
    const char *instructions = Owo_instructions(argc == 5);
    size_t prefix = strlen(instructions);
    char *input = malloc(INPUT_CAP + prefix + 1);
    size_t bodyCap = INPUT_CAP * 6u + ENVELOPE_CAP;
    char *body = malloc(bodyCap);
    if (input == nullptr || body == nullptr) {
        free(input);
        free(body);
        THROW("primeagen request: allocation failed");
        return 1;
    }
    memcpy(input, instructions, prefix);
    size_t length = fread(input + prefix, 1, INPUT_CAP, stdin);
    bool ok = length != 0 && !ferror(stdin) && fgetc(stdin) == EOF && memchr(input + prefix, 0, length) == nullptr;
    length += prefix;
    input[length] = '\0';
    if (!ok)
        THROW("primeagen request: invalid transcript");
    char url[ENVELOPE_CAP];
    ApiAuth auth;
    if (ok) {
        Message message = Message(1, 1, 0, 0, input, length);
        AiMessage turn = {"user", Message_getText(&message)};
        if (strcmp(argv[1], "anthropic") == 0) {
            AiChatAnthropic chat = AiChatAnthropic_1(argv[2]);
            AiChatAnthropic_setBaseUrl(&chat, argv[3]);
            ok = AiChatAnthropic_buildRequest(&chat, &turn, 1, body, bodyCap, url, sizeof(url), &auth);
        } else {
            AiChat chat = AiChat_1(argv[2]);
            AiChat_setBaseUrl(&chat, argv[3]);
            ok = AiChat_buildRequest(&chat, &turn, 1, body, bodyCap, url, sizeof(url), &auth);
        }
        if (!ok)
            THROW("primeagen request: builder rejected input");
    }
    if (ok)
        ok = printf("%s\n%s", url, body) >= 0 && !ferror(stdout);
    free(body);
    free(input);
    return ok ? 0 : 1;
}
