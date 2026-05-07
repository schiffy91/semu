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


def setup_flatpak(flatpak_id, path, project_dir=None):
    """Install the flatpak (user scope) and grant filesystem access to `path`.

    Security gate (round-5 critic finding #3): refuse to grant `--filesystem=`
    for any path that resolves outside `project_dir`. Without this a peer
    who can write into a synced symlinks.json could redirect a config entry
    to /home/<user>/Documents and silently grant the Flatpak'd emulator full
    Documents access on next install run.

    For backwards compat, `project_dir=None` skips the check (legacy callers
    that already validated upstream). All in-tree call sites have been
    updated to pass project_dir.
    """
    if state.PLATFORM != "linux" or not flatpak_id or not is_available():
        return

    if project_dir and not _path_under(path, project_dir):
        from core.console import console_error
        console_error(
            f"Refusing flatpak override --filesystem={path}: "
            f"path is outside the project dir ({project_dir})."
        )
        return

    result = execute("run", ["flatpak", "list", "--user", "--app"])
    if result and flatpak_id not in result.stdout:
        # `flatpak install` can hang indefinitely if flatpakd is wedged or
        # flathub is slow. 10 minutes is generous for a real download but
        # cleanly bounds a stuck process so the GUI worker doesn't hang
        # forever (round-8 critic finding H7).
        import subprocess
        try:
            subprocess.run(
                ["flatpak", "install", "--user", "-y", flatpak_id],
                check=False, timeout=600,
            )
        except subprocess.TimeoutExpired:
            from core.console import console_error
            console_error(
                f"flatpak install of {flatpak_id} timed out after 10m. "
                f"Check `flatpak install --user {flatpak_id}` manually."
            )
            return
    execute("run", ["flatpak", "override", "--user", flatpak_id, f"--filesystem={path}"])
    if state.PORTABLE and (not project_dir or _path_under(state.PORTABLE, project_dir)):
        execute("run", ["flatpak", "override", "--user", flatpak_id, f"--filesystem={state.PORTABLE}"])


def _path_under(child: str, parent: str) -> bool:
    import os
    try:
        c = os.path.realpath(child)
        p = os.path.realpath(parent)
        return os.path.commonpath([c, p]) == p
    except (OSError, ValueError):
        return False


def remove_overrides(flatpak_id):
    """Reset all user-scope filesystem overrides for `flatpak_id`."""
    if state.PLATFORM != "linux" or not flatpak_id or not is_available():
        return
    execute("run", ["flatpak", "override", "--user", "--reset", flatpak_id])



