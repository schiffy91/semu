"""Parse setup.json + per-emulator symlinks.json and materialise the symlinks.

This is the single source of truth for how project-dir paths map onto the OS's
config locations. The CLI and the GUI both call into here.
"""

import json
import os

from core import state
from core.console import console_error
from core.exec import delete, execute, own


BACKUP_EXCLUDE = {"ROMs", "downloaded_media", "n3ds-fixed", "n3ds-original"}


def _parent(path):
    return os.path.dirname(path)


def _create_parent_dirs(path):
    execute("makedirs", _parent(path))


def _link_has_user_data(link: str) -> bool:
    """If `link` is a real directory (not a symlink) with files inside, the
    user has accumulated data there directly. We must not silently delete it."""
    if not os.path.isdir(link) or os.path.islink(link):
        return False
    try:
        return any(True for _ in os.scandir(link))
    except OSError:
        return False


def create_symlink(link, source):
    if os.path.lexists(_parent(link)):
        own(_parent(link))
    if os.path.lexists(link):
        own(link)
    # CRITICAL: refuse to silently delete a populated real directory at the
    # link path. Users sometimes pre-populate ~/.config/<emu>/ with saves
    # before discovering schemulator; clobbering it would lose their data.
    if _link_has_user_data(link):
        from core.console import console_error
        console_error(
            f"Refusing to replace existing data at {link}\n"
            f"  Move its contents into {source} first, then re-run symlink.\n"
            f"  (This protects user data when migrating an existing install.)"
        )
        return
    delete(link)
    _create_parent_dirs(link)
    execute("symlink", source, link, target_is_directory=os.path.isdir(source))


def create_symlinks(link, source):
    if os.path.isdir(source):
        for item in os.listdir(source):
            create_symlink(os.path.join(link, item), os.path.join(source, item))
    else:
        _, file_name = os.path.split(source)
        create_symlink(os.path.join(link, file_name), source)


def _find_project_dir(config_path):
    if os.path.isabs(config_path):
        return os.path.dirname(config_path)
    return os.path.dirname(os.path.abspath(config_path)) or "."


def resolve_config(args):
    """Resolve the config file path and project directory from CLI args."""
    config_file = args.config or os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "setup.json"
    )
    config_file = os.path.abspath(config_file)
    project_dir = _find_project_dir(config_file)
    return config_file, project_dir


def _flatpak_installed(flatpak_id: str) -> bool:
    """Return True if `flatpak_id` is installed in the user-scope Flatpak DB.

    On non-Linux platforms returns False (Flatpak doesn't apply).
    """
    if state.PLATFORM != "linux" or not flatpak_id:
        return False
    import shutil, subprocess
    if not shutil.which("flatpak"):
        return False
    try:
        result = subprocess.run(
            ["flatpak", "list", "--user", "--app", "--columns=application"],
            capture_output=True, text=True, check=False, timeout=5,
        )
        return flatpak_id in (result.stdout or "")
    except (subprocess.SubprocessError, OSError):
        return False


def _flatpak_remap(link: str, flatpak_id: str) -> str:
    """When the user runs the Flatpak version of an emulator on Linux, the
    emulator only sees its sandboxed home (`~/.var/app/<id>/`), not the
    classic XDG paths. We retarget our symlink to live inside the sandbox so
    config syncing actually works.

    Maps:
      ~/.config/<x>     -> ~/.var/app/<id>/config/<x>
      ~/.local/share/<x> -> ~/.var/app/<id>/data/<x>
      ~/.cache/<x>      -> ~/.var/app/<id>/cache/<x>

    Anything else is left alone (will fail at runtime — caller's responsibility).
    """
    if state.PLATFORM != "linux" or not flatpak_id:
        return link
    home = os.path.expanduser("~")
    sandbox = os.path.join(home, ".var", "app", flatpak_id)
    mapping = (
        (os.path.join(home, ".config"),     os.path.join(sandbox, "config")),
        (os.path.join(home, ".local/share"), os.path.join(sandbox, "data")),
        (os.path.join(home, ".cache"),       os.path.join(sandbox, "cache")),
    )
    for native_prefix, sandbox_prefix in mapping:
        if link == native_prefix or link.startswith(native_prefix + os.sep):
            tail = link[len(native_prefix):].lstrip(os.sep)
            return os.path.join(sandbox_prefix, tail)
    return link


def parse_config(file_path, path):
    """Parse setup.json + per-emulator symlinks.json. Returns
    {EMULATOR_UPPER: [(flatpak_id, link, source), ...]}.

    On Linux, when the corresponding Flatpak is installed, link paths under
    standard XDG dirs are retargeted to the Flatpak's sandbox under
    ~/.var/app/<id>/. This is what makes the symlink visible to the emulator
    at runtime — without it the symlink is invisible inside the sandbox and
    config sync silently breaks (critic finding #26)."""
    state.reset_errors()

    with open(file_path) as f:
        config_data = json.load(f)

    if state.PLATFORM not in config_data.get("host", {}):
        console_error(f"Platform '{state.PLATFORM}' not found in config host paths")
        return {}
    if state.PLATFORM not in config_data.get("portable", {}):
        console_error(f"Platform '{state.PLATFORM}' not found in config portable paths")
        return {}

    state.HOST = os.path.expanduser(config_data["host"][state.PLATFORM])
    state.PORTABLE = os.path.expanduser(config_data["portable"][state.PLATFORM])

    if not os.path.isabs(state.HOST):
        console_error(f"host path is not absolute after expansion: {state.HOST!r}")
        return {}
    if not os.path.isabs(state.PORTABLE):
        console_error(f"portable path is not absolute after expansion: {state.PORTABLE!r}")
        return {}

    results = {}
    for emulator in os.listdir(path):
        symlinks_file = os.path.join(path, emulator, "symlinks.json")
        if not os.path.isfile(symlinks_file):
            continue
        with open(symlinks_file) as f:
            try:
                symlinks_config = json.load(f)
            except json.JSONDecodeError as e:
                console_error(f"Invalid JSON in {symlinks_file}: {e}")
                continue

        flatpak_id = symlinks_config.get("flatpak", "")
        flatpak_active = _flatpak_installed(flatpak_id)
        for entry, links in symlinks_config.items():
            if entry == "flatpak":
                continue
            if not isinstance(links, dict):
                continue
            source = os.path.abspath(os.path.join(path, emulator, entry))
            if state.PLATFORM not in links:
                continue
            link = os.path.abspath(os.path.expanduser(os.path.expandvars(
                links[state.PLATFORM]
                .replace("${host}", state.HOST)
                .replace("${portable}", state.PORTABLE)
                .replace("${flatpak}", flatpak_id)
            )))
            if flatpak_active:
                link = _flatpak_remap(link, flatpak_id)
            results.setdefault(emulator.upper(), []).append(
                (flatpak_id, os.path.normpath(link), os.path.normpath(source))
            )
    return results


def filter_emulators(emulators_dict, filter_list):
    """Filter the result of parse_config by emulator name (case-insensitive)."""
    if not filter_list:
        return emulators_dict
    lower_filter = [e.lower() for e in filter_list]
    return {k: v for k, v in emulators_dict.items() if k.lower() in lower_filter}


def find_emulator_dir(project_dir, name):
    """Case-insensitive lookup of an emulator subdirectory."""
    for d in os.listdir(project_dir):
        if d.lower() == name.lower() and os.path.isdir(os.path.join(project_dir, d)):
            return d
    return None
