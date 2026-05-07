"""Structured logging for schemulator.

Two channels:
  - Console (stdout): what the CLI prints and what the GUI's progress dialog
    streams. Format: bare lines, no timestamps, so it doesn't clutter
    interactive output.
  - File: ~/.cache/schemulator/schemulator.log, rotated at 1MB, 3 backups.
    Includes timestamps, levels, module names. Useful for "I clicked things
    in the GUI and something broke, attach this log."

We expose a single logger instance via `get_logger()`; modules call
`get_logger().info("message")`. The legacy `console_log` / `console_error`
helpers in core.console are wrapped to also emit through this logger so
callers don't have to migrate all at once.
"""

import logging
import logging.handlers
import os
from typing import Optional


_LOGGER: Optional[logging.Logger] = None


def _log_dir() -> str:
    base = os.environ.get("XDG_CACHE_HOME") or os.path.expanduser("~/.cache")
    return os.path.join(base, "schemulator")


def get_logger() -> logging.Logger:
    """Return the shared schemulator logger. Initialises on first call."""
    global _LOGGER
    if _LOGGER is not None:
        return _LOGGER

    log = logging.getLogger("schemulator")
    log.setLevel(logging.DEBUG)
    log.propagate = False

    # File handler: timestamps + level
    try:
        os.makedirs(_log_dir(), exist_ok=True)
        path = os.path.join(_log_dir(), "schemulator.log")
        fh = logging.handlers.RotatingFileHandler(
            path, maxBytes=1_000_000, backupCount=3, encoding="utf-8",
        )
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(logging.Formatter(
            "%(asctime)s %(levelname)-7s %(name)s: %(message)s",
            datefmt="%Y-%m-%dT%H:%M:%S",
        ))
        log.addHandler(fh)
    except OSError:
        # Logging shouldn't crash the app even if the cache dir is unwritable.
        pass

    _LOGGER = log
    return log


def log_path() -> str:
    return os.path.join(_log_dir(), "schemulator.log")
