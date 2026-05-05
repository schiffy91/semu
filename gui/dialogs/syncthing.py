"""Syncthing pairing + sidecar control dialog."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QDialog,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
)

from core import syncthing


class SyncthingDialog(QDialog):
    def __init__(self, project_dir: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Sync saves with other devices")
        self.resize(560, 440)
        self._project_dir = project_dir

        layout = QVBoxLayout(self)

        binary = syncthing.find_binary()
        if not binary:
            layout.addWidget(QLabel(
                "<b>Syncthing not found.</b><br>"
                "Drop a syncthing binary at <code>bin/syncthing</code> in the repo, "
                "or set SCHEMULATOR_SYNCTHING to its path."
            ))
            return

        self._status_label = QLabel("(checking status…)")
        layout.addWidget(self._status_label)

        layout.addWidget(QLabel("<b>This device's ID</b>"))
        self._device_id_box = QLineEdit()
        self._device_id_box.setReadOnly(True)
        layout.addWidget(self._device_id_box)

        layout.addWidget(QLabel("<b>Pair with another device</b><br>Paste the peer's device ID:"))
        peer_row = QHBoxLayout()
        self._peer_input = QLineEdit()
        self._peer_input.setPlaceholderText("XXXXXXX-XXXXXXX-XXXXXXX-XXXXXXX-XXXXXXX-XXXXXXX-XXXXXXX")
        pair_btn = QPushButton("Authorize peer")
        pair_btn.clicked.connect(self._pair_peer)
        peer_row.addWidget(self._peer_input)
        peer_row.addWidget(pair_btn)
        layout.addLayout(peer_row)

        action_row = QHBoxLayout()
        share_btn = QPushButton("Share saves folder")
        share_btn.clicked.connect(self._share_folder)
        start_btn = QPushButton("Start sidecar")
        start_btn.clicked.connect(self._start)
        action_row.addWidget(share_btn)
        action_row.addWidget(start_btn)
        action_row.addStretch()
        layout.addLayout(action_row)

        layout.addStretch()
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        layout.addWidget(close_btn, alignment=Qt.AlignmentFlag.AlignRight)

        self._refresh()

    def _refresh(self):
        status = syncthing.status()
        self._device_id_box.setText(status.device_id or "(not initialised — start the sidecar)")
        self._status_label.setText(
            "Sidecar running" if status.running else "Sidecar stopped"
        )
        self._status_label.setStyleSheet(
            "color: #5cb85c;" if status.running else "color: #f5a623;"
        )

    def _start(self):
        proc = syncthing.start()
        if proc is None:
            QMessageBox.warning(self, "Syncthing", "Couldn't launch Syncthing.")
            return
        QMessageBox.information(self, "Syncthing", "Sidecar started in the background.")
        self._refresh()

    def _pair_peer(self):
        peer = self._peer_input.text().strip()
        if not peer:
            return
        if not syncthing.add_device(peer):
            QMessageBox.warning(
                self, "Syncthing",
                "Pairing failed. Is the sidecar running and the device ID correct?",
            )
            return
        QMessageBox.information(self, "Syncthing", "Peer authorized.")
        self._peer_input.clear()

    def _share_folder(self):
        if not syncthing.add_folder(self._project_dir):
            QMessageBox.warning(
                self, "Syncthing",
                "Couldn't add saves folder. Is the sidecar running?",
            )
            return
        QMessageBox.information(self, "Syncthing", "Saves folder shared.")
