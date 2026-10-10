"""Cold JSON admission shared by model actions and provider responses.

============================================================================
MODULE: Primeagen bounded JSON admission (src/json_input.py)
============================================================================
No persistent class, foreign fields or runtime interpreter is owned here.

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Public admission:
  - loads(text, max_depth) : scan structural depth outside escaped JSON strings;
                            decode with duplicate-key rejection
Private decode callback:
  - local pairs(items)    : reject duplicate object member names

Input is a live bounded text/byte value supplied by the host's capped pipes.
The named depth safety bound is independent of Python's decoder/recursion limit.
Malformed encodings/syntax, duplicate names and excessive nesting reject with
ValueError; no caller state is mutated. Strings containing brackets/escaped
quotes do not contribute structural depth. No concurrency or resource retention.
============================================================================
"""
import json

MAX_DEPTH = 32  # Explicit untrusted protocol safety bound, not a collection ceiling.


def loads(text, max_depth=MAX_DEPTH):
    if not isinstance(text, (str, bytes)) or type(max_depth) is not int or max_depth <= 0:
        raise ValueError("invalid JSON admission")
    if isinstance(text, bytes):
        try:
            text = text.decode("utf-8")
        except UnicodeError as error:
            raise ValueError("invalid JSON encoding") from error
    depth = 0
    quoted = False
    escaped = False
    for char in text:
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char in "[{":
            depth += 1
            if depth > max_depth:
                raise ValueError("JSON nesting safety limit exceeded")
        elif char in "]}":
            depth -= 1
            if depth < 0:
                raise ValueError("invalid JSON nesting")

    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("duplicate JSON field")
            result[key] = value
        return result

    try:
        return json.loads(text, object_pairs_hook=pairs)
    except RecursionError as error:
        raise ValueError("JSON nesting exceeded decoder budget") from error
