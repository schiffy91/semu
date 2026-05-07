"""Detect tools the lifecycle commands need (nix, flatpak, syncthing).

Returns a structured report so the GUI can show a friendly warning panel
instead of failing late inside a worker thread.
"""

import shutil
from dataclasses import dataclass
from typing import List, Optional

from core import state


@dataclass
class Prereq:
    name: str
    available: bool
    install_hint: str


def check() -> List[Prereq]:
    """Return one Prereq per dependency. `available` is False when the tool
    is required but missing for the current platform."""
    out: List[Prereq] = []

    out.append(Prereq(
        name="nix",
        available=shutil.which("nix") is not None,
        install_hint=(
            "Install Nix from https://nixos.org/download.html "
            "(curl -L https://nixos.org/nix/install | sh on macOS / Linux)"
        ),
    ))

    if state.PLATFORM == "linux":
        out.append(Prereq(
            name="flatpak",
            available=shutil.which("flatpak") is not None,
            install_hint=(
                "Install Flatpak via your distro's package manager "
                "(`sudo pacman -S flatpak` on SteamOS Desktop)"
            ),
        ))

    syncthing_present = shutil.which("syncthing") is not None
    # Also check the bundled-binary location.
    import os
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bundled = os.path.join(repo_root, "bin", "syncthing")
    if os.path.isfile(bundled) and os.access(bundled, os.X_OK):
        syncthing_present = True
    out.append(Prereq(
        name="syncthing",
        available=syncthing_present,
        install_hint=(
            "Drop a syncthing binary at <repo>/bin/syncthing or install via "
            "your package manager (Save sync requires it; install/update don't)."
        ),
    ))

    return out


def critical_missing() -> List[Prereq]:
    """Return only prereqs that are actually required (currently: nix)."""
    return [p for p in check() if not p.available and p.name == "nix"]
