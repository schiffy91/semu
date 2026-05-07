"""Linux-only Flatpak helpers.

Uses `flatpak --user` exclusively so we never need to prompt for sudo from a
worker thread (which has no TTY and would hang). User-scope overrides go to
`~/.local/share/flatpak/overrides/<id>` and survive logout but not other
users on the machine — appropriate for a single-user emulator install.
"""

import shutil

from core import state
from core.exec import execute


def is_available():
    return state.PLATFORM == "linux" and bool(shutil.which("flatpak"))


def setup_flatpak(flatpak_id, path):
    """Install the flatpak (user scope) and grant filesystem access to `path`."""
    if state.PLATFORM != "linux" or not flatpak_id or not is_available():
        return
    result = execute("run", ["flatpak", "list", "--user", "--app"])
    if result and flatpak_id not in result.stdout:
        execute("install", ["flatpak", "install", "--user", "-y", flatpak_id])
    execute("run", ["flatpak", "override", "--user", flatpak_id, f"--filesystem={path}"])
    if state.PORTABLE:
        execute("run", ["flatpak", "override", "--user", flatpak_id, f"--filesystem={state.PORTABLE}"])


def remove_overrides(flatpak_id):
    """Reset all user-scope filesystem overrides for `flatpak_id`."""
    if state.PLATFORM != "linux" or not flatpak_id or not is_available():
        return
    execute("run", ["flatpak", "override", "--user", "--reset", flatpak_id])


def uninstall(flatpak_id):
    if state.PLATFORM != "linux" or not flatpak_id or not is_available():
        return
    execute("install", ["flatpak", "uninstall", "--user", "-y", flatpak_id])


def sandbox_dir(flatpak_id: str) -> str:
    """The path Flatpak uses for an app's sandboxed user data:
    ~/.var/app/<id>/. Used to retarget symlinks when an emulator is installed
    via Flatpak instead of natively."""
    import os
    return os.path.expanduser(f"~/.var/app/{flatpak_id}")

