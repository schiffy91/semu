"""A single emulator card in the main grid."""

from __future__ import annotations

import os
from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QMenu,
    QPushButton,
    QVBoxLayout,
)

from gui.manifest import EmulatorMeta


class EmulatorCard(QFrame):
    install_clicked = Signal(str)
    update_clicked = Signal(str)
    uninstall_clicked = Signal(str)
    backup_clicked = Signal(str)
    rollback_clicked = Signal(str)
    revert_clicked = Signal(str)
    open_config_clicked = Signal(str)
    apply_controller_clicked = Signal(str)

    def __init__(self, meta: EmulatorMeta, current_platform: str, parent=None):
        super().__init__(parent)
        self.meta = meta
        self.current_platform = current_platform
        self.setFrameShape(QFrame.StyledPanel)
        self.setObjectName("EmulatorCard")
        self.setMinimumHeight(80)

        self.installed_version_label = QLabel("not installed")
        self.installed_version_label.setStyleSheet("color: #888;")
        self.status_label = QLabel("")

        title = QLabel(f"<b>{meta.name}</b>")
        systems = QLabel(meta.systems)
        systems.setStyleSheet("color: #aaa;")

        left = QVBoxLayout()
        left.addWidget(title)
        left.addWidget(systems)

        middle = QVBoxLayout()
        middle.addWidget(self.installed_version_label)
        middle.addWidget(self.status_label)

        self.action_button = QPushButton("Install")
        self.action_button.clicked.connect(self._on_action_clicked)

        self.menu_button = QPushButton("…")
        self.menu_button.setFixedWidth(32)
        self.menu_button.clicked.connect(self._show_menu)

        right = QHBoxLayout()
        right.addWidget(self.action_button)
        right.addWidget(self.menu_button)

        layout = QHBoxLayout(self)
        layout.addLayout(left, 3)
        layout.addLayout(middle, 2)
        layout.addLayout(right, 1)
        self.setLayout(layout)

        self._update_state(installed=False, available=False)
        if current_platform not in meta.platforms:
            self.action_button.setEnabled(False)
            self.action_button.setText("Unsupported")
            self.status_label.setText(f"Not available on {current_platform}")

    def set_installed(self, version: Optional[str], update_available_to: Optional[str]):
        if version is None:
            self.installed_version_label.setText("not installed")
            self.installed_version_label.setStyleSheet("color: #888;")
            self.action_button.setText("Install")
            self.status_label.setText("")
        elif update_available_to and update_available_to != version:
            self.installed_version_label.setText(f"v{version}")
            self.installed_version_label.setStyleSheet("color: #ddd;")
            self.action_button.setText("Update")
            self.status_label.setText(f"-> v{update_available_to}")
            self.status_label.setStyleSheet("color: #f5a623;")
        else:
            self.installed_version_label.setText(f"v{version}")
            self.installed_version_label.setStyleSheet("color: #ddd;")
            self.action_button.setText("Reinstall")
            self.status_label.setText("up-to-date")
            self.status_label.setStyleSheet("color: #5cb85c;")
        self._update_state(installed=version is not None, available=bool(update_available_to))

    def _update_state(self, installed: bool, available: bool):
        self.action_button.setEnabled(self.current_platform in self.meta.platforms)

    def _on_action_clicked(self):
        text = self.action_button.text()
        if text == "Install" or text == "Reinstall":
            self.install_clicked.emit(self.meta.name)
        elif text == "Update":
            self.update_clicked.emit(self.meta.name)

    def _show_menu(self):
        menu = QMenu(self)
        menu.addAction("Install", lambda: self.install_clicked.emit(self.meta.name))
        menu.addAction("Update", lambda: self.update_clicked.emit(self.meta.name))
        menu.addAction("Backup", lambda: self.backup_clicked.emit(self.meta.name))
        menu.addAction("Rollback to previous build", lambda: self.rollback_clicked.emit(self.meta.name))
        menu.addAction("Revert to original", lambda: self.revert_clicked.emit(self.meta.name))
        menu.addSeparator()
        menu.addAction("Apply controller defaults...", lambda: self.apply_controller_clicked.emit(self.meta.name))
        menu.addSeparator()
        menu.addAction("Open config folder", lambda: self.open_config_clicked.emit(self.meta.name))
        menu.addAction("Uninstall", lambda: self.uninstall_clicked.emit(self.meta.name))
        menu.exec(self.menu_button.mapToGlobal(self.menu_button.rect().bottomLeft()))
