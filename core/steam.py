"""Steam non-Steam-game registration via shortcuts.vdf.

shortcuts.vdf is binary KeyValues (NOT text VDF). We implement a minimal
encoder/decoder that handles the subset Steam writes for non-Steam shortcuts:

    object  marker = 0x00
    string  marker = 0x01
    int32   marker = 0x02
    end-of-object marker = 0x08
    end-of-file   marker = 0x08 0x08

Each shortcut entry is keyed by its index ("0", "1", ...) inside a top-level
"shortcuts" object.

Reference: open-source projects (Steam-ROM-Manager, vdf-py) use the same shape.
"""

import os
import struct
import time
import zlib
from dataclasses import dataclass, field
from typing import Dict, List, Optional


@dataclass
class Shortcut:
    appname: str
    exe: str
    start_dir: str = ""
    icon: str = ""
    launch_options: str = ""
    is_hidden: int = 0
    allow_desktop_config: int = 1
    allow_overlay: int = 1
    open_vr: int = 0
    last_play_time: int = 0
    tags: List[str] = field(default_factory=list)


def _appid(appname: str, exe: str) -> int:
    """Steam computes a non-Steam app ID from CRC32(exe + appname). Matches
    the algorithm used by Steam itself for shortcut hashing."""
    crc = zlib.crc32((exe + appname).encode("utf-8")) & 0xFFFFFFFF
    return crc | 0x80000000


def _write_string(key: str, value: str) -> bytes:
    return b"\x01" + key.encode("utf-8") + b"\x00" + value.encode("utf-8") + b"\x00"


def _write_int(key: str, value: int) -> bytes:
    # shortcuts.vdf stores 32-bit values in the int32 slot but Steam packs
    # appids with bit 31 set, exceeding signed int32. Use unsigned for round-trip
    # safety; matches Steam-ROM-Manager and vdf-py behaviour.
    return b"\x02" + key.encode("utf-8") + b"\x00" + struct.pack("<I", value & 0xFFFFFFFF)


def _write_object(key: str, body: bytes) -> bytes:
    return b"\x00" + key.encode("utf-8") + b"\x00" + body + b"\x08"


def encode_shortcuts(shortcuts: List[Shortcut]) -> bytes:
    entries = b""
    for i, s in enumerate(shortcuts):
        appid = _appid(s.appname, s.exe)
        body = (
            _write_int("appid", appid)
            + _write_string("AppName", s.appname)
            + _write_string("Exe", s.exe)
            + _write_string("StartDir", s.start_dir or os.path.dirname(s.exe))
            + _write_string("icon", s.icon)
            + _write_string("ShortcutPath", "")
            + _write_string("LaunchOptions", s.launch_options)
            + _write_int("IsHidden", s.is_hidden)
            + _write_int("AllowDesktopConfig", s.allow_desktop_config)
            + _write_int("AllowOverlay", s.allow_overlay)
            + _write_int("OpenVR", s.open_vr)
            + _write_int("Devkit", 0)
            + _write_string("DevkitGameID", "")
            + _write_int("DevkitOverrideAppID", 0)
            + _write_int("LastPlayTime", s.last_play_time or int(time.time()))
            + _write_string("FlatpakAppID", "")
            + _write_object("tags", b"".join(_write_string(str(j), t) for j, t in enumerate(s.tags)))
        )
        entries += _write_object(str(i), body)
    return _write_object("shortcuts", entries) + b"\x08"


def decode_shortcuts(data: bytes) -> List[Shortcut]:
    """Decode a shortcuts.vdf blob. Best-effort, tolerant of unknown keys."""
    pos = 0

    def read_cstring(start: int):
        end = data.index(b"\x00", start)
        return data[start:end].decode("utf-8", errors="replace"), end + 1

    def parse_object(start: int):
        out: Dict[str, object] = {}
        i = start
        while i < len(data):
            marker = data[i]
            i += 1
            if marker == 0x08:
                return out, i
            key, i = read_cstring(i)
            if marker == 0x00:
                value, i = parse_object(i)
                out[key] = value
            elif marker == 0x01:
                value, i = read_cstring(i)
                out[key] = value
            elif marker == 0x02:
                value = struct.unpack("<I", data[i:i + 4])[0]
                i += 4
                out[key] = value
            else:
                # Unknown marker — bail.
                return out, i
        return out, i

    parsed, _ = parse_object(pos)
    shortcuts_obj = parsed.get("shortcuts", {})
    out: List[Shortcut] = []
    if isinstance(shortcuts_obj, dict):
        for _, entry in shortcuts_obj.items():
            if not isinstance(entry, dict):
                continue
            tags_obj = entry.get("tags", {}) if isinstance(entry.get("tags"), dict) else {}
            out.append(Shortcut(
                appname=str(entry.get("AppName", "")),
                exe=str(entry.get("Exe", "")),
                start_dir=str(entry.get("StartDir", "")),
                icon=str(entry.get("icon", "")),
                launch_options=str(entry.get("LaunchOptions", "")),
                is_hidden=int(entry.get("IsHidden", 0) or 0),
                allow_desktop_config=int(entry.get("AllowDesktopConfig", 1) or 0),
                allow_overlay=int(entry.get("AllowOverlay", 1) or 0),
                open_vr=int(entry.get("OpenVR", 0) or 0),
                last_play_time=int(entry.get("LastPlayTime", 0) or 0),
                tags=[str(v) for v in tags_obj.values()] if isinstance(tags_obj, dict) else [],
            ))
    return out


STEAM_ROOT_CANDIDATES = (
    "~/.steam/steam",                                              # Linux native
    "~/.local/share/Steam",                                        # Linux alt
    "~/.var/app/com.valvesoftware.Steam/.local/share/Steam",        # Linux Flatpak
    "~/Library/Application Support/Steam",                          # macOS
)


def find_steam_root(extra: Optional[str] = None) -> Optional[str]:
    """Locate a Steam install. Returns absolute path or None."""
    candidates = [extra] + list(STEAM_ROOT_CANDIDATES) if extra else list(STEAM_ROOT_CANDIDATES)
    for c in candidates:
        if not c:
            continue
        path = os.path.expanduser(c)
        if os.path.isdir(os.path.join(path, "userdata")):
            return path
    return None


def list_steam_users(steam_root: Optional[str] = None) -> List[str]:
    """All numeric userdata IDs under a Steam install, newest first."""
    root = steam_root or find_steam_root()
    if not root:
        return []
    userdata = os.path.join(root, "userdata")
    if not os.path.isdir(userdata):
        return []
    users = [d for d in os.listdir(userdata) if d.isdigit()]
    # Sort newest-modified first so the active user is preferred.
    users.sort(key=lambda u: os.path.getmtime(os.path.join(userdata, u)), reverse=True)
    return users


def shortcuts_path(steam_root: Optional[str] = None, user_id: Optional[str] = None) -> Optional[str]:
    """Locate <steam_root>/userdata/<id>/config/shortcuts.vdf.

    If `user_id` is None, picks the most-recently-modified userdata directory.
    Returns None if no Steam install is detected.
    """
    root = steam_root or find_steam_root()
    if not root:
        return None
    users = list_steam_users(root)
    if not users:
        return None
    chosen = user_id or users[0]
    return os.path.join(root, "userdata", chosen, "config", "shortcuts.vdf")


def upsert_shortcut(path: str, shortcut: Shortcut) -> bool:
    """Insert or update a shortcut by AppName. Returns True if file was written."""
    existing: List[Shortcut] = []
    if os.path.exists(path):
        try:
            with open(path, "rb") as f:
                existing = decode_shortcuts(f.read())
        except Exception:
            existing = []
    found = False
    for i, s in enumerate(existing):
        if s.appname == shortcut.appname:
            existing[i] = shortcut
            found = True
            break
    if not found:
        existing.append(shortcut)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(encode_shortcuts(existing))
    return True


def remove_shortcut(path: str, appname: str) -> bool:
    if not os.path.exists(path):
        return False
    with open(path, "rb") as f:
        existing = decode_shortcuts(f.read())
    new = [s for s in existing if s.appname != appname]
    if len(new) == len(existing):
        return False
    with open(path, "wb") as f:
        f.write(encode_shortcuts(new))
    return True


def discover_installed_emulators(project_dir: str) -> List["DiscoveredEmulator"]:
    """Walk `<project_dir>/result-<emu>/` symlinks and find launchable binaries
    for each installed emulator. Returns one DiscoveredEmulator per emulator
    that has a usable binary in the standard layout (Linux: bin/<name>; macOS:
    Applications/<Name>.app/Contents/MacOS/<name>)."""
    out: List[DiscoveredEmulator] = []
    if not os.path.isdir(project_dir):
        return out
    for entry in os.listdir(project_dir):
        if not entry.startswith("result-") or entry.endswith("-prev"):
            continue
        result = os.path.join(project_dir, entry)
        emulator = entry[len("result-"):]
        # Linux: <result>/bin/<some_name>
        bin_dir = os.path.join(result, "bin")
        if os.path.isdir(bin_dir):
            for fname in os.listdir(bin_dir):
                full = os.path.join(bin_dir, fname)
                if os.path.isfile(full) and os.access(full, os.X_OK):
                    out.append(DiscoveredEmulator(name=emulator, exe=full, kind="binary"))
                    break
            continue
        # macOS: <result>/Applications/<Name>.app
        apps = os.path.join(result, "Applications")
        if os.path.isdir(apps):
            for entry in os.listdir(apps):
                if entry.endswith(".app"):
                    app_path = os.path.join(apps, entry)
                    out.append(DiscoveredEmulator(name=emulator, exe=app_path, kind="app"))
                    break
    return out


@dataclass
class DiscoveredEmulator:
    name: str          # lowercase emulator name (matches result-<name>/)
    exe: str           # absolute path to launchable binary or .app
    kind: str          # "binary" (Linux) or "app" (macOS)
