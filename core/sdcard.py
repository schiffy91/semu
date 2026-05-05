"""SD card / external-storage detection and ROM scanning for Steam Deck.

EmuDeck/RetroDeck convention:
    /run/media/deck/<sd>/Emulation/roms/<system>/...
    /run/media/<user>/<sd>/Emulation/roms/<system>/...
    /run/media/mmcblk0p1/Emulation/roms/<system>/...
"""

import os
from dataclasses import dataclass, field
from typing import Dict, List, Optional


# Maps a ROM file extension onto the ES-DE / EmuDeck system folder name.
EXT_TO_SYSTEM: Dict[str, str] = {
    # Nintendo
    ".gb": "gb", ".gbc": "gbc", ".gba": "gba",
    ".nds": "nds",
    ".nes": "nes", ".smc": "snes", ".sfc": "snes",
    ".n64": "n64", ".z64": "n64", ".v64": "n64",
    ".gcm": "gc", ".rvz": "gc", ".iso": "gc",
    ".wad": "wii", ".wbfs": "wii",
    ".3ds": "n3ds", ".cia": "n3ds",
    ".nsp": "switch", ".xci": "switch",
    ".wua": "wiiu", ".rpx": "wiiu",
    # Sega
    ".md": "genesis", ".gen": "genesis", ".smd": "genesis",
    ".gg": "gg", ".sms": "mastersystem",
    ".cdi": "dreamcast", ".gdi": "dreamcast", ".chd": "dreamcast",
    # Sony
    ".cue": "psx", ".bin": "psx", ".pbp": "psx",
    ".cso": "psp",
    # PCSX2 specific extensions
    ".bin.ecm": "ps2",
}


# Standard /run/media mount roots to scan.
SD_MOUNT_ROOTS = (
    "/run/media",
    "/media",
)


@dataclass
class SdCard:
    """A detected external storage mount."""
    mount_path: str
    label: str
    has_emudeck_layout: bool
    rom_systems: Dict[str, List[str]] = field(default_factory=dict)


def list_sdcards() -> List[SdCard]:
    """Enumerate currently-mounted external storage candidates."""
    cards: List[SdCard] = []
    for root in SD_MOUNT_ROOTS:
        if not os.path.isdir(root):
            continue
        for user_dir in _safe_listdir(root):
            user_path = os.path.join(root, user_dir)
            if not os.path.isdir(user_path):
                continue
            # Either /run/media/<user>/<label> (modern) or /run/media/<label> (legacy)
            if _looks_like_mount(user_path):
                cards.append(_inspect_mount(user_path, user_dir))
                continue
            for label in _safe_listdir(user_path):
                mount = os.path.join(user_path, label)
                if _looks_like_mount(mount):
                    cards.append(_inspect_mount(mount, label))
    return cards


def _safe_listdir(path: str) -> List[str]:
    try:
        return os.listdir(path)
    except OSError:
        return []


def _looks_like_mount(path: str) -> bool:
    """A mount has children we can read and isn't a regular file."""
    if not os.path.isdir(path):
        return False
    # Skip mountpoints we can't read.
    try:
        os.listdir(path)
    except OSError:
        return False
    return True


def _inspect_mount(mount_path: str, label: str) -> SdCard:
    emudeck_dir = os.path.join(mount_path, "Emulation", "roms")
    has_emudeck = os.path.isdir(emudeck_dir)
    rom_systems = scan_roms(mount_path)
    return SdCard(
        mount_path=mount_path,
        label=label,
        has_emudeck_layout=has_emudeck,
        rom_systems=rom_systems,
    )


def scan_roms(mount_path: str, max_depth: int = 4) -> Dict[str, List[str]]:
    """Scan a mount for ROM files. Prefers EmuDeck's `Emulation/roms/<system>/`
    layout; falls back to extension-based detection."""
    emudeck_dir = os.path.join(mount_path, "Emulation", "roms")
    if os.path.isdir(emudeck_dir):
        return _scan_emudeck(emudeck_dir)
    return _scan_by_extension(mount_path, max_depth)


def _scan_emudeck(roms_dir: str) -> Dict[str, List[str]]:
    out: Dict[str, List[str]] = {}
    for system in _safe_listdir(roms_dir):
        sys_path = os.path.join(roms_dir, system)
        if not os.path.isdir(sys_path):
            continue
        files: List[str] = []
        for root, _, names in os.walk(sys_path):
            for name in names:
                if not name.startswith("."):
                    files.append(os.path.join(root, name))
        if files:
            out[system] = files
    return out


def _scan_by_extension(root: str, max_depth: int) -> Dict[str, List[str]]:
    out: Dict[str, List[str]] = {}
    root_depth = root.rstrip(os.sep).count(os.sep)
    for dirpath, dirnames, filenames in os.walk(root):
        depth = dirpath.count(os.sep) - root_depth
        if depth > max_depth:
            dirnames[:] = []
            continue
        for f in filenames:
            ext = os.path.splitext(f)[1].lower()
            system = EXT_TO_SYSTEM.get(ext)
            if not system:
                continue
            out.setdefault(system, []).append(os.path.join(dirpath, f))
    return out


def best_card(cards: List[SdCard]) -> Optional[SdCard]:
    """Pick the most-likely 'the user's ROM card' from a list of mounts."""
    if not cards:
        return None
    emudeck = [c for c in cards if c.has_emudeck_layout]
    if emudeck:
        return max(emudeck, key=lambda c: sum(len(v) for v in c.rom_systems.values()))
    return max(cards, key=lambda c: sum(len(v) for v in c.rom_systems.values()))


# Firmware / BIOS files that emulators expect. Returned by `check_firmware`.
FIRMWARE_REQUIREMENTS = {
    "PCSX2":   ["bios/ps2-0230a-20080220.bin", "bios/ps2-0230e-20080220.bin"],
    "Ryujinx": ["system/prod.keys", "system/title.keys"],
    "Cemu":    ["keys.txt"],
    "Azahar":  ["sysdata/aes_keys.txt"],
}


def check_firmware(project_dir: str) -> Dict[str, List[str]]:
    """Return a {emulator: [missing files...]} map for emulators that need BIOS/keys."""
    missing: Dict[str, List[str]] = {}
    for emulator, files in FIRMWARE_REQUIREMENTS.items():
        emu_dir = os.path.join(project_dir, emulator)
        if not os.path.isdir(emu_dir):
            continue
        missing_files = [f for f in files if not os.path.exists(os.path.join(emu_dir, f))]
        if missing_files:
            missing[emulator] = missing_files
    return missing
