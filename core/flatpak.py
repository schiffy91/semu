"""Linux-only Flatpak helpers."""

import shutil

from core import state
from core.exec import execute


def is_available():
    return state.PLATFORM == "linux" and bool(shutil.which("flatpak"))


def setup_flatpak(flatpak_id, path):
    """Install the flatpak if missing and grant filesystem access to `path`."""
    if state.PLATFORM != "linux" or not flatpak_id or not is_available():
        return
    result = execute("run", ["flatpak", "list", "--app"])
    if result and flatpak_id not in result.stdout:
        execute("install", ["flatpak", "install", "-y", flatpak_id])
    execute("run", ["sudo", "flatpak", "override", flatpak_id, f"--filesystem={path}"])
    execute("run", ["sudo", "flatpak", "override", flatpak_id, f"--filesystem={state.PORTABLE}"])


def remove_overrides(flatpak_id):
    """Reset all filesystem overrides for `flatpak_id` to flatpak defaults."""
    if state.PLATFORM != "linux" or not flatpak_id or not is_available():
        return
    execute("run", ["sudo", "flatpak", "override", "--reset", flatpak_id])


def uninstall(flatpak_id):
    if state.PLATFORM != "linux" or not flatpak_id or not is_available():
        return
    execute("install", ["flatpak", "uninstall", "-y", flatpak_id])
