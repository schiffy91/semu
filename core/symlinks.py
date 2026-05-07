"""Parse setup.json + per-emulator symlinks.json and materialise the symlinks.

This is the single source of truth for how project-dir paths map onto the OS's
config locations. The CLI and the GUI both call into here.
"""

import json
import os
from typing import Dict

from core import state
from core.console import console_error
from core.exec import delete, execute, own


BACKUP_EXCLUDE = {"ROMs", "downloaded_media", "n3ds-fixed", "n3ds-original"}


def _parent(path):
    return os.path.dirname(path)


def _create_parent_dirs(path):
    execute("makedirs", _parent(path))


def _link_has_user_data(link: str) -> bool:
    """If `link` is a real directory containing real (non-symlink) entries,
    the user has accumulated data there. We must not silently delete it.

    A directory full ONLY of symlinks (e.g. one schemulator created on a
    previous run, where each child is a per-file symlink into the project
    dir) is NOT user data — it's our own wiring. Returns False there so
    re-install / uninstall flows that legitimately want to replace it can
    proceed. Round-6 critic refinement.
    """
    if not os.path.isdir(link) or os.path.islink(link):
        return False
    try:
        for entry in os.scandir(link):
            # Real file or real subdirectory → user data.
            if not entry.is_symlink():
                return True
        return False
    except OSError:
        return False


def create_symlink(link, source, migrate=False):
    """Wire a host path to point at the project-dir source.

    If `link` is a real directory containing user data:
      - migrate=False (default): refuse and emit an error so the caller can
        prompt the user. Protects existing saves from silent destruction.
      - migrate=True: copy the user's data into `source` (preserving anything
        already there — `source` wins on conflict), then replace `link` with
        the symlink. This is the active migration path the GUI invokes after
        the user opts in.
    """
    if os.path.lexists(_parent(link)):
        own(_parent(link))
    if os.path.lexists(link):
        own(link)
    if _link_has_user_data(link):
        if not migrate:
            from core.console import console_error
            console_error(
                f"Refusing to replace existing data at {link}\n"
                f"  Move its contents into {source} first, then re-run symlink,\n"
                f"  or invoke with migrate=True to auto-merge into the project dir.\n"
            )
            return
        _migrate_existing(link, source)
    delete(link)
    _create_parent_dirs(link)
    execute("symlink", source, link, target_is_directory=os.path.isdir(source))


def _migrate_existing(link: str, source: str) -> None:
    """Copy contents of the host link path into the project source dir.
    Already-present files in `source` win — the project dir is the source of
    truth (e.g. user already cloud-synced a save from another device)."""
    import shutil
    from core.console import console_log
    if not os.path.isdir(source):
        os.makedirs(source, exist_ok=True)
    moved = 0
    for entry in os.listdir(link):
        src_path = os.path.join(link, entry)
        dst_path = os.path.join(source, entry)
        if os.path.exists(dst_path):
            continue  # project version wins
        try:
            if os.path.isdir(src_path):
                shutil.copytree(src_path, dst_path, symlinks=True)
            else:
                shutil.copy2(src_path, dst_path)
            moved += 1
        except OSError:
            pass
    console_log(f"Migrated {moved} existing entries from {link} into {source}")


def create_symlinks(link, source, migrate=False):
    if os.path.isdir(source):
        for item in os.listdir(source):
            create_symlink(os.path.join(link, item), os.path.join(source, item), migrate=migrate)
    else:
        _, file_name = os.path.split(source)
        create_symlink(os.path.join(link, file_name), source, migrate=migrate)


def detect_existing_user_data(parsed: dict) -> Dict[str, list]:
    """Return {EMULATOR: [link_path, ...]} for emulators where the host link
    target already contains user data.

    Used by the GUI to prompt "Move existing data into project dir?" before
    running symlink (round-6 critic finding #3). `parsed` is the dict
    returned by parse_config().
    """
    out = {}
    for emulator, entries in parsed.items():
        for _, link_path, _ in entries:
            if _link_has_user_data(link_path):
                out.setdefault(emulator, []).append(link_path)
    return out


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
            # Only our explicit ${host} / ${portable} / ${flatpak} placeholders
            # are substituted, plus a leading ~/ via os.path.expanduser. We
            # deliberately do NOT call os.path.expandvars: arbitrary $VAR
            # substitution from a peer-controlled symlinks.json is a
            # filesystem-redirection vector (round-5 critic finding #3).
            raw = (links[state.PLATFORM]
                   .replace("${host}", state.HOST)
                   .replace("${portable}", state.PORTABLE)
                   .replace("${flatpak}", flatpak_id))
            link = os.path.abspath(os.path.expanduser(raw))
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
