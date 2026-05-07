"""Behaviour-driven GUI tests: simulate clicks and verify the visible result.

We monkeypatch core.lifecycle._nix_build to a fake that produces a result-*
symlink without needing a real Nix install — same fake used in
test_lifecycle. This exercises the full chain: button click → emulator card
emits signal → main window dispatches worker → core function runs →
finished_ok flips status back to "installed".
"""

import argparse
import json
import os
import sys
import time

import pytest


pytest.importorskip("PySide6")
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QEventLoop, QTimer  # noqa: E402
from PySide6.QtWidgets import QApplication, QPushButton  # noqa: E402


@pytest.fixture(scope="session")
def qapp():
    return QApplication.instance() or QApplication(sys.argv)


@pytest.fixture
def project(tmp_path, monkeypatch):
    """A project with one fake emulator and a stubbed nix-build."""
    cfg = {
        "host":     {"linux": str(tmp_path / "host"), "macos": str(tmp_path / "host"), "windows": str(tmp_path / "host")},
        "portable": {"linux": str(tmp_path / "p"),    "macos": str(tmp_path / "p"),    "windows": str(tmp_path / "p")},
    }
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    emu = tmp_path / "Dolphin"  # match a name in gui.manifest
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "Dolphin.ini").write_text("default=true\n")
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {
            "linux":   str(tmp_path / "host" / "Dolphin"),
            "macos":   str(tmp_path / "host" / "Dolphin"),
            "windows": str(tmp_path / "host" / "Dolphin"),
        }
    }))

    # Fake nix store + version file
    fake_store = tmp_path / "nix-store-fake"
    (fake_store / "bin").mkdir(parents=True)
    (fake_store / "bin" / "Dolphin").write_text("#!/bin/sh\n")

    from core import lifecycle, updater

    def fake_build(emulator, project_dir):
        out_link = lifecycle._result_dir(project_dir, emulator)
        if os.path.lexists(out_link):
            os.unlink(out_link)
        os.symlink(str(fake_store), out_link)
        return True

    monkeypatch.setattr(lifecycle, "_nix_build", fake_build)
    monkeypatch.setattr(lifecycle, "_nix_available", lambda: True)
    # Force the GUI to read this project dir (vs the repo root)
    monkeypatch.setenv("SCHEMULATOR_PROJECT_DIR", str(tmp_path))
    return tmp_path


def _spin(qapp, predicate, timeout_ms=10000):
    """Run the Qt event loop until `predicate()` is true, or timeout."""
    loop = QEventLoop()
    timer = QTimer()
    timer.setInterval(50)
    timer.timeout.connect(lambda: predicate() and loop.quit())
    timer.start()
    QTimer.singleShot(timeout_ms, loop.quit)
    loop.exec()
    timer.stop()


def test_install_button_triggers_worker_and_creates_symlinks(qapp, project, monkeypatch):
    from gui.main_window import MainWindow

    # Don't pop the QMessageBox.question dialog during the test
    from PySide6.QtWidgets import QMessageBox
    monkeypatch.setattr(QMessageBox, "information", lambda *a, **kw: None)

    w = MainWindow()
    card = w._cards["Dolphin"]
    # Sanity: card visible, button enabled
    assert card.action_button.isEnabled()

    # Trigger install via the signal directly — simulates a real button click
    card.install_clicked.emit("Dolphin")

    # Wait for the worker to finish
    _spin(qapp, lambda: w._current_worker and w._current_worker.isFinished())

    # Result symlink + host link should exist
    assert os.path.islink(project / "result-dolphin")
    assert (project / "host" / "Dolphin" / "Dolphin.ini").exists()


def test_backup_button_creates_zip(qapp, project, monkeypatch):
    from gui.main_window import MainWindow
    from PySide6.QtWidgets import QMessageBox
    monkeypatch.setattr(QMessageBox, "information", lambda *a, **kw: None)

    w = MainWindow()
    w._on_backup("Dolphin")
    _spin(qapp, lambda: w._current_worker and w._current_worker.isFinished())

    backups = list((project / "backups").glob("*.zip"))
    assert len(backups) == 1


def test_uninstall_removes_symlinks_keeps_data(qapp, project, monkeypatch):
    from gui.main_window import MainWindow
    from PySide6.QtWidgets import QMessageBox
    # Auto-confirm the "are you sure?" prompt
    monkeypatch.setattr(QMessageBox, "question", lambda *a, **kw: QMessageBox.Yes)
    monkeypatch.setattr(QMessageBox, "information", lambda *a, **kw: None)

    w = MainWindow()
    w._on_install("Dolphin")
    _spin(qapp, lambda: w._current_worker and w._current_worker.isFinished())
    assert (project / "host" / "Dolphin" / "Dolphin.ini").exists()

    w._on_uninstall("Dolphin")
    _spin(qapp, lambda: w._current_worker and w._current_worker.isFinished())

    # Project-dir data preserved
    assert (project / "Dolphin" / "config" / "Dolphin.ini").exists()
    # Host symlinks gone
    assert not (project / "host" / "Dolphin").exists() or not list((project / "host" / "Dolphin").iterdir())


def test_about_dialog_lists_prereqs(qapp):
    from gui.dialogs.about import AboutDialog
    d = AboutDialog()
    text = "".join(c.text() for c in d.findChildren(__import__('PySide6.QtWidgets', fromlist=['QLabel']).QLabel))
    assert "nix" in text.lower()
    assert "syncthing" in text.lower()


def test_main_window_about_button_present(qapp):
    from gui.main_window import MainWindow
    w = MainWindow()
    btns = [b.text() for b in w.findChildren(QPushButton)]
    assert "About" in btns
    assert "Migrate…" in btns
    assert "Settings…" in btns
