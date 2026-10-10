"""Explicit host transport for a rendered API Haven request; isolated child.

TLS uses Python's verified default context. Redirects are refused, and only an
explicit local backend permits loopback cleartext. The parent enforces an overall
deadline/output cap; socket timeout alone is not claimed to bound a whole job.
"""
"""
============================================================================
CLASS: NoRedirect (src/transport.py)
============================================================================
INSTANCE FIELDS:
----------------------------------------------------------------------------
  No fields are added to urllib.request.HTTPRedirectHandler by this subclass.

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Overridden callback:
  - redirect_request(req, fp, code, msg, headers, newurl)
      Return None; every redirect rejects instead of forwarding credentials.
Procedural child entry (outside the class):
  - main() : read parent-rendered request; obtain key from environment; POST with
             verified TLS; reject HTTP errors/oversize; return bounded response

This is a trusted parent-only transport entry, not a public arbitrary-URL proxy.
Provider validates endpoint policy before launch. The parent's whole-job deadline
and capped pipes bound DNS/socket/response work. HTTP/non-HTTP failures emit only
safe diagnostics; untrusted bodies and credential values are never logged.
============================================================================
"""
import json
import os
import sys
import urllib.error
import urllib.request

RESPONSE_CAP = 1024 * 1024


class NoRedirect(urllib.request.HTTPRedirectHandler):
    """Never forward credentials to another origin, including 307/308."""
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def main():
    try:
        payload = json.loads(sys.stdin.buffer.read(RESPONSE_CAP + 1))
        family = payload["family"]
        key = os.environ.get("ANTHROPIC_API_KEY" if family == "anthropic" else "OPENAI_API_KEY", "")
        headers = {"Content-Type": "application/json"}
        if family == "anthropic":
            headers["anthropic-version"] = "2023-06-01"
            headers["x-api-key"] = key
        elif family != "local" and key:
            headers["Authorization"] = "Bearer " + key
        request = urllib.request.Request(payload["url"], data=payload["body"].encode(), headers=headers, method="POST")
        with urllib.request.build_opener(NoRedirect).open(request, timeout=payload["timeout"]) as response:
            data = response.read(RESPONSE_CAP + 1)
            if len(data) > RESPONSE_CAP:
                raise ValueError("response limit exceeded")
            sys.stdout.buffer.write(data)
        return 0
    except urllib.error.HTTPError as error:
        print(f"[vex] provider: HTTP {error.code}", file=sys.stderr)
    except Exception:
        # Neither credentials nor untrusted response/error text are logged.
        print("[vex] provider: transport or response failure", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
