"""Model binding over real API Haven envelopes and host-owned transport.

This first slice asks for one strict JSON text action, not vendor-native tool
calls. History and tool policy belong to the app; messages remain ordinary text.
No provider request or credential use occurs at construction or build time.
"""
"""
============================================================================
CLASS: Provider (src/provider.py)
============================================================================
INSTANCE FIELDS (in construction order):
----------------------------------------------------------------------------
  Provider {
    family;       // openai, anthropic, or explicit loopback local mode
    model;        // validated host-selected model identifier
    base;         // validated explicit/default URL without credential/query
    timeout;      // caller-selected whole-child deadline in seconds
    bridge;       // borrowed native binding executable path
  }

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Constructors:
  - Provider(family, model, base, timeout, bridge)
Core operations:
  - render(transcript) : real Harness admission + API Haven request envelope
  - complete(transcript) : isolated host transport and response-text extraction

Configuration is immutable for an admitted binding; construct a replacement to
change providers rather than mutate an in-flight request. Missing credentials,
binding, malformed response, timeout and transport errors fail observably. No
call at construction; no live proof from successful render. No streaming/retry,
vendor-native tools, network ownership in the C binding, or key serialization.
============================================================================
"""
import ipaddress
import json
import os
from pathlib import Path
import sys
from urllib.parse import urlsplit

from process import run

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BASES = {"openai": "https://api.openai.com/v1", "anthropic": "https://api.anthropic.com", "local": "http://127.0.0.1:11434/v1"}


class Provider:
    """Immutable configuration; bounded complete() returns text, never executes it."""
    def __init__(self, family, model, base=None, timeout=60.0, bridge=None):
        if family not in DEFAULT_BASES or not model or any(ord(c) < 32 for c in model) or len(model) > 256:
            raise ValueError("invalid provider or model")
        self.family, self.model = family, model
        self.base = (base or DEFAULT_BASES[family]).rstrip("/")
        parsed = urlsplit(self.base)
        if parsed.username or parsed.password or parsed.query or parsed.fragment or not parsed.hostname:
            raise ValueError("endpoint must have a host and no credentials/query/fragment")
        if any(ord(c) <= 32 for c in self.base):
            raise ValueError("invalid endpoint bytes")
        if family == "local":
            local = parsed.hostname == "localhost"
            try:
                local = local or ipaddress.ip_address(parsed.hostname).is_loopback
            except ValueError:
                pass
            if not local or parsed.scheme not in ("http", "https"):
                raise ValueError("local backend requires an explicit loopback endpoint")
        elif parsed.scheme != "https":
            raise ValueError("hosted providers require verified HTTPS")
        if not 0 < timeout <= 3600:
            raise ValueError("invalid provider deadline")
        # Existing API Haven Anthropic builder appends /v1/messages itself.
        if family == "anthropic" and parsed.path.endswith("/v1"):
            raise ValueError("Anthropic base must omit the trailing /v1")
        self.timeout = timeout
        self.bridge = Path(bridge or os.environ.get("PRIMEAGEN_REQUEST", ROOT / ".build/request"))

    def render(self, transcript):
        """R4 Message admits borrowed text; API Haven renders the wire envelope."""
        if not self.bridge.is_file():
            raise ValueError("native request binding missing; run python3 build.py")
        wire = "anthropic" if self.family == "anthropic" else "openai"
        data = run([str(self.bridge), wire, self.model, self.base],
                   data=transcript.encode(), timeout=self.timeout, cap=8 * 1024 * 1024)
        url, body = data.decode().split("\n", 1)
        return url, body

    def complete(self, transcript):
        """One explicit network call; strict response shape, no retries or streams."""
        variable = "ANTHROPIC_API_KEY" if self.family == "anthropic" else "OPENAI_API_KEY"
        if self.family != "local" and not os.environ.get(variable):
            raise ValueError(f"missing {variable}")
        url, body = self.render(transcript)
        payload = json.dumps({"family": self.family, "url": url, "body": body, "timeout": self.timeout}).encode()
        data = run([sys.executable, str(ROOT / "src/transport.py")], data=payload, timeout=self.timeout)
        response = json.loads(data)
        if self.family == "anthropic":
            parts = response["content"]
            if not isinstance(parts, list) or not parts or any(p.get("type") != "text" for p in parts):
                raise ValueError("unsupported Anthropic response")
            text = "".join(part["text"] for part in parts)
        else:
            text = response["choices"][0]["message"]["content"]
        if not isinstance(text, str) or not text:
            raise ValueError("provider returned no text")
        return text
