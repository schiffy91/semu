"""Originals browser: list captured snapshots and revert to one."""

from __future__ import annotations

import argparse
import json
import os
from typing import Optional

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QDialog,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
)

from core.backup import cmd_capture, cmd_revert
from core.symlinks import find_emulator_dir


class OriginalsDialog(QDialog):
    def __init__(self, emulator: str, project_dir: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Originals — {emulator}")
        self.resize(560, 420)
        self._emulator = emulator
        self._project_dir = project_dir
        self._config_path = os.path.join(project_dir, "setup.json")

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(f"<b>{emulator}</b> — captured original configs"))
        self._list = QListWidget()
        layout.addWidget(self._list)

        row = QHBoxLayout()
        capture_btn = QPushButton("Capture current as new original…")
        capture_btn.clicked.connect(self._capture_dialog)
        revert_btn = QPushButton("Revert to selected")
        revert_btn.clicked.connect(self._revert_selected)
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        row.addWidget(capture_btn)
        row.addStretch()
        row.addWidget(revert_btn)
        row.addWidget(close_btn)
        layout.addLayout(row)

        self._refresh()

    def _manifest_path(self) -> str:
        emu_dir = find_emulator_dir(self._project_dir, self._emulator) or self._emulator
        return os.path.join(self._project_dir, emu_dir, "originals", "manifest.json")

    def _refresh(self):
        self._list.clear()
        path = self._manifest_path()
        if not os.path.exists(path):
            self._list.addItem("(no originals captured yet)")
            return
        try:
            with open(path) as f:
                manifest = json.load(f)
        except (OSError, json.JSONDecodeError):
            self._list.addItem("(manifest unreadable)")
            return
        if not manifest:
            self._list.addItem("(no originals captured yet)")
            return
        for entry in manifest:
            self._list.addItem(f"{entry['version']}  —  captured {entry['captured']}")

    def _capture_dialog(self):
        from PySide6.QtWidgets import QInputDialog
        version, ok = QInputDialog.getText(self, "Capture original", "Version label:")
        if not ok or not version.strip():
            return
        try:
            cmd_capture(argparse.Namespace(
                config=self._config_path,
                emulator=self._emulator,
                version=version.strip(),
            ))
        except Exception as e:
            QMessageBox.critical(self, "Capture", str(e))
            return
        self._refresh()

    def _revert_selected(self):
        item = self._list.currentItem()
        if not item:
            return
        version = item.text().split("  —")[0].strip()
        if not version or version.startswith("("):
            return
        confirm = QMessageBox.question(
            self, "Revert",
            f"Revert {self._emulator} to '{version}'? Current config will be backed up first."
        )
        if confirm != QMessageBox.Yes:
            return
        try:
            cmd_revert(argparse.Namespace(
                config=self._config_path,
                emulator=self._emulator,
                version=version,
            ))
        except Exception as e:
            QMessageBox.critical(self, "Revert", str(e))
            return
        QMessageBox.information(self, "Revert", f"Reverted to '{version}'.")
