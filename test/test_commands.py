import argparse
import json
import os
import zipfile

import setup


def test_backup_creates_zip(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)
    args = argparse.Namespace(config=str(mock_project / "setup.json"), emulators=[])
    setup.parse_config(str(mock_project / "setup.json"), str(mock_project))
    setup.cmd_backup(args)

    backups = list((mock_project / "backups").glob("*.zip"))
    assert len(backups) == 1
    with zipfile.ZipFile(backups[0]) as zf:
        names = zf.namelist()
        assert any("settings.ini" in n for n in names)


def test_backup_rotation(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)
    setup.parse_config(str(mock_project / "setup.json"), str(mock_project))

    # Create 7 dummy backup files to test rotation
    backup_dir = mock_project / "backups"
    backup_dir.mkdir(exist_ok=True)
    for i in range(7):
        (backup_dir / f"{setup.PLATFORM}-2026010{i}-120000.zip").write_bytes(b"fake")

    # Run one real backup (triggers rotation)
    args = argparse.Namespace(config=str(mock_project / "setup.json"), emulators=[])
    setup.cmd_backup(args)

    backups = list(backup_dir.glob(f"{setup.PLATFORM}-*.zip"))
    assert len(backups) == 5  # rotated to max 5


def test_capture_creates_readonly_snapshot(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)
    args = argparse.Namespace(emulator="TestEmu", version="v1.0", config=None)
    setup.cmd_capture(args)

    snapshot = mock_project / "TestEmu" / "originals" / "v1.0" / "config" / "settings.ini"
    assert snapshot.exists()
    # Check permission bits are read-only (root can still write, so check mode not access)
    mode = oct(snapshot.stat().st_mode)
    assert mode.endswith("444")  # r--r--r--


def test_capture_appends_to_manifest(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v1.0", config=None))
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v2.0", config=None))

    manifest = json.load(open(mock_project / "TestEmu" / "originals" / "manifest.json"))
    assert len(manifest) == 2
    assert manifest[0]["version"] == "v1.0"
    assert manifest[1]["version"] == "v2.0"


def test_capture_rejects_duplicate_version(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v1.0", config=None))
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v1.0", config=None))

    manifest = json.load(open(mock_project / "TestEmu" / "originals" / "manifest.json"))
    assert len(manifest) == 1  # didn't duplicate


def test_revert_restores_snapshot(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)
    # Capture original
    setup.cmd_capture(argparse.Namespace(emulator="TestEmu", version="v1.0", config=None))

    # Modify the config
    config_file = mock_project / "TestEmu" / "config" / "settings.ini"
    config_file.write_text("[General]\nfullscreen=false\nmodified=true\n")

    # Revert
    setup.parse_config(str(mock_project / "setup.json"), str(mock_project))
    setup.cmd_revert(argparse.Namespace(
        emulator="TestEmu", version="v1.0",
        config=str(mock_project / "setup.json")
    ))

    # Check config was restored
    assert config_file.read_text() == "[General]\nfullscreen=true\n"


def test_migrate_copies_matching_entries(mock_project, monkeypatch):
    monkeypatch.chdir(mock_project)
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

    setup.parse_config(str(mock_project / "setup.json"), str(mock_project))
    setup.cmd_migrate(argparse.Namespace(
        source="TestEmu", target="TargetEmu",
        config=str(mock_project / "setup.json")
    ))

    # Target should now have source's config
    assert (emu2 / "config" / "settings.ini").read_text() == "[General]\nfullscreen=true\n"
