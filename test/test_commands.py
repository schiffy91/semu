import argparse
import json
import os
import zipfile

import setup


def _config_path(mock_project):
    return str(mock_project / "setup.json")


def test_backup_creates_zip(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)
    args = argparse.Namespace(config=_config_path(mock_project), emulators=[])
    setup.cmd_backup(args)

    backups = list((mock_project / "backups").glob("*.zip"))
    assert len(backups) == 1
    with zipfile.ZipFile(backups[0]) as zf:
        names = zf.namelist()
        assert any("settings.ini" in n for n in names)


def test_backup_rotation(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)

    # Create 7 dummy backup files to test rotation
    backup_dir = mock_project / "backups"
    backup_dir.mkdir(exist_ok=True)
    for i in range(7):
        (backup_dir / f"{setup.PLATFORM}-2026010{i}-120000.zip").write_bytes(b"fake")

    args = argparse.Namespace(config=_config_path(mock_project), emulators=[])
    setup.cmd_backup(args)

    backups = list(backup_dir.glob(f"{setup.PLATFORM}-*.zip"))
    assert len(backups) == 5


def test_capture_creates_snapshot_with_readonly_dir(mock_project):
    """The snapshot DIRECTORY is read-only (so a typo `rm -rf` fails) but the
    contained FILES stay writable so cmd_revert can restore them and the
    emulator can rewrite its config at next run (critic finding #13)."""
    args = argparse.Namespace(emulator="TestEmu", version="v1.0", config=_config_path(mock_project))
    setup.cmd_capture(args)

    snapshot_dir = mock_project / "TestEmu" / "originals" / "v1.0"
    assert snapshot_dir.exists()
    assert snapshot_dir.is_dir()
    # Directory should NOT have user-write bit set.
    dir_mode = snapshot_dir.stat().st_mode & 0o777
    assert not (dir_mode & 0o200), oct(dir_mode)
    # Contained file should be writable so revert+emulator-startup work.
    snapshot_file = snapshot_dir / "config" / "settings.ini"
    assert snapshot_file.exists()
    file_mode = snapshot_file.stat().st_mode & 0o777
    assert file_mode & 0o200, oct(file_mode)


def test_capture_appends_to_manifest(mock_project):
    cfg = _config_path(mock_project)
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v1.0", config=cfg))
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v2.0", config=cfg))

    with open(mock_project / "TestEmu" / "originals" / "manifest.json") as f:
        manifest = json.load(f)
    assert len(manifest) == 2
    assert manifest[0]["version"] == "v1.0"
    assert manifest[1]["version"] == "v2.0"


def test_capture_rejects_duplicate_version(mock_project):
    cfg = _config_path(mock_project)
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v1.0", config=cfg))
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v1.0", config=cfg))

    with open(mock_project / "TestEmu" / "originals" / "manifest.json") as f:
        manifest = json.load(f)
    assert len(manifest) == 1


def test_revert_restores_snapshot(mock_project):
    cfg = _config_path(mock_project)
    # Capture original
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v1.0", config=cfg))

    # Modify the config
    config_file = mock_project / "TestEmu" / "config" / "settings.ini"
    config_file.write_text("[General]\nfullscreen=false\nmodified=true\n")

    # Revert
    setup.cmd_revert(argparse.Namespace(emulator="TestEmu", version="v1.0", config=cfg))

    assert config_file.read_text() == "[General]\nfullscreen=true\n"


def test_migrate_copies_matching_entries(mock_project):
    cfg = _config_path(mock_project)
    # Create a second emulator with same structure
    emu2 = mock_project / "TargetEmu"
    emu2.mkdir()
    (emu2 / "config").mkdir()
    (emu2 / "config" / "settings.ini").write_text("[General]\nempty=true\n")
    symlinks2 = {
        "config": {
            "windows": str(mock_project / "host_win" / "TargetEmu"),
            "linux": str(mock_project / "host_linux" / "TargetEmu"),
            "macos": str(mock_project / "host_macos" / "TargetEmu"),
        }
    }
    (emu2 / "symlinks.json").write_text(json.dumps(symlinks2))

    setup.cmd_migrate(argparse.Namespace(source="TestEmu", target="TargetEmu", config=cfg))

    assert (emu2 / "config" / "settings.ini").read_text() == "[General]\nfullscreen=true\n"
