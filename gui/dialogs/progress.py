"""Streaming log dialog used while a worker runs."""

from PySide6.QtGui import QTextCursor
from PySide6.QtWidgets import (
    QDialog,
    QHBoxLayout,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QVBoxLayout,
)


class ProgressDialog(QDialog):
    def __init__(self, label: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(label)
        self.setModal(False)
        self.resize(720, 420)

        self._title = QLabel(label)
        self._log = QPlainTextEdit()
        self._log.setReadOnly(True)
        self._log.setStyleSheet("font-family: monospace; font-size: 11px;")

        self._close_btn = QPushButton("Close")
        self._close_btn.setEnabled(False)
        self._close_btn.clicked.connect(self.accept)

        layout = QVBoxLayout(self)
        layout.addWidget(self._title)
        layout.addWidget(self._log)
        row = QHBoxLayout()
        row.addStretch()
        row.addWidget(self._close_btn)
        layout.addLayout(row)

    def append(self, text: str):
        # PlainTextEdit doesn't strip newlines, so just call insertPlainText.
        self._log.moveCursor(QTextCursor.MoveOperation.End)
        self._log.insertPlainText(text)
        self._log.ensureCursorVisible()

    def set_finished(self, ok: bool):
        marker = "Done." if ok else "FAILED — see log above."
        self.append(f"\n--- {marker} ---\n")
        self._close_btn.setEnabled(True)
        self._close_btn.setDefault(True)
