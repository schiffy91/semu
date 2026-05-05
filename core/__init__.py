"""Schemulator core: shared library used by both the CLI (setup.py) and the GUI."""

from core import state
from core.console import console_error, console_log
from core.exec import execute, DRY_RUN
from core.symlinks import (
    BACKUP_EXCLUDE,
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
    "BACKUP_EXCLUDE",
    "DRY_RUN",
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
