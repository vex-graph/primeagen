"""Bounded POSIX child seam. Literal argv, simultaneous capped pipe IO.

Deadline covers the whole job, including inherited pipes. On failure/cancel,
kill the process group and reap before releasing owned pipes. Trusted external
programs are not sandboxed; detached descendants are outside this contract.
"""
"""
============================================================================
MODULE: Primeagen bounded child seam (src/process.py)
============================================================================
No public class or persistent fields are owned by this procedural module.

FUNCTION REGISTRY:
----------------------------------------------------------------------------
Core operations:
  - run(argv, data, timeout, cap, cwd)
      Validate literal argv and budgets; concurrently feed stdin/drain both
      output pipes; return stdout only after quiet exit zero and bounded reap.

Each invocation owns one process group, three pipes, a selector and bounded
stdout/stderr byte arrays. EOF closes each pipe; every exceptional path kills
the group and reaps before releasing resources. Timeout covers inherited pipes,
not merely the first child exit. Cancel, output cap, diagnostics and nonzero exit
are explicit failures. No shell, worker threads, trusted-output printing or claim
to contain malicious detached descendants. POSIX host scope only.
============================================================================
"""
import os
import selectors
import signal
import subprocess
import time
import math

DEFAULT_TIMEOUT = 60.0
DEFAULT_OUTPUT_CAP = 1024 * 1024


def run(argv, *, data=b"", timeout=DEFAULT_TIMEOUT, cap=DEFAULT_OUTPUT_CAP, cwd=None):
    """Return stdout bytes only on exit zero; reject timeout/overflow/nonzero."""
    if not argv or not all(isinstance(item, str) and item and "\0" not in item for item in argv):
        raise ValueError("invalid literal argument vector")
    if not math.isfinite(timeout) or timeout <= 0 or cap <= 0:
        raise ValueError("invalid child budget")
    if not isinstance(data, bytes) or len(data) > DEFAULT_OUTPUT_CAP:
        raise ValueError("child input exceeds safety limit")
    with subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, start_new_session=True, cwd=cwd) as child:
        with selectors.DefaultSelector() as selector:
            for stream in (child.stdin, child.stdout, child.stderr):
                os.set_blocking(stream.fileno(), False)
            output, error = bytearray(), bytearray()
            selector.register(child.stdout, selectors.EVENT_READ, output)
            selector.register(child.stderr, selectors.EVENT_READ, error)
            if data:
                selector.register(child.stdin, selectors.EVENT_WRITE, None)
            else:
                child.stdin.close()
            offset = 0
            deadline = time.monotonic() + timeout
            try:
                while selector.get_map():
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise ValueError("child deadline exceeded")
                    for key, events in selector.select(min(remaining, 0.1)):
                        stream = key.fileobj
                        if events & selectors.EVENT_WRITE:
                            try:
                                offset += os.write(stream.fileno(), data[offset:offset + 65536])
                            except BrokenPipeError:
                                offset = len(data)
                            if offset == len(data):
                                selector.unregister(stream)
                                stream.close()
                        else:
                            block = os.read(stream.fileno(), 65536)
                            if not block:
                                selector.unregister(stream)
                                stream.close()
                            else:
                                if len(key.data) + len(block) > cap:
                                    raise ValueError("child output truncated: safety limit exceeded")
                                key.data.extend(block)
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise ValueError("child deadline exceeded")
                code = child.wait(timeout=remaining)
                if code:
                    # Untrusted errors may contain terminal escapes or secrets.
                    raise ValueError(f"child failed (exit {code})")
                if error:
                    raise ValueError("child emitted unexpected diagnostics")
                return bytes(output)
            except BaseException as error:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.wait(timeout=5)
                if isinstance(error, subprocess.TimeoutExpired):
                    raise ValueError("child deadline exceeded") from error
                raise
