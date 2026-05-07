"""Settings dialog: edits setup.json's host/portable paths and a few prefs.

Keeps the JSON shape the CLI expects so the GUI and CLI stay symmetric.
"""

from __future__ import annotations

import json
import os
from typing import Optional

from PySide6.QtWidgets import (
    QDialog,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
)

from core import state


class SettingsDialog(QDialog):
    def __init__(self, project_dir: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Settings")
        self.resize(520, 240)
        self._project_dir = project_dir
        self._config_path = os.path.join(project_dir, "setup.json")

        config = self._load()
        host = config.get("host", {}).get(state.PLATFORM, "")
        portable = config.get("portable", {}).get(state.PLATFORM, "")

        layout = QVBoxLayout(self)

        form = QFormLayout()
        self._host_input = self._path_input(host)
        self._portable_input = self._path_input(portable)
        form.addRow("Host config dir:", self._host_input[0])
        form.addRow("Portable (ES-DE) dir:", self._portable_input[0])
        layout.addLayout(form)

        layout.addWidget(QLabel(
            "<i>Host = where the OS expects each emulator's config "
            "(/Library/Application Support, ~/.config, etc.).<br>"
            "Portable = where ES-DE stores ROMs / media / themes.</i>"
        ))
        layout.addStretch()

        row = QHBoxLayout()
        row.addStretch()
        cancel = QPushButton("Cancel")
        cancel.clicked.connect(self.reject)
        save = QPushButton("Save")
        save.setDefault(True)
        save.clicked.connect(self._save)
        row.addWidget(cancel)
        row.addWidget(save)
        layout.addLayout(row)

    def _path_input(self, initial: str):
        wrap = QHBoxLayout()
        edit = QLineEdit(initial)
        browse = QPushButton("Browse…")
        browse.clicked.connect(lambda: self._browse(edit))
        container = QLabel()
        # We just return the edit + browse pair as a horizontal layout in a frame
        from PySide6.QtWidgets import QWidget
        widget = QWidget()
        h = QHBoxLayout(widget)
        h.setContentsMargins(0, 0, 0, 0)
        h.addWidget(edit, 1)
        h.addWidget(browse)
        return widget, edit

    def _browse(self, edit: QLineEdit):
        path = QFileDialog.getExistingDirectory(self, "Choose directory", edit.text() or os.path.expanduser("~"))
        if path:
            edit.setText(path)

    def _load(self) -> dict:
        if not os.path.exists(self._config_path):
            return {"host": {}, "portable": {}}
        try:
            with open(self._config_path) as f:
                return json.load(f)
        except (OSError, json.JSONDecodeError):
            return {"host": {}, "portable": {}}

    def _save(self):
        config = self._load()
        host_value = self._host_input[1].text().strip()
        portable_value = self._portable_input[1].text().strip()

        if not host_value or not portable_value:
            QMessageBox.warning(self, "Settings", "Both paths must be set.")
            return

        config.setdefault("host", {})[state.PLATFORM] = host_value
        config.setdefault("portable", {})[state.PLATFORM] = portable_value

        try:
            with open(self._config_path, "w") as f:
                json.dump(config, f, indent=4)
        except OSError as e:
            QMessageBox.critical(self, "Settings", f"Couldn't write {self._config_path}:\n{e}")
            return
        self.accept()
