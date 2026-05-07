"""Schemulator core: shared library used by both the CLI (setup.py) and the GUI.

Modules are imported directly (e.g. `from core import lifecycle`) rather than
through this package's `__all__`. The few names re-exported here are the ones
setup.py's tests historically reach through, kept stable for back-compat.
"""

from core import state
from core.console import console_error, console_log
from core.exec import execute
from core.symlinks import (
    create_symlink,
    create_symlinks,
    parse_config,
    resolve_config,
)
from core.flatpak import setup_flatpak
from core.backup import (
    cmd_backup,
    cmd_capture,
    cmd_migrate,
    cmd_originals,
    cmd_revert,
)
from core.lifecycle import (
    install,
    rollback,
    uninstall,
    update,
)

PLATFORM = state.PLATFORM

__all__ = [
    "PLATFORM",
    "console_error",
    "console_log",
    "create_symlink",
    "create_symlinks",
    "execute",
    "parse_config",
    "resolve_config",
    "setup_flatpak",
    "cmd_backup",
    "cmd_capture",
    "cmd_migrate",
    "cmd_originals",
    "cmd_revert",
    "install",
    "rollback",
    "uninstall",
    "update",
]
