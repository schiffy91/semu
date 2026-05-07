"""Tests for the migration dialog, first-run wizard, and logging."""

import argparse
import json
import logging
import os
import sys

import pytest


pytest.importorskip("PySide6")

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication  # noqa: E402


@pytest.fixture(scope="session")
def qapp():
    return QApplication.instance() or QApplication(sys.argv)


@pytest.fixture
def fake_project(tmp_path):
    cfg = {
        "host":     {"linux": str(tmp_path / "host"), "macos": str(tmp_path / "host"), "windows": str(tmp_path / "host")},
        "portable": {"linux": str(tmp_path / "p"),    "macos": str(tmp_path / "p"),    "windows": str(tmp_path / "p")},
    }
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    for name in ("Source", "Target", "Solo"):
        emu = tmp_path / name
        (emu / "config").mkdir(parents=True)
        (emu / "config" / "settings.ini").write_text("[General]\nv=1\n")
        manifest = {"config": {"linux": str(tmp_path / "host" / name),
                               "macos": str(tmp_path / "host" / name),
                               "windows": str(tmp_path / "host" / name)}}
        (emu / "symlinks.json").write_text(json.dumps(manifest))
    return tmp_path


# -------- migration --------

def test_migration_dialog_lists_overlapping_entries(qapp, fake_project):
    from gui.dialogs.migration import MigrationDialog
    d = MigrationDialog(str(fake_project))
    items = [d._preview.item(i).text() for i in range(d._preview.count())]
    # Source -> Target both have 'config', so it should appear.
    assert any("config" in t for t in items)


def test_migration_dialog_shows_no_overlap_message(qapp, tmp_path):
    """If two emulators share no entries, preview shows the empty-state line."""
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    a = tmp_path / "AlphaEmu"
    (a / "data").mkdir(parents=True)
    (a / "symlinks.json").write_text(json.dumps({"data": {"linux": "/x"}}))
    b = tmp_path / "BetaEmu"
    (b / "config").mkdir(parents=True)
    (b / "symlinks.json").write_text(json.dumps({"config": {"linux": "/y"}}))

    from gui.dialogs.migration import MigrationDialog
    d = MigrationDialog(str(tmp_path))
    items = [d._preview.item(i).text() for i in range(d._preview.count())]
    assert any("no overlapping" in t for t in items)


# -------- first-run wizard --------

def test_first_run_wizard_emulator_page_lists_known_emulators(qapp, tmp_path):
    from gui.dialogs.first_run import FirstRunWizard
    from gui.manifest import EMULATORS
    w = FirstRunWizard(str(tmp_path))
    chosen = w.selected_emulators()  # all platform-supported ones default to checked
    assert all(name in [m.name for m in EMULATORS] for name in chosen)


def test_has_run_before_false_for_empty_dir(tmp_path):
    from gui.dialogs.first_run import has_run_before
    assert has_run_before(str(tmp_path)) is False


def test_has_run_before_true_when_result_present(tmp_path):
    (tmp_path / "result-dolphin").symlink_to(tmp_path)
    from gui.dialogs.first_run import has_run_before
    assert has_run_before(str(tmp_path)) is True


# -------- logging --------

def test_logger_writes_to_file(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path))
    # Reset the cached logger so monkeypatch takes effect
    from core import logger as logger_mod
    monkeypatch.setattr(logger_mod, "_LOGGER", None)
    log = logger_mod.get_logger()
    log.info("hello-world")
    log_file = os.path.join(str(tmp_path), "schemulator", "schemulator.log")
    for h in log.handlers:
        if hasattr(h, "flush"):
            h.flush()
    assert os.path.exists(log_file)
    content = open(log_file).read()
    assert "hello-world" in content


def test_console_log_emits_to_stdout_and_file(tmp_path, monkeypatch, capsys):
    monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path))
    from core import logger as logger_mod, console
    monkeypatch.setattr(logger_mod, "_LOGGER", None)
    console.console_log("user-facing message")
    out = capsys.readouterr().out
    assert "user-facing message" in out
    log_file = os.path.join(str(tmp_path), "schemulator", "schemulator.log")
    for h in logger_mod.get_logger().handlers:
        if hasattr(h, "flush"):
            h.flush()
    assert "user-facing message" in open(log_file).read()


# -------- atomic backup --------

def test_backup_does_not_leave_partial_zip_on_crash(tmp_path, monkeypatch):
    """If zipping raises mid-way, the .zip is not present (only .zip.tmp may be)."""
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "x").write_text("x")
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": str(tmp_path / "h" / "Emu")}}))

    from core import backup, state
    monkeypatch.setattr(state, "PLATFORM", "linux")

    # Patch ZipFile.write to raise on first call
    import zipfile
    original_write = zipfile.ZipFile.write

    def boom(self, *args, **kw):
        raise RuntimeError("disk full")

    monkeypatch.setattr(zipfile.ZipFile, "write", boom)

    args = argparse.Namespace(config=str(tmp_path / "setup.json"), emulators=[])
    with pytest.raises(RuntimeError):
        backup.cmd_backup(args)

    backup_dir = tmp_path / "backups"
    if backup_dir.exists():
        zips = list(backup_dir.glob("linux-*.zip"))
        assert zips == [], f"a half-baked zip was left behind: {zips}"
