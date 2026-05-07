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

# Subdirectories under <project_dir>/<Emulator>/ that hold syncable saves.
# Empty list means "ignore this emulator." The keys must match emulator dir names.
SAVE_PATHS = {
    "RetroArch": ["config/saves", "config/states"],
    "Dolphin":   ["data/GC", "data/Wii", "data/StateSaves"],
    "PCSX2":     ["config/memcards", "config/sstates"],
    "Cemu":      ["data/mlc01/usr/save"],
    "Ryujinx":   ["config/bis_system", "config/bis_user/save"],
    "Azahar":    ["data/sdmc"],
    "Lime3DS":   ["data/sdmc"],
}


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
    """Read this device's Syncthing ID from its config.xml."""
    home = home or _config_home()
    cfg = os.path.join(home, "config.xml")
    if not os.path.exists(cfg):
        return None
    try:
        tree = ET.parse(cfg)
        for d in tree.getroot().findall("device"):
            if d.get("name") and d.get("id"):
                # The first device entry is always the local one.
                return d.get("id")
    except ET.ParseError:
        return None
    return None


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

    Returns the Popen handle (caller terminates it) or None if startup failed.
    """
    binary = find_binary()
    if not binary:
        return None
    home = home or _config_home()
    init(home)
    env = os.environ.copy()
    env["STNORESTART"] = "1"
    env["STNOUPGRADE"] = "1"
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
    """Tell Syncthing to manage <project_dir>/saves/ as the shared folder."""
    home = home or _config_home()
    folder_path = saves_dir(project_dir)
    os.makedirs(folder_path, exist_ok=True)
    _populate_save_links(project_dir, folder_path)

    key = api_key(home)
    if not key:
        return False

    body = {
        "id": SYNC_FOLDER_ID,
        "label": SYNC_FOLDER_NAME,
        "path": folder_path,
        "type": "sendreceive",
        "rescanIntervalS": 60,
        "fsWatcherEnabled": True,
        "ignorePerms": False,
    }
    req = urllib.request.Request(
        f"{DEFAULT_API}/rest/config/folders",
        data=json.dumps(body).encode("utf-8"),
        method="POST",
        headers={"X-API-Key": key, "Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return 200 <= resp.status < 300
    except urllib.error.URLError:
        return False


def add_device(peer_id: str, peer_name: str = "", home: Optional[str] = None,
               share_folder: bool = True) -> bool:
    """Authorise a peer device by its Syncthing ID.

    If `share_folder` is True (the default), the schemulator-saves folder is
    automatically shared with the new device so saves start replicating
    immediately.
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
        f"{DEFAULT_API}/rest/config/devices",
        data=json.dumps(body).encode("utf-8"),
        method="POST",
        headers={"X-API-Key": key, "Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            ok = 200 <= resp.status < 300
    except urllib.error.URLError:
        return False
    if ok and share_folder:
        _share_folder_with(peer_id, home, key)
    return ok


def _valid_device_id(d_id: str) -> bool:
    """A syncthing device ID is 7 dash-separated 7-char base32 chunks (56 chars)."""
    parts = d_id.split("-")
    if len(parts) != 8:
        return False
    return all(len(p) == 7 and p.isalnum() for p in parts)


def _share_folder_with(peer_id: str, home: str, key: str) -> bool:
    """Add `peer_id` to the SYNC_FOLDER_ID device list. Best-effort."""
    try:
        # Fetch current folder config
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
    folder["devices"] = devices
    req = urllib.request.Request(
        f"{DEFAULT_API}/rest/config/folders/{SYNC_FOLDER_ID}",
        data=json.dumps(folder).encode("utf-8"),
        method="PUT",
        headers={"X-API-Key": key, "Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return 200 <= resp.status < 300
    except urllib.error.URLError:
        return False


def saves_dir(project_dir: str) -> str:
    return os.path.join(project_dir, "saves")


def _populate_save_links(project_dir: str, saves_root: str) -> None:
    """Create symlinks from <saves_root>/<Emulator>/<subdir> into the canonical
    save locations under <project_dir>/<Emulator>/."""
    for emulator, subdirs in SAVE_PATHS.items():
        emu_dir = os.path.join(project_dir, emulator)
        if not os.path.isdir(emu_dir):
            continue
        for sub in subdirs:
            target = os.path.join(emu_dir, sub)
            if not os.path.exists(target):
                continue
            link_dir = os.path.join(saves_root, emulator)
            os.makedirs(link_dir, exist_ok=True)
            link = os.path.join(link_dir, os.path.basename(sub))
            if os.path.lexists(link):
                continue
            try:
                os.symlink(target, link)
            except OSError:
                pass


def device_id_qr_payload(d_id: str) -> str:
    """Format a device ID as a QR payload that Syncthing's mobile apps recognise."""
    return f"syncthing://{d_id}"
