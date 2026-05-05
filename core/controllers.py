"""Apply controller-binding profile bundles to per-emulator configs.

Profiles live under controllers/<profile>/<emulator>.<ext>. Applying a profile
copies the file into the emulator's project-dir config (which is symlinked
into place by core.symlinks).
"""

import os
import shutil
from typing import List

from core.console import console_error, console_log


PROFILES = ("xbox", "dualsense", "generic-xinput", "steamdeck")


# Per-emulator: where in <project_dir>/<Emulator>/ does the profile fragment land?
PROFILE_TARGETS = {
    "RetroArch": ("config", "autoconfig.fragment.cfg"),
    "Dolphin":   ("config", "GCPadNew.ini"),
    "PCSX2":     ("config", "inputprofiles", "schemulator.ini"),
    "Cemu":      ("config", "controllerProfiles", "schemulator.xml"),
    "Ryujinx":   ("config", "profiles", "controller", "schemulator.json"),
    "Azahar":    ("data", "config", "qt-config.ini"),
    "Lime3DS":   ("data", "config", "qt-config.ini"),
}


def _profile_root() -> str:
    return os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "controllers")


def list_profiles() -> List[str]:
    root = _profile_root()
    if not os.path.isdir(root):
        return []
    return sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))


def _profile_file(profile: str, emulator: str) -> str:
    """Find the source file for a (profile, emulator) pair."""
    root = os.path.join(_profile_root(), profile)
    if not os.path.isdir(root):
        return ""
    name = emulator.lower()
    for entry in os.listdir(root):
        if entry.lower().startswith(name + "."):
            return os.path.join(root, entry)
    return ""


def apply(project_dir: str, emulator: str, profile: str) -> bool:
    """Copy `controllers/<profile>/<emulator>.*` into the project-dir slot for
    that emulator. Returns True on success."""
    if profile not in PROFILES:
        console_error(f"Unknown controller profile: {profile}")
        return False

    src = _profile_file(profile, emulator)
    if not src:
        console_log(f"No {profile} profile bundled for {emulator}; skipping")
        return False

    target_parts = PROFILE_TARGETS.get(emulator)
    if not target_parts:
        console_log(f"No target path defined for {emulator}; skipping")
        return False

    dest = os.path.join(project_dir, emulator, *target_parts)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    shutil.copyfile(src, dest)
    console_log(f"Applied {profile} controller profile to {emulator} -> {dest}")
    return True


def apply_all(project_dir: str, profile: str) -> int:
    """Apply a profile to every emulator that has a bundled fragment. Returns
    the number of emulators successfully updated."""
    n = 0
    for emulator in PROFILE_TARGETS:
        if apply(project_dir, emulator, profile):
            n += 1
    return n
