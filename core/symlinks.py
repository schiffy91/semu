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


def create_symlink(link, source):
    if os.path.lexists(_parent(link)):
        own(_parent(link))
    if os.path.lexists(link):
        own(link)
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


def parse_config(file_path, path):
    """Parse setup.json + per-emulator symlinks.json. Returns
    {EMULATOR_UPPER: [(flatpak_id, link, source), ...]}."""
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
