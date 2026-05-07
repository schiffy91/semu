"""Tests for round-8 (reliability) + round-9 (cross-platform) + round-10
(maintainability) hardening fixes."""

import argparse
import json
import os

import pytest

from core import lifecycle, sdcard, state, steam
from core.lock import BusyError, project_lock


# ----- R8-H1: process-level lock prevents concurrent lifecycle ops -----

def test_project_lock_excludes_concurrent_acquirers(tmp_path):
    """Two processes can't both hold the lock. Second acquirer raises BusyError."""
    project = tmp_path / "project"
    project.mkdir()
    with project_lock(str(project)):
        with pytest.raises(BusyError):
            with project_lock(str(project)):
                pass


def test_project_lock_release_on_context_exit(tmp_path):
    project = tmp_path / "project"
    project.mkdir()
    with project_lock(str(project)):
        pass
    # After context exit, can re-acquire.
    with project_lock(str(project)):
        pass


# ----- R8-H8: shortcuts.vdf preserved on parse failure -----

def test_upsert_shortcut_preserves_corrupt_existing(tmp_path):
    """If shortcuts.vdf is non-empty but parses to nothing, the existing
    bytes are preserved as <name>.broken-<ts> and a fresh file is written
    instead of silently nuking the user's other non-Steam games."""
    path = str(tmp_path / "shortcuts.vdf")
    # Write garbage that's clearly non-empty.
    with open(path, "wb") as f:
        f.write(b"\x99corrupt\x00data" * 100)

    s = steam.Shortcut(appname="MyGame", exe="/usr/bin/mygame")
    steam.upsert_shortcut(path, s)

    # The corrupt file got renamed.
    broken = list(tmp_path.glob("shortcuts.vdf.broken-*"))
    assert len(broken) == 1
    # The new file decodes to one entry (ours).
    with open(path, "rb") as f:
        decoded = steam.decode_shortcuts(f.read())
    assert any(d.appname == "MyGame" for d in decoded)


def test_upsert_shortcut_normal_path_doesnt_rescue(tmp_path):
    """A correctly-formed existing file is preserved (no .broken file)."""
    path = str(tmp_path / "shortcuts.vdf")
    steam.upsert_shortcut(path, steam.Shortcut(appname="A", exe="/a"))
    steam.upsert_shortcut(path, steam.Shortcut(appname="B", exe="/b"))
    broken = list(tmp_path.glob("shortcuts.vdf.broken-*"))
    assert broken == []


# ----- R8-H4: orphan .tmp cleanup -----

def test_backup_sweeps_old_tmp_files(tmp_path, monkeypatch):
    """Stale .zip.tmp files older than an hour are deleted on entry."""
    import time
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    backup_dir = tmp_path / "backups"
    backup_dir.mkdir()
    old_tmp = backup_dir / "linux-old.zip.tmp"
    old_tmp.write_bytes(b"x")
    # Set mtime to 2 hours ago.
    os.utime(str(old_tmp), (time.time() - 7200, time.time() - 7200))

    fresh_tmp = backup_dir / "linux-fresh.zip.tmp"
    fresh_tmp.write_bytes(b"y")

    from core.backup import _sweep_stale_tmp_zips
    _sweep_stale_tmp_zips(str(backup_dir))

    assert not old_tmp.exists()
    assert fresh_tmp.exists()  # under the threshold


# ----- R8-H5: revert/migrate uses copytree-into-tmp-then-swap -----

def test_revert_does_not_destroy_dst_before_copy_succeeds(tmp_path, monkeypatch):
    """If copytree fails partway, the original destination must still exist."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "settings.ini").write_text("real")
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": "/x"}}))

    cfg_path = str(tmp_path / "setup.json")
    from core.backup import cmd_capture, cmd_revert
    cmd_capture(argparse.Namespace(config=cfg_path, emulator="Emu", version="v1"))

    # Sabotage copytree to fail mid-revert.
    import shutil as _shutil
    real_copytree = _shutil.copytree

    def boom(src, dst, *a, **kw):
        # Run real copytree to start, then raise once the .swap.tmp dir exists.
        if dst.endswith(".swap.tmp") and not os.path.exists(dst):
            real_copytree(src, dst, *a, **kw)
        raise RuntimeError("simulated I/O error")

    monkeypatch.setattr(_shutil, "copytree", boom)
    with pytest.raises(RuntimeError):
        cmd_revert(argparse.Namespace(config=cfg_path, emulator="Emu", version="v1"))

    # The original config must still exist (never destroyed).
    assert (emu / "config" / "settings.ini").exists()
    assert (emu / "config" / "settings.ini").read_text() == "real"


# ----- R9-H2: macOS .app shortcut wraps with `open` -----

def test_shortcut_for_app_wraps_with_open():
    """A discovered .app gets Exe=/usr/bin/open + LaunchOptions=-a <path>,
    not the .app path directly (Steam can't launch a directory)."""
    from gui.dialogs.steamdeck import _shortcut_for_discovered
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    import sys as _sys
    from PySide6.QtWidgets import QApplication
    QApplication.instance() or QApplication(_sys.argv)

    from core.steam import DiscoveredEmulator
    emu = DiscoveredEmulator(
        name="dolphin",
        exe="/Applications/Schemulator/Dolphin.app",
        kind="app",
    )
    sc = _shortcut_for_discovered(emu)
    assert sc.exe == "/usr/bin/open"
    assert "/Applications/Schemulator/Dolphin.app" in sc.launch_options
    assert sc.launch_options.startswith("-a ")


def test_shortcut_for_binary_passes_exe_directly():
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    import sys as _sys
    from PySide6.QtWidgets import QApplication
    QApplication.instance() or QApplication(_sys.argv)

    from gui.dialogs.steamdeck import _shortcut_for_discovered
    from core.steam import DiscoveredEmulator
    emu = DiscoveredEmulator(name="retroarch", exe="/usr/bin/retroarch", kind="binary")
    sc = _shortcut_for_discovered(emu)
    assert sc.exe == "/usr/bin/retroarch"


# ----- R9-H3: macOS ES-DE settings dir -----

def test_es_de_settings_dir_macos_default(monkeypatch, tmp_path):
    """On macOS without a portable marker, settings live under
    ~/Library/Application Support/ES-DE/."""
    monkeypatch.setattr(state, "PLATFORM", "macos")
    monkeypatch.setenv("HOME", str(tmp_path))
    path = sdcard._es_de_settings_dir()
    assert "Library/Application Support/ES-DE" in path


def test_es_de_settings_dir_macos_portable_marker(monkeypatch, tmp_path):
    """If ~/ES-DE/.portable.txt exists on macOS, prefer the portable layout."""
    monkeypatch.setattr(state, "PLATFORM", "macos")
    monkeypatch.setenv("HOME", str(tmp_path))
    portable = tmp_path / "ES-DE"
    portable.mkdir()
    (portable / ".portable.txt").write_text("")
    path = sdcard._es_de_settings_dir()
    assert path.endswith(os.path.join("ES-DE", "settings"))
    assert "Library/Application Support" not in path


def test_es_de_settings_dir_linux(monkeypatch, tmp_path):
    """On Linux, always ~/ES-DE/settings."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    monkeypatch.setenv("HOME", str(tmp_path))
    path = sdcard._es_de_settings_dir()
    assert path.endswith(os.path.join("ES-DE", "settings"))


# ----- R10-1: dead code removed -----

def test_save_paths_constant_is_gone():
    """SAVE_PATHS was the saves/ shadow tree config; round-6 reworked sync
    to share the project dir directly so it's no longer needed."""
    from core import syncthing
    assert not hasattr(syncthing, "SAVE_PATHS")
    assert not hasattr(syncthing, "saves_dir")
    assert not hasattr(syncthing, "_populate_save_links")


def test_unused_updater_helpers_are_gone():
    """has_update / stage_download / atomic_swap had no live callers after
    lifecycle.update grew its own rename logic."""
    from core import updater
    assert not hasattr(updater, "has_update")
    assert not hasattr(updater, "stage_download")
    assert not hasattr(updater, "atomic_swap")


def test_unused_flatpak_helpers_are_gone():
    from core import flatpak
    assert not hasattr(flatpak, "uninstall")
    assert not hasattr(flatpak, "sandbox_dir")


# ----- R10-3: DRY_RUN re-export gone -----

def test_dry_run_reexport_removed():
    """The exec.py DRY_RUN snapshot was a footgun: setting it did nothing
    because consumers read state.DRY_RUN. Removed entirely."""
    from core import exec as _exec, __init__ as _init  # noqa: F401
    import core
    assert not hasattr(_exec, "DRY_RUN")
    assert "DRY_RUN" not in (core.__all__ or [])
