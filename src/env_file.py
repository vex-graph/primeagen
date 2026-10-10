"""Cold local credential admission, not a shell or general dotenv interpreter.

============================================================================
MODULE: Primeagen private key file (src/env_file.py)
============================================================================
OWNED STATE:
  No persistent class. One local descriptor and temporary copied key mapping are
  owned during admission; valid keys publish to the caller's environment only
  after every line validates. The caller owns the process environment lifetime.

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Public admission:
  - load_env(path, environ=None)
      Missing file is a no-op. Open a non-symlink regular file; bounded UTF-8 read;
      parse only OPENAI_API_KEY and ANTHROPIC_API_KEY; reject duplicates, malformed
      assignments, unknown names, controls and oversized input before publishing.
      Existing environment entries (including empty entries) take precedence.

CONTRACT:
  Optional single/double quotes delimit one literal value; no multiline values,
  export statements, interpolation, backslash decoding or inline comments. Empty
  values are valid placeholders. Blank lines and full-line comments are ignored.
  The byte cap is an explicit credential-file safety bound, not a history limit.
  Error text never includes file contents, values or offending line text. File IO
  is cold; no realtime bound, hostile-parent sandbox, encryption, key erasure or
  concurrent environment publication guarantee is claimed. Unix no-follow and
  nonblocking open reject symlinks and prevent FIFO admission from hanging.
  Chat alone calls this module: builds, doctor and offline TUI do not load keys.
============================================================================
"""
import os
import stat

ENV_FILE_BYTES_MAX = 16 * 1024  # Explicit maximum for this two-credential format.
KEY_NAMES = frozenset(("OPENAI_API_KEY", "ANTHROPIC_API_KEY"))


def load_env(path, environ=None):
    """Admit all values before changing environment; diagnostics never echo keys."""
    target = os.environ if environ is None else environ
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    except FileNotFoundError:
        return
    except OSError:
        raise ValueError("cannot open private .env key file") from None
    try:
        with os.fdopen(descriptor, "rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise ValueError("private .env must be a regular file")
            data = stream.read(ENV_FILE_BYTES_MAX + 1)
    except OSError:
        raise ValueError("cannot read private .env key file") from None
    if len(data) > ENV_FILE_BYTES_MAX:
        raise ValueError("private .env exceeds safety size limit")
    try:
        text = data.decode("utf-8")
    except UnicodeError:
        raise ValueError("private .env has invalid encoding") from None
    pending = {}
    for line in text.split("\n"):
        # Permit CRLF file endings, but not controls inside values.
        line = line.removesuffix("\r").strip(" \t")
        if not line or line.startswith("#"):
            continue
        name, separator, value = line.partition("=")
        name = name.strip(" \t")
        value = value.strip(" \t")
        if not separator or name not in KEY_NAMES or name in pending:
            raise ValueError("private .env has invalid or duplicate key assignment")
        if value.startswith(("'", '"')):
            if len(value) < 2 or value[-1] != value[0]:
                raise ValueError("private .env has invalid value quotes")
            value = value[1:-1]
        if any(ord(c) < 33 or ord(c) > 126 or c in "\"'" for c in value):
            raise ValueError("private .env key values must be literal printable tokens")
        pending[name] = value
    target.update({name: value for name, value in pending.items() if name not in target})
