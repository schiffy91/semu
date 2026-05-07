"""About dialog: version, log file path, prereq report."""

from __future__ import annotations

import os
import sys

from PySide6.QtCore import QUrl
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import (
    QDialog,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
)

from core import logger as _logger, prereqs, state


VERSION = "0.1.0-dev"


class AboutDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("About Schemulator")
        self.resize(480, 360)

        layout = QVBoxLayout(self)

        layout.addWidget(QLabel(f"<h2>Schemulator</h2>"))
        layout.addWidget(QLabel(f"Version <b>{VERSION}</b>"))
        layout.addWidget(QLabel(f"Platform: <b>{state.PLATFORM}</b>"))
        layout.addWidget(QLabel(f"Python: <code>{sys.version.split()[0]}</code>"))

        layout.addWidget(QLabel("<b>Prerequisites:</b>"))
        for p in prereqs.check():
            mark = "✓" if p.available else "✗"
            color = "#5cb85c" if p.available else "#f5a623"
            label = QLabel(f"<span style='color: {color}'>{mark} {p.name}</span>")
            layout.addWidget(label)
            if not p.available:
                hint = QLabel(f"<i>{p.install_hint}</i>")
                hint.setWordWrap(True)
                hint.setStyleSheet("color: #aaa; margin-left: 16px;")
                layout.addWidget(hint)

        layout.addWidget(QLabel(f"Log file: <code>{_logger.log_path()}</code>"))

        layout.addStretch()

        row = QHBoxLayout()
        row.addStretch()
        open_log = QPushButton("Open log folder")
        open_log.clicked.connect(self._open_log_folder)
        close = QPushButton("Close")
        close.clicked.connect(self.accept)
        close.setDefault(True)
        row.addWidget(open_log)
        row.addWidget(close)
        layout.addLayout(row)

    def _open_log_folder(self):
        path = os.path.dirname(_logger.log_path())
        if not os.path.isdir(path):
            os.makedirs(path, exist_ok=True)
        QDesktopServices.openUrl(QUrl.fromLocalFile(path))
