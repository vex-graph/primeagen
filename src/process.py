"""Bounded POSIX child seam. Literal argv, simultaneous capped pipe IO.

Deadline covers the whole job, including inherited pipes. On failure/cancel,
attempt process-group cancellation and reap the direct child before releasing
owned pipes. Refused group signalling is surfaced, not descendant-stop proof. Trusted external
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
stdout/stderr byte arrays. EOF closes each pipe; exceptional paths attempt group
kill and reap before releasing resources. Refused group cleanup is observable and
remains unresolved, not a claim that descendants stopped. Timeout covers inherited pipes,
not merely the first child exit. Cancel, output cap, diagnostics and nonzero exit
are explicit failures. No implicit shell, worker threads, trusted-output printing or claim
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
    child = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, start_new_session=True, cwd=cwd)
    try:
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
        except PermissionError:
            # Reap the direct child, but report unresolved group cleanup.
            try:
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired) as cleanup_error:
                raise ValueError(f"{error}; process-group cleanup refused; direct child cleanup unresolved") from cleanup_error
            raise ValueError(f"{error}; process-group cleanup refused") from error
        child.wait(timeout=5)
        if isinstance(error, subprocess.TimeoutExpired):
            raise ValueError("child deadline exceeded") from error
        raise
    finally:
        # Do not use Popen.__exit__: its implicit wait is unbounded if the host
        # refuses cancellation/reap. The explicit paths above own bounded waits;
        # unresolved refusal returns loudly instead of hanging on a context exit.
        for stream in (child.stdin, child.stdout, child.stderr):
            if stream is not None:
                stream.close()
