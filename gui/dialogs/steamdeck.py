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


def _shortcut_for_discovered(emu) -> steam.Shortcut:
    """Build a Steam shortcut for a discovered emulator. macOS .app bundles
    can't be Exe'd directly — Steam needs an executable, not a directory.
    Wrap with `open -a` so Steam launches the bundle correctly (round-9 H2).
    """
    if emu.kind == "app":
        return steam.Shortcut(
            appname=f"{emu.name.capitalize()} (Schemulator)",
            exe="/usr/bin/open",
            start_dir=os.path.dirname(emu.exe),
            launch_options=f'-a "{emu.exe}" --new --wait-apps',
            tags=["Schemulator", "Emulation"],
        )
    return steam.Shortcut(
        appname=f"{emu.name.capitalize()} (Schemulator)",
        exe=emu.exe,
        start_dir=os.path.dirname(emu.exe),
        tags=["Schemulator", "Emulation"],
    )


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

        # Steam shortcut: detect every installed emulator and offer each.
        steam_group = QGroupBox("Steam integration")
        steam_layout = QVBoxLayout(steam_group)
        self._add_shortcut = QCheckBox("Add ES-DE as a non-Steam game")
        self._add_shortcut.setChecked(True)
        # Critic finding #15: this only installs the layout as a *template* —
        # users still need to pick it from Steam's controller picker. Fully
        # auto-applying would require touching userdata/<id>/config/localconfig.vdf
        # which is high-risk (Steam rewrites it on shutdown). Be honest in the UI.
        self._install_layout = QCheckBox("Install ES-DE controller layout template (visible in Steam's picker)")
        self._install_layout.setChecked(True)
        steam_layout.addWidget(self._add_shortcut)
        steam_layout.addWidget(self._install_layout)

        self._emulator_checks = {}
        self._discovered = steam.discover_installed_emulators(self._project_dir)
        if self._discovered:
            steam_layout.addWidget(QLabel("Also add as separate non-Steam games:"))
            for emu in self._discovered:
                cb = QCheckBox(emu.name)
                cb.setChecked(False)
                self._emulator_checks[emu.name] = cb
                steam_layout.addWidget(cb)
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
                exe = self._es_de_exe()
                if exe.endswith(".app"):
                    # macOS: wrap with `open -a` so Steam can launch the bundle.
                    shortcut = steam.Shortcut(
                        appname="ES-DE (Schemulator)",
                        exe="/usr/bin/open",
                        start_dir=os.path.dirname(exe),
                        launch_options=f'-a "{exe}" --new --wait-apps',
                        tags=["Schemulator", "Emulation"],
                    )
                else:
                    shortcut = steam.Shortcut(
                        appname="ES-DE (Schemulator)",
                        exe=exe,
                        start_dir=os.path.dirname(exe) if os.path.isabs(exe) else "",
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
            # Actually wire ES-DE's ROMDirectory to point at the chosen card's
            # Emulation/roms/ root. Without this, the dialog reported "selected"
            # but the user had no working ROM library in Game Mode (round-6 #2).
            settings_written = sdcard.wire_es_de_to_card(chosen_card, self._project_dir)
            if settings_written:
                notes.append(
                    f"Wired ES-DE ROM root to {chosen_card.mount_path}/Emulation/roms/.\n"
                    f"({settings_written})"
                )
            else:
                notes.append(
                    f"Detected SD root at {chosen_card.mount_path} but couldn't "
                    f"write ~/ES-DE/settings/es_settings.xml. ROM library not wired."
                )

        # Per-emulator Steam shortcuts
        if self._emulator_checks:
            shortcuts_path = steam.shortcuts_path()
            for emu in self._discovered:
                cb = self._emulator_checks.get(emu.name)
                if cb and cb.isChecked() and shortcuts_path:
                    sc = _shortcut_for_discovered(emu)
                    steam.upsert_shortcut(shortcuts_path, sc)
                    notes.append(f"Added {emu.name} as a non-Steam game.")

        QMessageBox.information(self, "Steam Deck setup", "\n\n".join(notes) or "Nothing to apply.")
        self.accept()

    def _es_de_exe(self) -> str:
        """Locate the ES-DE binary. Prefers the schemulator-installed copy
        over a system one so the shortcut points at the version we manage.
        """
        # 1) Schemulator-installed via result-es-de
        for emu in steam.discover_installed_emulators(self._project_dir):
            if emu.name.lower() in ("es-de", "esde"):
                return emu.exe
        # 2) Anywhere on PATH or known installer locations.
        import shutil
        which = shutil.which("es-de")
        if which:
            return which
        for candidate in (
            os.path.expanduser("~/Applications/ES-DE.AppImage"),
            "/var/lib/flatpak/exports/bin/org.es_de.frontend",
            os.path.expanduser("~/.local/bin/es-de"),
        ):
            if os.path.exists(candidate):
                return candidate
        return "es-de"  # let Steam complain that the path doesn't exist

    @staticmethod
    def _steam_layout_dest() -> str:
        # Honour Flatpak / macOS Steam locations via find_steam_root. Round-9 H1.
        root = steam.find_steam_root() or os.path.expanduser("~/.steam/steam")
        return os.path.join(root, "controller_base", "templates", "schemulator_es_de.vdf")
