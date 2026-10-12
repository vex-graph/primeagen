#include "tools/websearch.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <unistd.h>
;;DEFINITION
/* Provider-neutral search delegates to one explicitly configured trusted search
 * executable. It receives query as one literal argument, not a shell fragment.
 * This consumer adapter implements no scraper, transport, auth or vendor API. */
;;OVERVIEW
/* ============================================================================
 * MODULE: websearch tool (tools/websearch.c)
 * OWNED STATE: none. Provider/query borrowed until exec transfers child lifetime.
 * FUNCTION REGISTRY: bool ToolWebsearch_execute(const char *provider,const char *query)
 * CONTRACT: absolute host-selected provider program and nonempty query; success
 * never returns. Host approves each search and supervises deadline/output/cleanup.
 * Missing provider rejects loudly. Provider must use authorized APIs; no implicit
 * install, guessed endpoint, HTML scraping or live search proof from fake programs.
 * No credentials copied; configured executable inherits host environment.
 * ============================================================================ */
bool ToolWebsearch_execute(const char *provider, const char *query) {
    if (provider != nullptr && *provider == '/' && query != nullptr && *query != '\0')
        execl(provider, provider, query, (char*) nullptr);
    THROW("websearch: configured absolute provider and nonempty query required or exec failed");
    return false;
}
