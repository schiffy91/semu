"""Process-level file locking for lifecycle commands.

Prevents two GUI windows (or a GUI + a CLI run) from racing on the
result-<emu> symlink rotation. Without this, both processes can do
`os.rename(old, prev)` and clobber each other's prev-link, destroying the
rollback target. Round-8 critic finding H1.

Uses fcntl.flock on Linux/macOS — POSIX advisory lock that's released
automatically when the file descriptor closes (process exit / crash).
On Windows the implementation degrades to a best-effort lockfile +
PID check, since fcntl isn't available.
"""

import errno
import os
import sys
import time
from contextlib import contextmanager
from typing import Optional


def _lock_path(project_dir: str) -> str:
    return os.path.join(project_dir, ".schemulator.lock")


@contextmanager
def project_lock(project_dir: str, wait: float = 0.0):
    """Acquire an exclusive lock on `<project_dir>/.schemulator.lock`.

    `wait`:
      0.0 (default): non-blocking; raise BusyError if held by another process.
      >0:           block up to `wait` seconds before giving up.

    Use as a context manager:
        with project_lock(project_dir):
            ... lifecycle ops ...

    On exit (success, exception, or interpreter shutdown) the OS releases
    the lock automatically by closing the fd.
    """
    os.makedirs(project_dir, exist_ok=True)
    lock_file = _lock_path(project_dir)

    if sys.platform == "win32":
        # Best-effort: write our PID; refuse if another live PID owns it.
        if os.path.exists(lock_file):
            try:
                with open(lock_file) as f:
                    pid = int(f.read().strip())
                if _pid_alive(pid):
                    raise BusyError(
                        f"Another schemulator process is running (pid {pid}). "
                        f"Wait for it to finish, or remove {lock_file} if stale."
                    )
            except (ValueError, OSError):
                pass
        try:
            with open(lock_file, "w") as f:
                f.write(str(os.getpid()))
        except OSError as e:
            raise BusyError(f"Couldn't acquire lock at {lock_file}: {e}") from e
        try:
            yield lock_file
        finally:
            try:
                os.remove(lock_file)
            except OSError:
                pass
        return

    # POSIX: fcntl.flock with optional retry.
    import fcntl
    fd = os.open(lock_file, os.O_RDWR | os.O_CREAT, 0o644)
    deadline = time.monotonic() + wait if wait > 0 else None
    try:
        while True:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except OSError as e:
                if e.errno not in (errno.EWOULDBLOCK, errno.EAGAIN):
                    os.close(fd)
                    raise
                if deadline is None or time.monotonic() > deadline:
                    os.close(fd)
                    raise BusyError(
                        f"Another schemulator process is running. "
                        f"Wait for it to finish, or check {lock_file}."
                    )
                time.sleep(0.2)
        # Record our PID for diagnostic visibility.
        try:
            os.ftruncate(fd, 0)
            os.write(fd, str(os.getpid()).encode())
        except OSError:
            pass
        yield lock_file
    finally:
        try:
            fcntl.flock(fd, fcntl.LOCK_UN)
        except OSError:
            pass
        try:
            os.close(fd)
        except OSError:
            pass


class BusyError(RuntimeError):
    """Raised when another schemulator process holds the project lock."""


def _pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except OSError as e:
        return e.errno == errno.EPERM
    return True
