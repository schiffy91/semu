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


def shortcuts_path(steam_root: Optional[str] = None, user_id: Optional[str] = None) -> Optional[str]:
    """Locate <steam_root>/userdata/<id>/config/shortcuts.vdf.

    If `user_id` is None, picks the first userdata directory found. Returns
    None if no Steam install is detected.
    """
    candidates = [
        steam_root,
        os.path.expanduser("~/.steam/steam"),
        os.path.expanduser("~/.local/share/Steam"),
        os.path.expanduser("~/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
    ]
    for root in candidates:
        if not root:
            continue
        userdata = os.path.join(root, "userdata")
        if not os.path.isdir(userdata):
            continue
        users = [d for d in os.listdir(userdata) if d.isdigit()]
        if not users:
            continue
        chosen = user_id or users[0]
        return os.path.join(userdata, chosen, "config", "shortcuts.vdf")
    return None


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
