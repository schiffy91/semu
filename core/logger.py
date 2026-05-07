"""Structured logging for schemulator.

Two channels:
  - Console (stdout): what the CLI prints and what the GUI's progress dialog
    streams. Bare lines, no timestamps.
  - File: ~/.cache/schemulator/schemulator.log, rotated at 1MB, 3 backups.
    Timestamped + level-tagged. PII (home dir paths, syncthing device IDs)
    is scrubbed before write so users can attach the log to bug reports
    without leaking credentials (critic finding #28).
"""

import logging
import logging.handlers
import os
import re
from typing import Optional


_LOGGER: Optional[logging.Logger] = None

# Regex for likely PII to scrub on write.
_HOME_RE = re.compile(re.escape(os.path.expanduser("~")))
_USER_RE = re.compile(r"/(?:home|Users)/([^/\s]+)")
_DEVID_RE = re.compile(r"\b(?:[A-Z0-9]{7}-){7}[A-Z0-9]{7}\b")


class _PIIScrubFormatter(logging.Formatter):
    def format(self, record: logging.LogRecord) -> str:
        msg = super().format(record)
        msg = _HOME_RE.sub("<HOME>", msg)
        msg = _USER_RE.sub(r"/Users/<USER>", msg)
        msg = _DEVID_RE.sub("<DEVICE-ID>", msg)
        return msg


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

    try:
        os.makedirs(_log_dir(), exist_ok=True)
        path = os.path.join(_log_dir(), "schemulator.log")
        fh = logging.handlers.RotatingFileHandler(
            path, maxBytes=1_000_000, backupCount=3, encoding="utf-8",
        )
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(_PIIScrubFormatter(
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
