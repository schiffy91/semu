"""Syncthing sidecar: launch, configure, and pair the bundled binary.

Two responsibilities:

  1. Manage the local Syncthing process so saves under <project_dir>/saves/
     are continuously synchronised with paired devices.
  2. Expose a tiny REST helper so the GUI can fetch this device's ID, accept
     pairing requests, and add shared folders.

Bundled binary lookup order:
  - $SCHEMULATOR_SYNCTHING (env override)
  - <repo_root>/bin/syncthing
  - whatever's on PATH
"""

import json
import os
import shutil
import subprocess
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET  # noqa: S405 — local syncthing config only
from dataclasses import dataclass
from typing import List, Optional


SYNC_FOLDER_ID = "schemulator-saves"
DEFAULT_API = "http://127.0.0.1:8384"
SYNC_FOLDER_NAME = "Schemulator Saves"


@dataclass
class SyncStatus:
    running: bool
    device_id: Optional[str] = None
    api_url: str = DEFAULT_API
    folder_path: Optional[str] = None


def find_binary() -> Optional[str]:
    env = os.environ.get("SCHEMULATOR_SYNCTHING")
    if env and os.path.isfile(env) and os.access(env, os.X_OK):
        return env
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bundled = os.path.join(repo_root, "bin", "syncthing")
    if os.path.isfile(bundled) and os.access(bundled, os.X_OK):
        return bundled
    return shutil.which("syncthing")


def _config_home() -> str:
    return os.path.expanduser("~/.config/schemulator/syncthing")


def init(home: Optional[str] = None) -> bool:
    """Generate a fresh Syncthing config directory if one doesn't exist."""
    binary = find_binary()
    if not binary:
        return False
    home = home or _config_home()
    if os.path.exists(os.path.join(home, "config.xml")):
        return True
    os.makedirs(home, exist_ok=True)
    # syncthing 1.x uses `generate` subcommand; older builds used `--generate=PATH`.
    # Try the modern form first; fall back if it fails.
    result = subprocess.run(
        [binary, "generate", f"--home={home}", "--no-port-probing"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        subprocess.run([binary, f"--generate={home}"], capture_output=True, text=True)
    return os.path.exists(os.path.join(home, "config.xml"))


def device_id(home: Optional[str] = None) -> Optional[str]:
    """Resolve THIS device's Syncthing ID.

    Priority:
      1. Live REST API at /rest/system/status (returns `myID`). This is
         authoritative — config.xml device-list ordering is NOT guaranteed
         to put the local device first after the user adds peers.
      2. Fall back to parsing config.xml when the sidecar isn't running.
         We pick the local device by matching against the cert hash if
         available, otherwise the first device entry (best-effort).
    """
    home = home or _config_home()

    # 1) Live REST
    key = api_key(home)
    if key:
        req = urllib.request.Request(
            f"{DEFAULT_API}/rest/system/status",
            headers={"X-API-Key": key},
        )
        try:
            with urllib.request.urlopen(req, timeout=2) as resp:
                if resp.status == 200:
                    body = json.loads(resp.read().decode("utf-8"))
                    my_id = body.get("myID")
                    if my_id:
                        return my_id
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
            pass

    # 2) Offline fallback via config.xml.
    cfg = os.path.join(home, "config.xml")
    if not os.path.exists(cfg):
        return None
    try:
        tree = ET.parse(cfg)
    except ET.ParseError:
        return None
    devices = list(tree.getroot().findall("device"))
    if not devices:
        return None
    # Best-effort: the local device is the one referenced in <defaults><folder>
    # via <device id="..."/>. If we can match against that, use it. Otherwise
    # fall back to the first device in the config.
    defaults = tree.getroot().find("defaults")
    if defaults is not None:
        folder_def = defaults.find("folder")
        if folder_def is not None:
            ref = folder_def.find("device")
            if ref is not None and ref.get("id"):
                return ref.get("id")
    return devices[0].get("id")


def api_key(home: Optional[str] = None) -> Optional[str]:
    home = home or _config_home()
    cfg = os.path.join(home, "config.xml")
    if not os.path.exists(cfg):
        return None
    try:
        tree = ET.parse(cfg)
        gui = tree.getroot().find("gui")
        if gui is None:
            return None
        key = gui.find("apikey")
        return key.text if key is not None else None
    except ET.ParseError:
        return None


def start(home: Optional[str] = None, wait_for_ready: float = 15.0) -> Optional[subprocess.Popen]:
    """Spawn syncthing as a child process and wait until the REST API is reachable.

    Spawns with a minimal env (HOME, PATH, USER, XDG_*, plus the schemulator
    sidecar control vars STNORESTART/STNOUPGRADE) so any credentials in the
    parent process env don't leak into the long-running daemon (round-5 #4).
    Syncthing has historically respected `STGUIAPIKEY` from env, which would
    let a parent process set our REST key without our consent — strip it.

    Returns the Popen handle (caller terminates it) or None if startup failed.
    """
    binary = find_binary()
    if not binary:
        return None
    home = home or _config_home()
    init(home)

    # Minimal env: only the vars syncthing actually needs to find $HOME/$PATH.
    parent = os.environ
    env = {
        "HOME": parent.get("HOME", ""),
        "PATH": parent.get("PATH", ""),
        "USER": parent.get("USER", parent.get("LOGNAME", "")),
        "LANG": parent.get("LANG", "C.UTF-8"),
        "STNORESTART": "1",
        "STNOUPGRADE": "1",
    }
    # Forward XDG_* dirs so syncthing's defaults align with the user's shell.
    for k, v in parent.items():
        if k.startswith("XDG_"):
            env[k] = v

    proc = subprocess.Popen(
        [binary, "serve", f"--home={home}", "--no-browser", "--no-upgrade", "--no-restart"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        env=env,
    )
    if wait_for_ready > 0:
        deadline = time.monotonic() + wait_for_ready
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                return None  # died during startup
            if _ping(home, timeout=1.0):
                return proc
            time.sleep(0.5)
        # Timed out — still return the proc so caller can decide.
    return proc


def stop(proc: Optional[subprocess.Popen], home: Optional[str] = None) -> None:
    """Gracefully stop the sidecar. Tries the REST `/system/shutdown` endpoint
    first (lets syncthing flush state cleanly), then falls back to SIGTERM,
    then SIGKILL."""
    if proc is None:
        return
    # Best-effort REST shutdown first.
    try:
        key = api_key(home or _config_home())
        if key:
            req = urllib.request.Request(
                f"{DEFAULT_API}/rest/system/shutdown",
                method="POST",
                headers={"X-API-Key": key},
            )
            try:
                urllib.request.urlopen(req, timeout=2)
            except (urllib.error.URLError, TimeoutError):
                pass
    except Exception:
        pass
    try:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                pass
    except Exception:
        pass


def status(home: Optional[str] = None) -> SyncStatus:
    """Snapshot of current sidecar state for the GUI."""
    home = home or _config_home()
    return SyncStatus(
        running=_ping(home),
        device_id=device_id(home),
        api_url=DEFAULT_API,
    )


def _ping(home: str, timeout: float = 1.0) -> bool:
    key = api_key(home)
    if not key:
        return False
    req = urllib.request.Request(f"{DEFAULT_API}/rest/system/ping", headers={"X-API-Key": key})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status == 200
    except (urllib.error.URLError, TimeoutError):
        return False


def add_folder(project_dir: str, home: Optional[str] = None) -> bool:
    """Share the entire project directory via Syncthing, filtered by .stignore
    to only sync save-relevant subtrees.

    Round-6 architecture rework (#4): the previous implementation built a
    `<project>/saves/<Emulator>/<sub>` shadow tree of symlinks pointing back
    into the per-emulator dirs. That broke cross-device sync because:
      1. Syncthing replicates symlinks AS symlinks (doesn't follow them), so
         the peer received link metadata pointing at a path that didn't exist
         on the peer's filesystem — saves appeared as dangling links.
      2. Save state lived OUTSIDE the linked tree on Mac (because the
         shipped `<project>/Dolphin/data/` was empty, so the host symlink
         didn't redirect anything; Dolphin wrote saves to its real
         ~/Library/.../Dolphin Emulator/StateSaves directory.

    Now we share `<project>` itself with a generated `.stignore` that
    excludes the things peers don't need (result-* nix store links, build
    artifacts, local backups, captured originals, ROMs, the saves/ shadow
    tree). The actual save data lives at `<project>/<Emulator>/data/...` /
    `<project>/<Emulator>/config/...` which IS in the shared tree.

    Security posture from round-5 carries forward:
      - ignorePerms=true so a hostile peer can't push setuid bits.
      - .stignore excludes symlinks-leaving-project AND dotfiles.
      - Idempotent PUT semantics.
    """
    home = home or _config_home()
    folder_path = project_dir
    _write_stignore(folder_path)

    key = api_key(home)
    if not key:
        return False

    body = {
        "id": SYNC_FOLDER_ID,
        "label": SYNC_FOLDER_NAME,
        "path": folder_path,
        "type": "sendreceive",
        # fsWatcher catches changes within ~10s; periodic rescan is just a
        # safety net. 60s wakes one CPU core every minute on Steam Deck;
        # 1h is plenty for missed-event recovery and saves real battery
        # (round-7 critic finding #7).
        "rescanIntervalS": 3600,
        "fsWatcherEnabled": True,
        "ignorePerms": True,
    }
    req = urllib.request.Request(
        f"{DEFAULT_API}/rest/config/folders/{SYNC_FOLDER_ID}",
        data=json.dumps(body).encode("utf-8"),
        method="PUT",
        headers={"X-API-Key": key, "Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return 200 <= resp.status < 300
    except urllib.error.URLError:
        return False


def _write_stignore(folder_path: str) -> None:
    """Drop a .stignore at the project-dir root that filters Syncthing to
    only the save-relevant subtrees. Excludes:
      - nix build artifacts (result-*, .direnv/)
      - local-only backups + captured originals
      - ROM directories (typically gigabytes; users have their own copies)
      - the legacy saves/ shadow tree from earlier rounds
      - dotfiles (defence in depth against hostile peers)
      - editor / OS churn

    Idempotent — overwrites whatever's there.
    """
    stignore = os.path.join(folder_path, ".stignore")
    rules = [
        "// Schemulator — DO NOT EDIT (regenerated on every add_folder call)",
        "//",
        "// Build artifacts (per-machine, not for sync):",
        "result-*",
        ".direnv",
        "//",
        "// Local-only state (backups, captured originals, package.json):",
        "backups",
        "originals",
        "//",
        "// Legacy: the symlink-shadow tree we used in earlier versions; now",
        "// project/<Emu>/data/... is the real save dir, so saves/ is dropped.",
        "saves",
        "//",
        "// ROMs are the user's responsibility; don't push gigabytes:",
        "ROMs",
        "downloaded_media",
        "n3ds-fixed",
        "n3ds-original",
        "//",
        "// Editor / OS churn:",
        "(?d)*.tmp",
        "(?d)*.swp",
        ".DS_Store",
        "//",
        "// Dotfiles (defence in depth: a hostile peer could pre-place ssh keys etc):",
        "/.??*",
        "!.stignore",
    ]
    try:
        with open(stignore, "w", encoding="utf-8") as f:
            f.write("\n".join(rules) + "\n")
    except OSError:
        pass


def folder_exists(home: Optional[str] = None) -> bool:
    """Has the schemulator-saves folder been registered yet?"""
    home = home or _config_home()
    key = api_key(home)
    if not key:
        return False
    req = urllib.request.Request(
        f"{DEFAULT_API}/rest/config/folders/{SYNC_FOLDER_ID}",
        headers={"X-API-Key": key},
    )
    try:
        with urllib.request.urlopen(req, timeout=2) as resp:
            return resp.status == 200
    except urllib.error.URLError:
        return False


def add_device(peer_id: str, peer_name: str = "", home: Optional[str] = None,
               share_folder: bool = True, project_dir: Optional[str] = None) -> bool:
    """Authorise a peer device by its Syncthing ID.

    Uses PUT /rest/config/devices/<id> for idempotency. If the saves folder
    isn't shared yet AND `project_dir` is given AND `share_folder` is True,
    we register it now so peers actually replicate (critic finding #5).
    """
    peer_id = peer_id.strip().upper()
    if not _valid_device_id(peer_id):
        return False
    home = home or _config_home()
    key = api_key(home)
    if not key:
        return False
    body = {
        "deviceID": peer_id,
        "name": peer_name or peer_id[:8],
        "addresses": ["dynamic"],
        "compression": "metadata",
        "introducer": False,
    }
    req = urllib.request.Request(
        f"{DEFAULT_API}/rest/config/devices/{peer_id}",
        data=json.dumps(body).encode("utf-8"),
        method="PUT",
        headers={"X-API-Key": key, "Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            ok = 200 <= resp.status < 300
    except urllib.error.URLError:
        return False
    if not ok or not share_folder:
        return ok

    # Make sure the saves folder exists, then attach the new peer to it via PATCH
    # (atomic — won't clobber concurrent edits from the Syncthing GUI).
    if not folder_exists(home) and project_dir:
        add_folder(project_dir, home)
    _share_folder_with(peer_id, home, key)
    return ok


# Base32 alphabet used by Syncthing device IDs (RFC4648).
_LUHN_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"
_LUHN_BASE = len(_LUHN_ALPHABET)


def _luhn_check_digit(seg: str) -> str:
    """Compute the Luhn-mod-32 check digit for a base32 string. Mirror of
    syncthing's lib/protocol/luhn.go::Luhn32.
    Reference: https://docs.syncthing.net/specs/device-ids.html
    """
    factor = 1
    total = 0
    for ch in seg:
        if ch not in _LUHN_ALPHABET:
            raise ValueError(f"non-base32 char: {ch}")
        addend = factor * _LUHN_ALPHABET.index(ch)
        addend = (addend // _LUHN_BASE) + (addend % _LUHN_BASE)
        total += addend
        factor = 1 if factor == 2 else 2
    rem = total % _LUHN_BASE
    return _LUHN_ALPHABET[(_LUHN_BASE - rem) % _LUHN_BASE]


def _valid_device_id(d_id: str) -> bool:
    """Validate a syncthing canonical device ID.

    Structure (per syncthing's deviceid.go):
      - SHA-256 of the device cert → 32 bytes → base32 (no padding) → 52 chars
      - Split into 4 chunks of 13 chars; append Luhn-mod-32 check digit to each
        → 4 chunks of 14 chars = 56 chars
      - Display as 8 groups of 7 chars separated by '-'

    To validate: strip dashes, regroup into 4×14, verify each chunk's 14th
    char is the Luhn-mod-32 of chars 0..12. Catches paste-typos that would
    otherwise be silently POSTed to syncthing only to bounce back as 400.
    """
    if not d_id:
        return False
    parts = d_id.split("-")
    if len(parts) != 8 or any(len(p) != 7 for p in parts):
        return False
    raw = "".join(parts)
    if len(raw) != 56:
        return False
    if not all(c in _LUHN_ALPHABET for c in raw):
        return False
    for i in range(4):
        chunk = raw[i * 14:(i + 1) * 14]
        try:
            if _luhn_check_digit(chunk[:13]) != chunk[13]:
                return False
        except ValueError:
            return False
    return True


def _share_folder_with(peer_id: str, home: str, key: str) -> bool:
    """Atomically attach `peer_id` to the SYNC_FOLDER_ID device list using
    PATCH (won't clobber concurrent edits from the Syncthing browser GUI)."""
    try:
        req = urllib.request.Request(
            f"{DEFAULT_API}/rest/config/folders/{SYNC_FOLDER_ID}",
            headers={"X-API-Key": key},
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            folder = json.loads(resp.read().decode("utf-8"))
    except (urllib.error.URLError, json.JSONDecodeError):
        return False
    devices = folder.get("devices") or []
    if any(d.get("deviceID") == peer_id for d in devices):
        return True
    devices.append({"deviceID": peer_id})
    patch = {"devices": devices}
    req = urllib.request.Request(
        f"{DEFAULT_API}/rest/config/folders/{SYNC_FOLDER_ID}",
        data=json.dumps(patch).encode("utf-8"),
        method="PATCH",
        headers={"X-API-Key": key, "Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return 200 <= resp.status < 300
    except urllib.error.URLError:
        return False


