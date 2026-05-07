"""Self-update + emulator-version manifest fetcher.

The manifest lives at the URL configured in `settings.json` (default: GitHub
release asset). It looks like:

    {
        "schemulator_version": "1.4.0",
        "emulators": {
            "dolphin":  {"version": "2603a", "channel": "stable"},
            "pcsx2":    {"version": "2.6.3"},
            ...
        }
    }

This module is intentionally network-light: the GUI fetches the manifest, and
core.lifecycle.update is what actually rebuilds via Nix.
"""

import hashlib
import json
import os
import shutil
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Dict, Optional


DEFAULT_MANIFEST_URL = (
    "https://github.com/schiffy91/schemulator/releases/latest/download/manifest.json"
)


@dataclass
class Manifest:
    schemulator_version: str
    emulators: Dict[str, Dict[str, str]]


def fetch_manifest(url: str = DEFAULT_MANIFEST_URL, timeout: float = 10.0) -> Optional[Manifest]:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
        return None
    return Manifest(
        schemulator_version=str(data.get("schemulator_version", "")),
        emulators=data.get("emulators", {}) or {},
    )


def installed_versions(project_dir: str) -> Dict[str, str]:
    """Resolve the version of each installed emulator.

    Looks at:
      1. `<project_dir>/<Emulator>/version.txt` (manually-written override)
      2. `<project_dir>/result-<emulator>/` symlink target (nix store path).
         The store path looks like `/nix/store/<hash>-<name>-<version>` so we
         extract the trailing `-<version>` chunk.
      3. `<project_dir>/result-<emulator>/version.txt` if the package writes one.
    """
    import re

    out: Dict[str, str] = {}
    if not os.path.isdir(project_dir):
        return out

    # 1) explicit version.txt override (highest priority).
    for entry in os.listdir(project_dir):
        version_file = os.path.join(project_dir, entry, "version.txt")
        if os.path.isfile(version_file):
            try:
                with open(version_file) as f:
                    out[entry.lower()] = f.read().strip()
            except OSError:
                pass

    # 2) result-<emu> symlinks → derive version from store path. Skip emulators
    # we already have an explicit version for (critic finding #10).
    for entry in os.listdir(project_dir):
        if not entry.startswith("result-"):
            continue
        if entry.endswith(("-prev", "-staging")):
            continue
        emu = entry[len("result-"):]
        if emu in out:
            continue
        result_path = os.path.join(project_dir, entry)
        if not os.path.islink(result_path):
            continue
        # Try a per-package version.txt first.
        ver_path = os.path.join(result_path, "version.txt")
        if os.path.exists(ver_path):
            try:
                with open(ver_path) as f:
                    out[emu] = f.read().strip()
                    continue
            except OSError:
                pass
        # Mine the nix store name. Pattern: /nix/store/<hash>-<rest>. The
        # version is the rightmost chunk that starts with a digit (handles
        # "dolphin-emu-2603a" → "2603a", "cemu-2.0-105" → "2.0-105",
        # "pcsx2-2.6.3-456-g4a5b6c" → "2.6.3-456-g4a5b6c"). Critic finding #11.
        target = os.readlink(result_path)
        base = os.path.basename(target.rstrip("/"))
        # Strip leading <hash>- (32 lowercase chars + hyphen).
        match = re.match(r"^[a-z0-9]{16,}-(.+)$", base)
        rest = match.group(1) if match else base
        # Find the FIRST chunk that begins with a digit; the version is
        # everything from that chunk onward.
        chunks = rest.split("-")
        for i, c in enumerate(chunks):
            if c and c[0].isdigit():
                out[emu] = "-".join(chunks[i:])
                break
        else:
            out[emu] = "installed"

    return out


def has_update(installed: Dict[str, str], manifest: Manifest) -> Dict[str, str]:
    """Return {emulator: latest_version} for every emulator with a newer build."""
    diffs: Dict[str, str] = {}
    for name, info in manifest.emulators.items():
        latest = info.get("version", "")
        current = installed.get(name.lower(), "")
        if latest and latest != current:
            diffs[name] = latest
    return diffs


def stage_download(url: str, dest: str, expected_sha256: str = "", chunk: int = 1 << 14) -> bool:
    """Stream `url` into `dest`. Verifies sha256 if provided."""
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    h = hashlib.sha256()
    try:
        with urllib.request.urlopen(url, timeout=30) as resp, open(dest, "wb") as f:
            while True:
                buf = resp.read(chunk)
                if not buf:
                    break
                h.update(buf)
                f.write(buf)
    except urllib.error.URLError:
        return False
    if expected_sha256 and h.hexdigest().lower() != expected_sha256.lower():
        os.remove(dest)
        return False
    return True


def atomic_swap(staging: str, current: str, rollback: str) -> bool:
    """Atomically swap `current` -> `staging`, retaining the old `current` as `rollback`."""
    if not os.path.isdir(staging):
        return False
    if os.path.lexists(rollback):
        if os.path.islink(rollback):
            os.unlink(rollback)
        else:
            shutil.rmtree(rollback)
    if os.path.lexists(current):
        os.rename(current, rollback)
    os.rename(staging, current)
    return True
