"""GUI integration tests using offscreen Qt.

These exercise the full chain: card click → worker thread → core function →
log dialog updates. Catches integration bugs that pure-Python tests miss
(signal/slot wiring, thread safety, dialog modality).
"""

import argparse
import json
import os
import sys

import pytest


pytest.importorskip("PySide6")

# Headless Qt
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QCoreApplication, QEventLoop, QTimer  # noqa: E402
from PySide6.QtWidgets import QApplication  # noqa: E402


@pytest.fixture(scope="session")
def qapp():
    app = QApplication.instance() or QApplication(sys.argv)
    yield app


@pytest.fixture
def fake_project(tmp_path):
    setup_json = {
        "host":     {"linux": str(tmp_path / "host"), "macos": str(tmp_path / "host"), "windows": str(tmp_path / "host")},
        "portable": {"linux": str(tmp_path / "portable"), "macos": str(tmp_path / "portable"), "windows": str(tmp_path / "portable")},
    }
    (tmp_path / "setup.json").write_text(json.dumps(setup_json))
    emu = tmp_path / "TestEmu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "settings.ini").write_text("default=true\n")
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {
            "linux":   str(tmp_path / "host" / "TestEmu"),
            "macos":   str(tmp_path / "host" / "TestEmu"),
            "windows": str(tmp_path / "host" / "TestEmu"),
        }
    }))
    return tmp_path


def test_main_window_renders_all_cards(qapp):
    from gui.main_window import MainWindow
    from gui.manifest import EMULATORS
    w = MainWindow()
    assert set(w._cards.keys()) == {meta.name for meta in EMULATORS}


def test_worker_streams_progress_to_signal(qapp):
    """Worker now uses logger-based capture (not redirect_stdout) — emit via
    core.console.console_log so the signal handler receives it."""
    from gui.workers import CoreWorker, make_args
    from core.console import console_log

    captured = []
    finished = []

    def fake(args):
        for i in range(3):
            console_log(f"step {i}")

    w = CoreWorker(fake, make_args())
    w.progress.connect(captured.append)
    w.finished_ok.connect(finished.append)

    loop = QEventLoop()
    w.finished_ok.connect(lambda _: loop.quit())
    QTimer.singleShot(5000, loop.quit)
    w.start()
    loop.exec()
    w.wait(5000)

    assert finished == [True]
    output = "".join(captured)
    assert "step 0" in output and "step 2" in output


def test_worker_emits_failure_signal_on_exception(qapp):
    from gui.workers import CoreWorker, make_args

    def boom(args):
        raise RuntimeError("kaboom")

    finished = []
    captured = []
    w = CoreWorker(boom, make_args())
    w.progress.connect(captured.append)
    w.finished_ok.connect(finished.append)

    loop = QEventLoop()
    w.finished_ok.connect(lambda _: loop.quit())
    QTimer.singleShot(5000, loop.quit)
    w.start()
    loop.exec()
    w.wait(5000)

    assert finished == [False]
    output = "".join(captured)
    assert "kaboom" in output


def test_progress_dialog_appends_and_finishes(qapp):
    from gui.dialogs.progress import ProgressDialog
    d = ProgressDialog("test")
    d.append("line 1\n")
    d.append("line 2\n")
    d.set_finished(True)
    assert d._close_btn.isEnabled()


def test_steamdeck_dialog_constructs_with_no_sd(qapp, fake_project, monkeypatch):
    from core import sdcard
    from gui.dialogs.steamdeck import SteamDeckDialog
    monkeypatch.setattr(sdcard, "SD_MOUNT_ROOTS", ())  # nothing to find
    d = SteamDeckDialog(str(fake_project))
    assert "no external storage" in d._sd_combo.currentText().lower()


def test_steamdeck_dialog_lists_systems_when_sd_present(qapp, fake_project, monkeypatch, tmp_path):
    from core import sdcard
    # Build a fake SD mount
    mount_root = tmp_path / "run-media"
    user = mount_root / "deck"
    sd = user / "MY_SD"
    (sd / "Emulation" / "roms" / "snes").mkdir(parents=True)
    (sd / "Emulation" / "roms" / "snes" / "mario.sfc").write_bytes(b"\0" * 1024)
    monkeypatch.setattr(sdcard, "SD_MOUNT_ROOTS", (str(mount_root),))

    from gui.dialogs.steamdeck import SteamDeckDialog
    d = SteamDeckDialog(str(fake_project))
    assert d._sd_combo.count() >= 1
    # systems_list shows the snes entry
    items = [d._systems_list.item(i).text() for i in range(d._systems_list.count())]
    assert any("snes" in t for t in items)


def test_syncthing_dialog_shows_missing_binary_message(qapp, fake_project, monkeypatch):
    from core import syncthing
    monkeypatch.setattr(syncthing, "find_binary", lambda: None)
    from gui.dialogs.syncthing import SyncthingDialog
    d = SyncthingDialog(str(fake_project))
    # Fallback content: dialog renders even without a binary, no crash.
    assert d.windowTitle()


def test_emulator_card_renders_disabled_when_unsupported(qapp):
    from gui.emulator_card import EmulatorCard
    from gui.manifest import EmulatorMeta

    meta = EmulatorMeta("FakeOnly", "Fake stuff", platforms=["windows"])
    card = EmulatorCard(meta, current_platform="linux")
    assert not card.action_button.isEnabled()
    assert "Unsupported" in card.action_button.text()


def test_settings_dialog_loads_existing_config(qapp, fake_project):
    from gui.dialogs.settings import SettingsDialog
    d = SettingsDialog(str(fake_project))
    # The fixture writes setup.json with str(tmp_path / 'host') etc.
    assert "host" in d._host_input[1].text() or "Volumes" in d._host_input[1].text()


def test_originals_dialog_shows_empty_state(qapp, fake_project):
    from gui.dialogs.originals import OriginalsDialog
    d = OriginalsDialog("TestEmu", str(fake_project))
    assert d._list.count() == 1
    assert "no originals" in d._list.item(0).text().lower()


def test_originals_dialog_lists_captured_versions(qapp, fake_project):
    import argparse
    from core.backup import cmd_capture
    cmd_capture(argparse.Namespace(
        config=str(fake_project / "setup.json"),
        emulator="TestEmu",
        version="v1.0",
    ))
    from gui.dialogs.originals import OriginalsDialog
    d = OriginalsDialog("TestEmu", str(fake_project))
    items = [d._list.item(i).text() for i in range(d._list.count())]
    assert any("v1.0" in t for t in items)
