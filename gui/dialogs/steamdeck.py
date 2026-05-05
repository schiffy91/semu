"""Steam Deck SD-card setup dialog: scan, register ES-DE shortcut, apply
controller layout, install Flatpak overrides."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
)

from core import sdcard, steam


class SteamDeckDialog(QDialog):
    def __init__(self, project_dir: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Steam Deck setup")
        self.resize(680, 580)
        self._project_dir = project_dir
        self._cards: list[sdcard.SdCard] = []

        layout = QVBoxLayout(self)

        # SD card discovery
        sd_group = QGroupBox("SD card / external storage")
        sd_layout = QVBoxLayout(sd_group)
        self._sd_combo = QComboBox()
        rescan = QPushButton("Rescan")
        rescan.clicked.connect(self._scan_sds)
        row = QHBoxLayout()
        row.addWidget(self._sd_combo, stretch=1)
        row.addWidget(rescan)
        sd_layout.addLayout(row)

        self._systems_list = QListWidget()
        sd_layout.addWidget(QLabel("Detected ROM systems:"))
        sd_layout.addWidget(self._systems_list)
        layout.addWidget(sd_group)

        # Firmware status
        fw_group = QGroupBox("BIOS / firmware status")
        fw_layout = QVBoxLayout(fw_group)
        self._firmware_label = QLabel("(scanning…)")
        self._firmware_label.setWordWrap(True)
        fw_layout.addWidget(self._firmware_label)
        layout.addWidget(fw_group)

        # Steam shortcut
        steam_group = QGroupBox("Steam integration")
        steam_layout = QVBoxLayout(steam_group)
        self._add_shortcut = QCheckBox("Add ES-DE as a non-Steam game")
        self._add_shortcut.setChecked(True)
        self._install_layout = QCheckBox("Install bundled controller layout for ES-DE")
        self._install_layout.setChecked(True)
        steam_layout.addWidget(self._add_shortcut)
        steam_layout.addWidget(self._install_layout)
        layout.addWidget(steam_group)

        # Buttons
        btn_row = QHBoxLayout()
        btn_row.addStretch()
        cancel = QPushButton("Cancel")
        cancel.clicked.connect(self.reject)
        apply_btn = QPushButton("Apply")
        apply_btn.setDefault(True)
        apply_btn.clicked.connect(self._apply)
        btn_row.addWidget(cancel)
        btn_row.addWidget(apply_btn)
        layout.addLayout(btn_row)

        self._scan_sds()
        self._refresh_firmware()

    def _scan_sds(self):
        self._sd_combo.clear()
        self._systems_list.clear()
        self._cards = sdcard.list_sdcards()
        if not self._cards:
            self._sd_combo.addItem("(no external storage detected)")
            self._sd_combo.setEnabled(False)
            return
        self._sd_combo.setEnabled(True)
        for c in self._cards:
            tag = " (EmuDeck layout)" if c.has_emudeck_layout else ""
            self._sd_combo.addItem(f"{c.label} — {c.mount_path}{tag}")
        self._sd_combo.currentIndexChanged.connect(self._show_systems)
        self._show_systems(self._sd_combo.currentIndex())

    def _show_systems(self, idx: int):
        self._systems_list.clear()
        if idx < 0 or idx >= len(self._cards):
            return
        c = self._cards[idx]
        for system, files in sorted(c.rom_systems.items()):
            self._systems_list.addItem(f"{system}: {len(files)} ROMs")

    def _refresh_firmware(self):
        missing = sdcard.check_firmware(self._project_dir)
        if not missing:
            self._firmware_label.setText("All required BIOS / keys present.")
            self._firmware_label.setStyleSheet("color: #5cb85c;")
            return
        lines = [f"<b>{emulator}:</b> missing {', '.join(files)}" for emulator, files in missing.items()]
        self._firmware_label.setText("<br>".join(lines))
        self._firmware_label.setStyleSheet("color: #f5a623;")

    def _apply(self):
        chosen_idx = self._sd_combo.currentIndex()
        chosen_card = self._cards[chosen_idx] if 0 <= chosen_idx < len(self._cards) else None

        notes = []
        if self._add_shortcut.isChecked():
            shortcuts_path = steam.shortcuts_path()
            if not shortcuts_path:
                notes.append("No Steam install found; shortcut skipped.")
            else:
                shortcut = steam.Shortcut(
                    appname="ES-DE (Schemulator)",
                    exe=self._es_de_exe(),
                    start_dir=os.path.dirname(self._es_de_exe()),
                    launch_options="",
                    tags=["Schemulator", "Emulation"],
                )
                steam.upsert_shortcut(shortcuts_path, shortcut)
                notes.append(f"Wrote shortcut to {shortcuts_path}.")

        if chosen_card and self._install_layout.isChecked():
            layout_dst = self._steam_layout_dest()
            layout_src = os.path.join(
                os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                "controllers", "steamdeck", "steam_input_template.vdf",
            )
            if os.path.isfile(layout_src) and layout_dst:
                os.makedirs(os.path.dirname(layout_dst), exist_ok=True)
                with open(layout_src, "rb") as r, open(layout_dst, "wb") as w:
                    w.write(r.read())
                notes.append(f"Installed Steam Input layout to {layout_dst}.")

        if chosen_card:
            notes.append(
                f"Selected SD root: {chosen_card.mount_path}. "
                "Run 'Install All' from the main window to wire emulators to this ROM root."
            )

        QMessageBox.information(self, "Steam Deck setup", "\n\n".join(notes) or "Nothing to apply.")
        self.accept()

    @staticmethod
    def _es_de_exe() -> str:
        # Best-effort guess; user can edit via Steam UI afterward.
        for candidate in (
            "/usr/bin/es-de",
            os.path.expanduser("~/Applications/ES-DE.AppImage"),
            "/var/lib/flatpak/exports/bin/org.es_de.frontend",
        ):
            if os.path.exists(candidate):
                return candidate
        return "/usr/bin/es-de"

    @staticmethod
    def _steam_layout_dest() -> str:
        steam_root = os.path.expanduser("~/.steam/steam")
        return os.path.join(steam_root, "controller_base", "templates", "schemulator_es_de.vdf")
