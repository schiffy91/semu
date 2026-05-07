"""Tests for round-7 performance hardening fixes."""

import os
import subprocess
import time

import pytest

from core import lifecycle, sdcard, state, symlinks


# ----- R7-1: parse_config caches the flatpak list -----

def test_parse_config_calls_flatpak_at_most_once(tmp_path, monkeypatch):
    """A user with 8 emulators must NOT trigger 8 flatpak subprocess calls
    on every parse_config — that's >100ms × 8 of GUI thread freeze
    on a slow flatpakd."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text("{}")
    import json as _json
    (tmp_path / "setup.json").write_text(_json.dumps(cfg))

    for name in ("A", "B", "C", "D", "E", "F", "G", "H"):
        d = tmp_path / name
        (d / "config").mkdir(parents=True)
        (d / "symlinks.json").write_text(_json.dumps({
            "flatpak": f"org.test.{name}",
            "config": {"linux": "${host}/X/"},
        }))

    calls = {"n": 0}
    real_run = subprocess.run

    def counting_run(*args, **kw):
        if args and isinstance(args[0], list) and args[0][:2] == ["flatpak", "list"]:
            calls["n"] += 1
            return type("R", (), {"stdout": "", "returncode": 0})()
        return real_run(*args, **kw)

    monkeypatch.setattr("subprocess.run", counting_run)
    # Pretend `flatpak` is on PATH so _list_user_flatpaks tries to run it.
    import shutil as _sh
    monkeypatch.setattr(_sh, "which", lambda name: "/fake/flatpak" if name == "flatpak" else None)

    symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert calls["n"] <= 1, f"flatpak should be called once at most, got {calls['n']}"


# ----- R7-3: update backs up only emulators it's about to update -----

def test_update_skips_backup_when_nothing_outdated(tmp_path, monkeypatch):
    """If filter_outdated drops everything, no backup zip should be created."""
    import json as _json
    cfg = {"host": {"linux": str(tmp_path / "h")},
           "portable": {"linux": str(tmp_path / "p")},
           "macos": {}, "windows": {}}
    cfg = {"host": {"linux": str(tmp_path / "h"), "macos": str(tmp_path / "h"), "windows": str(tmp_path / "h")},
           "portable": {"linux": str(tmp_path / "p"), "macos": str(tmp_path / "p"), "windows": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(_json.dumps(cfg))
    emu = tmp_path / "Dolphin"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "x").write_text("x")
    (emu / "symlinks.json").write_text(_json.dumps({
        "config": {"linux": "/x", "macos": "/x", "windows": "/x"},
    }))
    # Pretend the manifest matches what's installed: nothing to update.
    from core import updater
    monkeypatch.setattr(
        updater,
        "fetch_manifest",
        lambda *a, **kw: updater.Manifest(
            schemulator_version="1.0",
            emulators={"dolphin": {"version": "v1"}},
        ),
    )
    monkeypatch.setattr(updater, "installed_versions", lambda *a, **kw: {"dolphin": "v1"})

    import argparse
    args = argparse.Namespace(config=str(tmp_path / "setup.json"), emulators=["Dolphin"])
    n = lifecycle.update(args)
    assert n == 0
    # No backup zip should exist — round-7 #3.
    backups = list((tmp_path / "backups").glob("*.zip")) if (tmp_path / "backups").exists() else []
    assert backups == []


# ----- R7-6: SD scan honours _MIN_ROM_SIZE and skips non-ROM extensions -----

def test_scan_roms_drops_save_state_files(tmp_path):
    """Per-game state files (.s00, .srm) must NOT show up as ROMs in the
    EmuDeck-layout scanner. Round-7 #6."""
    roms = tmp_path / "Emulation" / "roms" / "snes"
    roms.mkdir(parents=True)
    (roms / "real-rom.sfc").write_bytes(b"\x00" * 2048)
    (roms / "real-rom.srm").write_bytes(b"\x00" * 2048)  # save: must be skipped
    (roms / "real-rom.s00").write_bytes(b"\x00" * 2048)  # save state: must be skipped
    (roms / "screenshot.png").write_bytes(b"\x00" * 2048)

    out = sdcard.scan_roms(str(tmp_path))
    assert "snes" in out
    files = [os.path.basename(p) for p in out["snes"]]
    assert "real-rom.sfc" in files
    assert "real-rom.srm" not in files
    assert "real-rom.s00" not in files
    assert "screenshot.png" not in files


def test_scan_roms_drops_undersized_files(tmp_path):
    """A 50-byte 'rom' is almost certainly a stub or save-state remnant."""
    roms = tmp_path / "Emulation" / "roms" / "gba"
    roms.mkdir(parents=True)
    (roms / "real.gba").write_bytes(b"\x00" * 2048)
    (roms / "stub.gba").write_bytes(b"\x00" * 50)

    out = sdcard.scan_roms(str(tmp_path))
    files = [os.path.basename(p) for p in out.get("gba", [])]
    assert "real.gba" in files
    assert "stub.gba" not in files


# ----- R7-7: syncthing rescanIntervalS bumped -----

def test_add_folder_uses_long_rescan_interval(tmp_path, monkeypatch):
    """rescanIntervalS=3600 means we rely on fsWatcher for 99% of changes
    and only do a periodic full rescan as a safety net. Round-7 #7."""
    from core import syncthing
    payload = {}

    class _R:
        status = 200
        def __enter__(self): return self
        def __exit__(self, *a): pass
        def read(self): return b""

    def capture_open(req, **kw):
        # Snapshot the body sent to syncthing's REST.
        body = req.data.decode("utf-8") if req.data else ""
        if "/rest/config/folders/" in req.full_url and req.method == "PUT":
            import json as _json
            payload.update(_json.loads(body))
        return _R()

    monkeypatch.setattr(syncthing, "api_key", lambda *a, **kw: "fake-key")
    import urllib.request as _ur
    monkeypatch.setattr(_ur, "urlopen", capture_open)

    project = tmp_path / "project"
    project.mkdir()
    syncthing.add_folder(str(project))
    assert payload.get("rescanIntervalS") == 3600
    assert payload.get("fsWatcherEnabled") is True


# ----- R7-4: _seed_project_dir copies symlinks.json only (no full copytree) -----

def test_seed_project_dir_only_copies_symlinks_json(tmp_path):
    """The previous version did shutil.copytree per emulator dir on the GUI
    thread, freezing the UI on slow project-dir storage. Now only the
    tiny symlinks.json files get seeded. Round-7 #4."""
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    import sys
    from PySide6.QtWidgets import QApplication
    QApplication.instance() or QApplication(sys.argv)

    from gui.main_window import MainWindow
    w = MainWindow()
    target = tmp_path / "fresh"
    started = time.monotonic()
    w._seed_project_dir(str(target))
    elapsed = time.monotonic() - started
    # On any reasonable machine, seeding 8 small JSON files is well under 1s.
    # The previous shutil.copytree path would frequently exceed this on
    # SD-card targets.
    assert elapsed < 1.0, f"seed took {elapsed:.2f}s, expected < 1s"
    # setup.json present.
    assert (target / "setup.json").exists()
    # At least one emulator dir with its symlinks.json copied through.
    seeded_manifests = list(target.glob("*/symlinks.json"))
    assert len(seeded_manifests) >= 1
