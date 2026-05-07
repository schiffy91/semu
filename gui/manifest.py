"""Static metadata describing each emulator card. Decouples the GUI from
filesystem layout so we can render cards even if a project dir hasn't been
chosen yet."""

from dataclasses import dataclass, field
from typing import List


@dataclass
class EmulatorMeta:
    name: str            # display name + matches subdir under project_dir
    systems: str         # one-line system list
    platforms: List[str] = field(default_factory=list)


EMULATORS: List[EmulatorMeta] = [
    EmulatorMeta("RetroArch", "Multi-system (libretro cores)", ["linux", "macos"]),
    EmulatorMeta("Dolphin",   "GameCube, Wii",                 ["linux", "macos"]),
    EmulatorMeta("PCSX2",     "PlayStation 2",                 ["linux", "macos"]),
    EmulatorMeta("Cemu",      "Wii U",                         ["linux", "macos"]),
    # Ryujinx upstream archived Oct 2024 — kept for users with existing
    # libraries; Azahar is the recommended path forward for Switch.
    EmulatorMeta("Ryujinx",   "Nintendo Switch (unmaintained upstream)", ["linux", "macos"]),
    EmulatorMeta("Azahar",    "Nintendo 3DS",                  ["linux", "macos"]),
    EmulatorMeta("ES-DE",     "Frontend",                      ["linux", "macos"]),
]
