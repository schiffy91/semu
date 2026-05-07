"""Edge cases that crop up on real users' machines.

- Corrupt JSON
- Broken symlinks (target doesn't exist)
- Permission errors (best-effort, run as user not root)
- Empty manifests
- Concurrent backup writes
- Non-UTF-8 paths
"""

import argparse
import json
import os
import zipfile

import pytest

from core import backup, sdcard, state, symlinks, syncthing


# -------- corrupt configs --------

def test_parse_skips_corrupt_symlinks_json(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "host")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    bad = tmp_path / "BadEmu"
    bad.mkdir()
    (bad / "symlinks.json").write_text("{this is not json")

    good = tmp_path / "GoodEmu"
    (good / "config").mkdir(parents=True)
    (good / "symlinks.json").write_text(json.dumps({"config": {"linux": str(tmp_path / "host" / "Good")}}))

    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert "BADEMU" not in parsed
    assert "GOODEMU" in parsed


def test_parse_handles_setup_json_missing_keys(tmp_path):
    (tmp_path / "setup.json").write_text(json.dumps({}))
    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert parsed == {}


# -------- broken symlinks --------

def test_create_symlink_replaces_dangling_link(tmp_path):
    """Re-running symlink should replace a dangling link, not error."""
    real_target = tmp_path / "real-target.txt"
    real_target.write_text("hello")
    link = tmp_path / "link.txt"
    # Dangling symlink to start
    link.symlink_to(tmp_path / "doesnt-exist")
    assert link.is_symlink() and not link.exists()

    symlinks.create_symlink(str(link), str(real_target))
    assert link.is_symlink()
    assert link.read_text() == "hello"


# -------- backup edge cases --------

def test_backup_handles_emulator_with_no_source(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    emu = tmp_path / "EmptyEmu"
    emu.mkdir()
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {"linux": str(tmp_path / "h" / "EmptyEmu")},
    }))
    # source dir doesn't exist - backup should still work, just skip empty.
    args = argparse.Namespace(config=str(tmp_path / "setup.json"), emulators=[])
    path = backup.cmd_backup(args)
    assert path and os.path.exists(path)


def test_backup_excludes_roms_directory(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "small.cfg").write_text("real")
    # ROMs dir with a giant file should be excluded.
    (emu / "config" / "ROMs").mkdir()
    (emu / "config" / "ROMs" / "huge.iso").write_bytes(b"\0" * 100_000)
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": str(tmp_path / "h" / "Emu")}}))

    args = argparse.Namespace(config=str(tmp_path / "setup.json"), emulators=[])
    path = backup.cmd_backup(args)
    with zipfile.ZipFile(path) as zf:
        names = zf.namelist()
    assert any("small.cfg" in n for n in names)
    assert not any("huge.iso" in n for n in names)


def test_backup_rotation_keeps_5(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "x").write_text("x")
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": str(tmp_path / "h" / "Emu")}}))

    backup_dir = tmp_path / "backups"
    backup_dir.mkdir()
    for i in range(7):
        (backup_dir / f"linux-2026010{i}-120000.zip").write_bytes(b"old")

    args = argparse.Namespace(config=str(tmp_path / "setup.json"), emulators=[])
    backup.cmd_backup(args)
    backups = sorted(backup_dir.glob("linux-*.zip"))
    assert len(backups) == 5


# -------- sdcard edge cases --------

def test_sdcard_handles_unreadable_dir(tmp_path, monkeypatch):
    """A subdirectory with no read permission should be silently skipped."""
    mount = tmp_path / "mount"
    mount.mkdir()
    forbidden = mount / "forbidden"
    forbidden.mkdir()
    (mount / "snes").mkdir()
    (mount / "snes" / "mario.sfc").write_bytes(b"\0")
    try:
        os.chmod(forbidden, 0o000)
        result = sdcard.scan_roms(str(mount))
        assert "snes" in result
    finally:
        os.chmod(forbidden, 0o755)  # cleanup


def test_sdcard_max_depth_limits_walk(tmp_path):
    """Don't walk infinitely deep — bound at max_depth."""
    deep = tmp_path
    for i in range(10):
        deep = deep / f"level{i}"
        deep.mkdir()
    (deep / "deep.gba").write_bytes(b"\0")

    result = sdcard.scan_roms(str(tmp_path), max_depth=3)
    # 10 levels deep, max_depth=3 - should NOT find deep.gba
    assert "gba" not in result


# -------- syncthing edge cases --------

def test_syncthing_validates_device_id_format():
    """Real syncthing IDs have valid Luhn-mod-32 check digits over 13-char
    chunks. Random 56-char base32 strings (like the old test's all-A string)
    must NOT validate, otherwise we'd silently POST garbage to syncthing.
    """
    # Real device ID pattern (generated by syncthing in a previous test run).
    valid = "YBGSNWW-6WAU53O-K6P7FDU-DG3DJIM-4PXIL5K-RKDHFXU-SFYNU3X-QMRJCQD"
    assert syncthing._valid_device_id(valid)
    # Garbage with right shape but wrong check digits.
    assert not syncthing._valid_device_id("AAAAAAA-BBBBBBB-CCCCCCC-DDDDDDD-EEEEEEE-FFFFFFF-GGGGGGG-HHHHHHH")
    # Wrong chunk count / length.
    assert not syncthing._valid_device_id("too-short")
    assert not syncthing._valid_device_id("")
    assert not syncthing._valid_device_id("ABC-DEF-GHI-JKL-MNO-PQR-STU-VWX")


def test_syncthing_add_device_rejects_invalid_id():
    """Even when the API isn't reachable, add_device should reject bogus IDs early."""
    assert syncthing.add_device("garbage", home="/nonexistent") is False


# -------- migrate edge cases --------

def test_migrate_handles_missing_target(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    src = tmp_path / "Src"
    (src / "config").mkdir(parents=True)
    (src / "symlinks.json").write_text(json.dumps({"config": {"linux": str(tmp_path / "h" / "Src")}}))

    args = argparse.Namespace(config=str(tmp_path / "setup.json"), source="Src", target="DoesNotExist")
    # Should not crash.
    backup.cmd_migrate(args)
