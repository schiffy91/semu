"""Module-level mutable state shared across core operations.

Lives in its own module so other modules can import a single source of truth
without circular dependencies.
"""

import sys

PLATFORM = {"win32": "windows", "darwin": "macos", "linux": "linux"}.get(sys.platform, "linux")

DRY_RUN = False
NUM_ERRORS = 0
HOST = ""
PORTABLE = ""


def reset_errors():
    global NUM_ERRORS
    NUM_ERRORS = 0


def add_error():
    global NUM_ERRORS
    NUM_ERRORS += 1
