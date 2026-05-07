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
#
# Notes on ambiguous extensions:
#   - .iso could be GameCube, Wii, PS2, Dreamcast, PSP, or even PS1. We don't
#     guess by extension; instead, EmuDeck-layout scanning is preferred and
#     mixed-content carts fall through to extension scanning where .iso is
#     intentionally absent.
#   - .bin is also ambiguous (PS1 cue/bin, PS2 BIN/CUE, Dreamcast). Same
#     reasoning — we drop it from the extension table.
#   - .chd covers Dreamcast, PS1, PS2, Saturn — we map it to Dreamcast as the
#     most common case but the user can recategorise via ES-DE.
EXT_TO_SYSTEM: Dict[str, str] = {
    # Nintendo
    ".gb": "gb", ".gbc": "gbc", ".gba": "gba",
    ".nds": "nds",
    ".nes": "nes", ".smc": "snes", ".sfc": "snes",
    ".n64": "n64", ".z64": "n64", ".v64": "n64",
    ".gcm": "gc", ".rvz": "gc",
    ".wad": "wii", ".wbfs": "wii", ".wia": "wii",
    ".3ds": "n3ds", ".cia": "n3ds", ".cci": "n3ds",
    ".nsp": "switch", ".xci": "switch",
    ".wua": "wiiu", ".rpx": "wiiu", ".wud": "wiiu",
    # Sega
    ".md": "genesis", ".gen": "genesis", ".smd": "genesis",
    ".gg": "gg", ".sms": "mastersystem",
    ".cdi": "dreamcast", ".gdi": "dreamcast", ".chd": "dreamcast",
    ".32x": "sega32x",
    # Sony
    ".pbp": "psx", ".cue": "psx",
    ".cso": "psp",
    # Atari
    ".lnx": "atarilynx", ".jag": "atarijaguar", ".j64": "atarijaguar",
    # Other
    ".min": "pokemini",
}


# Standard /run/media mount roots to scan. macOS uses /Volumes; we list it last
# so Linux paths take precedence on dual-platform devs.
SD_MOUNT_ROOTS = (
    "/run/media",
    "/media",
    "/Volumes",
)


# Minimum file size to consider a candidate "real ROM" rather than a stub.
_MIN_ROM_SIZE = 1024  # 1KB; anything smaller is almost certainly a save state stub


@dataclass
class SdCard:
    """A detected external storage mount."""
    mount_path: str
    label: str
    has_emudeck_layout: bool
    rom_systems: Dict[str, List[str]] = field(default_factory=dict)


def _is_root_volume(path: str) -> bool:
    """True if `path` resolves to the same mount as `/`. On macOS, /Volumes
    contains the boot disk itself ("/Volumes/Macintosh HD") which would make
    the scanner walk the entire disk — bound it (critic finding #17)."""
    try:
        return os.stat(path).st_dev == os.stat("/").st_dev
    except OSError:
        return False


def list_sdcards() -> List[SdCard]:
    """Enumerate currently-mounted external storage candidates.

    Steam Deck convention: `/run/media/deck/<sd_label>/`. Older / non-Deck
    Linux: `/media/<user>/<sd_label>/` or `/run/media/<sd_label>/`. macOS:
    `/Volumes/<sd_label>/`, but we explicitly skip the boot volume which
    would otherwise trigger a full-disk walk.

    A user-dir vs actual-mount disambiguation: only directories that look
    like ROM stores (have an `Emulation/` subtree, or hold known ROM
    extensions in the first two levels) are reported.
    """
    cards: List[SdCard] = []
    for root in SD_MOUNT_ROOTS:
        if not os.path.isdir(root):
            continue
        for user_dir in _safe_listdir(root):
            user_path = os.path.join(root, user_dir)
            if not os.path.isdir(user_path):
                continue
            # Skip the boot volume on /Volumes (macOS) — same fs as /.
            if root == "/Volumes" and _is_root_volume(user_path):
                continue
            if _looks_like_rom_store(user_path):
                cards.append(_inspect_mount(user_path, user_dir))
                continue
            # Treat user_path as a username; descend one more level.
            for label in _safe_listdir(user_path):
                mount = os.path.join(user_path, label)
                if root == "/Volumes" and _is_root_volume(mount):
                    continue
                if _looks_like_rom_store(mount):
                    cards.append(_inspect_mount(mount, label))
    return cards


def _safe_listdir(path: str) -> List[str]:
    try:
        return os.listdir(path)
    except OSError:
        return []


def _looks_like_mount(path: str) -> bool:
    """Path is a readable directory (kept for backward compat)."""
    if not os.path.isdir(path):
        return False
    try:
        os.listdir(path)
    except OSError:
        return False
    return True


def _looks_like_rom_store(path: str) -> bool:
    """A directory worth surfacing as an SD card: contains an Emulation/
    subtree (EmuDeck convention) or has at least one ROM extension somewhere
    in the first two levels."""
    if not _looks_like_mount(path):
        return False
    if os.path.isdir(os.path.join(path, "Emulation")):
        return True
    try:
        for entry in os.listdir(path):
            full = os.path.join(path, entry)
            if os.path.isfile(full) and os.path.splitext(entry)[1].lower() in EXT_TO_SYSTEM:
                return True
            if os.path.isdir(full):
                try:
                    for sub in os.listdir(full):
                        if os.path.splitext(sub)[1].lower() in EXT_TO_SYSTEM:
                            return True
                except OSError:
                    continue
    except OSError:
        return False
    return False


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


# Save-state / metadata extensions that aren't ROMs even when sitting in a
# system dir. Filtering them out keeps the rom_systems lists from ballooning
# on EmuDeck-layout cards where Dolphin/PCSX2 leave per-game state files.
_NON_ROM_EXTS = frozenset({
    ".srm", ".sav", ".state", ".s00", ".s01", ".s02", ".s03", ".s04",
    ".s05", ".s06", ".s07", ".s08", ".s09", ".s10",
    ".sa1", ".sa2",
    ".rtc", ".rwx",
    ".png", ".jpg", ".jpeg", ".webp", ".bmp",
    ".txt", ".xml", ".json", ".cfg", ".ini",
    ".log",
})


def _looks_like_rom(name: str, full_path: str) -> bool:
    """Heuristic to drop save-state / artwork files from ROM listings.
    Round-7 critic finding #6 (SD scans on EmuDeck-layout cards used to
    list 50k+ entries because per-game save states slipped in)."""
    if name.startswith("."):
        return False
    ext = os.path.splitext(name)[1].lower()
    if ext in _NON_ROM_EXTS:
        return False
    try:
        if os.path.getsize(full_path) < _MIN_ROM_SIZE:
            return False
    except OSError:
        return False
    return True


def _scan_emudeck(roms_dir: str) -> Dict[str, List[str]]:
    out: Dict[str, List[str]] = {}
    for system in _safe_listdir(roms_dir):
        sys_path = os.path.join(roms_dir, system)
        if not os.path.isdir(sys_path):
            continue
        files: List[str] = []
        for root, _, names in os.walk(sys_path):
            for name in names:
                full = os.path.join(root, name)
                if _looks_like_rom(name, full):
                    files.append(full)
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
            full = os.path.join(dirpath, f)
            if not _looks_like_rom(f, full):
                continue
            out.setdefault(system, []).append(full)
    return out


def wire_es_de_to_card(card: "SdCard", project_dir: str) -> str:
    """Make ES-DE read ROMs from the chosen SD card's `Emulation/roms/` root.

    ES-DE keeps its top-level settings in `<portable>/settings/es_settings.xml`
    where `<portable>` resolves to `~/ES-DE/` on Linux/macOS. The
    `ROMDirectory` setting is what `%ROMPATH%` substitutes to in
    `es_systems.xml`. Setting it to the SD card's `Emulation/roms/` makes
    every system in our shipped systems file resolve onto the card.

    Returns the path to the file written, or "" if the write failed.
    Round-6 critic finding #2.
    """
    import xml.etree.ElementTree as ET

    rom_root = os.path.join(card.mount_path, "Emulation", "roms")
    if not os.path.isdir(rom_root):
        # Fall back to the card root if there's no EmuDeck layout.
        rom_root = card.mount_path

    settings_dir = os.path.expanduser("~/ES-DE/settings")
    os.makedirs(settings_dir, exist_ok=True)
    settings_path = os.path.join(settings_dir, "es_settings.xml")

    # Load existing settings if present so we preserve user customisations.
    if os.path.exists(settings_path):
        try:
            tree = ET.parse(settings_path)
            root = tree.getroot()
        except ET.ParseError:
            root = ET.Element("settings")
            tree = ET.ElementTree(root)
    else:
        root = ET.Element("settings")
        tree = ET.ElementTree(root)

    # Upsert the <string name="ROMDirectory" value="..." /> element.
    found = False
    for el in root.findall("string"):
        if el.get("name") == "ROMDirectory":
            el.set("value", rom_root)
            found = True
            break
    if not found:
        ET.SubElement(root, "string", {"name": "ROMDirectory", "value": rom_root})

    # Atomic write
    tmp = settings_path + ".tmp"
    try:
        tree.write(tmp, encoding="utf-8", xml_declaration=True)
        os.replace(tmp, settings_path)
    except OSError:
        if os.path.exists(tmp):
            try:
                os.remove(tmp)
            except OSError:
                pass
        return ""
    return settings_path


def best_card(cards: List[SdCard]) -> Optional[SdCard]:
    """Pick the most-likely 'the user's ROM card' from a list of mounts."""
    if not cards:
        return None
    emudeck = [c for c in cards if c.has_emudeck_layout]
    if emudeck:
        return max(emudeck, key=lambda c: sum(len(v) for v in c.rom_systems.values()))
    return max(cards, key=lambda c: sum(len(v) for v in c.rom_systems.values()))


# Firmware / BIOS requirements per emulator. `glob` is checked relative to the
# emulator's project-dir. The check passes if at least one match is found
# (e.g. PCSX2 ships with several BIOS dumps and only one is needed). `desc`
# is the user-facing description shown in the GUI when missing.
FIRMWARE_REQUIREMENTS = {
    "PCSX2":   {"glob": "bios/*.bin", "desc": "PS2 BIOS (.bin) under PCSX2/bios/"},
    "Ryujinx": {"glob": "config/system/prod.keys", "desc": "Switch keys (prod.keys) under Ryujinx/config/system/"},
    "Cemu":    {"glob": "data/keys.txt", "desc": "Wii U keys.txt under Cemu/data/"},
    "Azahar":  {"glob": "data/sysdata/aes_keys.txt", "desc": "3DS aes_keys.txt under Azahar/data/sysdata/"},
}


def check_firmware(project_dir: str) -> Dict[str, str]:
    """Return a {emulator: description} map for emulators with missing firmware.

    Only emulators that have a project-dir already are checked — if the
    directory doesn't exist (emulator not installed), no firmware warning
    is emitted.
    """
    import glob as _glob
    missing: Dict[str, str] = {}
    for emulator, req in FIRMWARE_REQUIREMENTS.items():
        emu_dir = os.path.join(project_dir, emulator)
        if not os.path.isdir(emu_dir):
            continue
        matches = _glob.glob(os.path.join(emu_dir, req["glob"]))
        if not matches:
            missing[emulator] = req["desc"]
    return missing
