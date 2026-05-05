"""QThread workers wrapping core.* operations.

Each worker takes a callable + args, runs it on a background thread, and
re-emits stdout/stderr to a `progress` signal so the GUI's log panel can
stream output identically to what the CLI prints.
"""

from __future__ import annotations

import argparse
import contextlib
import io
import sys
import traceback
from typing import Callable

from PySide6.QtCore import QThread, Signal


class _StreamRouter(io.TextIOBase):
    """Routes writes back into a Qt signal."""

    def __init__(self, emit: Callable[[str], None]):
        super().__init__()
        self._emit = emit

    def writable(self):
        return True

    def write(self, s):
        if s:
            self._emit(s)
        return len(s)

    def flush(self):
        pass


class CoreWorker(QThread):
    """Generic worker. Pass any `core.*` function and an `argparse.Namespace`."""

    progress = Signal(str)
    finished_ok = Signal(bool)

    def __init__(self, fn: Callable, args: argparse.Namespace, parent=None):
        super().__init__(parent)
        self._fn = fn
        self._args = args

    def run(self):
        router = _StreamRouter(lambda s: self.progress.emit(s))
        ok = True
        with contextlib.redirect_stdout(router), contextlib.redirect_stderr(router):
            try:
                self._fn(self._args)
            except Exception:
                traceback.print_exc()
                ok = False
        self.finished_ok.emit(ok)


def make_args(**kw) -> argparse.Namespace:
    """Convenience for building the Namespace `core.*` functions expect."""
    return argparse.Namespace(**kw)
