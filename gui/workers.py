"""QThread workers wrapping core.* operations.

Each worker takes a callable + args, runs it on a background thread. Output
is captured via a logging handler attached to the schemulator logger (NOT
contextlib.redirect_stdout, which would rebind sys.stdout process-wide and
interleave with other Qt threads — see critic finding #21). Worker prints to
the console via the same logger, so the GUI's progress dialog mirrors what
the CLI shows.
"""

from __future__ import annotations

import argparse
import logging
import re
import threading
import traceback
from typing import Callable

from PySide6.QtCore import QThread, Signal

from core.logger import get_logger


# Strip ANSI color escapes when forwarding to the GUI log panel (critic #22).
_ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


class _SignalLogHandler(logging.Handler):
    """A logging.Handler that forwards records to a Qt signal.

    We attach it to the schemulator logger ONLY for the duration of one
    worker run, scoped via a per-thread filter so other threads' log output
    isn't pulled into this worker's progress stream.
    """

    def __init__(self, emit_fn: Callable[[str], None], thread_id: int):
        super().__init__(level=logging.DEBUG)
        self._emit = emit_fn
        self._thread_id = thread_id

    def filter(self, record: logging.LogRecord) -> bool:
        return record.thread == self._thread_id

    def emit(self, record: logging.LogRecord):
        try:
            msg = _ANSI_RE.sub("", self.format(record))
            self._emit(msg + "\n")
        except Exception:
            self.handleError(record)


class CoreWorker(QThread):
    """Generic worker. Pass any `core.*` function and an `argparse.Namespace`."""

    progress = Signal(str)
    finished_ok = Signal(bool)

    def __init__(self, fn: Callable, args: argparse.Namespace, parent=None):
        super().__init__(parent)
        self._fn = fn
        self._args = args

    def run(self):
        log = get_logger()
        handler = _SignalLogHandler(
            emit_fn=lambda s: self.progress.emit(s),
            thread_id=threading.get_ident(),
        )
        handler.setFormatter(logging.Formatter("%(message)s"))
        log.addHandler(handler)
        ok = True
        try:
            self._fn(self._args)
        except Exception:
            log.error("Operation failed:\n%s", traceback.format_exc())
            ok = False
        finally:
            log.removeHandler(handler)
        self.finished_ok.emit(ok)


def make_args(**kw) -> argparse.Namespace:
    """Convenience for building the Namespace `core.*` functions expect."""
    return argparse.Namespace(**kw)
