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
    EmulatorMeta("Ryujinx",   "Nintendo Switch",               ["linux", "macos"]),
    EmulatorMeta("Azahar",    "Nintendo 3DS",                  ["linux", "macos"]),
    EmulatorMeta("Lime3DS",   "Nintendo 3DS (legacy)",         ["linux"]),
    EmulatorMeta("ES-DE",     "Frontend",                      ["linux", "macos"]),
]
