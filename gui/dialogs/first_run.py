"""First-run wizard: pick a project dir, optionally scan an SD card, and
optionally install the bundled emulators that have a build for this platform."""

from __future__ import annotations

import argparse
import json
import os
from typing import List

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QCheckBox,
    QFileDialog,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
    QWidget,
    QWizard,
    QWizardPage,
)

from core import sdcard, state
from gui.manifest import EMULATORS


def has_run_before(project_dir: str) -> bool:
    """Heuristic: any installed result-* symlink or non-default setup.json
    means first-run is done."""
    if not os.path.isdir(project_dir):
        return False
    if any(name.startswith("result-") for name in os.listdir(project_dir)):
        return True
    setup = os.path.join(project_dir, "setup.json")
    if os.path.exists(setup):
        try:
            with open(setup) as f:
                cfg = json.load(f)
        except (OSError, json.JSONDecodeError):
            return False
        host = cfg.get("host", {}).get(state.PLATFORM, "")
        if host and not host.endswith(("AppData/Roaming/", ".config/", "Application Support/")):
            return True
    return False


class _ProjectPage(QWizardPage):
    def __init__(self, default_dir: str, parent=None):
        super().__init__(parent)
        self.setTitle("Project directory")
        self.setSubTitle(
            "Pick the folder where Schemulator stores per-emulator configs. "
            "Place this on a cloud-synced folder to keep saves in sync."
        )
        layout = QVBoxLayout(self)

        from PySide6.QtWidgets import QLineEdit
        self._edit = QLineEdit(default_dir)
        browse = QPushButton("Browse…")
        browse.clicked.connect(self._browse)
        row = QHBoxLayout()
        row.addWidget(self._edit, 1)
        row.addWidget(browse)
        layout.addLayout(row)
        layout.addStretch()
        self.registerField("project_dir*", self._edit)

    def _browse(self):
        path = QFileDialog.getExistingDirectory(
            self, "Choose project directory", self._edit.text(),
        )
        if path:
            self._edit.setText(path)


class _SdCardPage(QWizardPage):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTitle("SD card")
        self.setSubTitle(
            "Optional: pick an SD card with ROMs. Schemulator can wire each "
            "emulator to read from the card automatically. (Steam Deck only.)"
        )
        layout = QVBoxLayout(self)
        self._cards = sdcard.list_sdcards()
        if not self._cards:
            layout.addWidget(QLabel("No external storage detected. You can skip this step."))
        else:
            from PySide6.QtWidgets import QComboBox
            self._combo = QComboBox()
            for c in self._cards:
                tag = " (EmuDeck layout)" if c.has_emudeck_layout else ""
                self._combo.addItem(f"{c.label} — {c.mount_path}{tag}")
            layout.addWidget(self._combo)
            self._summary = QLabel("")
            self._summary.setWordWrap(True)
            self._combo.currentIndexChanged.connect(self._update)
            layout.addWidget(self._summary)
            self._update(0)
        layout.addStretch()

    def _update(self, idx: int):
        if 0 <= idx < len(self._cards):
            c = self._cards[idx]
            n = sum(len(v) for v in c.rom_systems.values())
            self._summary.setText(
                f"Detected {n} ROMs across {len(c.rom_systems)} systems on {c.mount_path}."
            )


class _EmulatorsPage(QWizardPage):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTitle("Emulators")
        self.setSubTitle(
            "Pick which emulators to install now. You can change this later."
        )
        layout = QVBoxLayout(self)
        self._list = QListWidget()
        for meta in EMULATORS:
            available = state.PLATFORM in meta.platforms
            item = QListWidgetItem(f"{meta.name} — {meta.systems}")
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Checked if available else Qt.CheckState.Unchecked)
            if not available:
                item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsEnabled)
                item.setText(item.text() + f"  (not available on {state.PLATFORM})")
            self._list.addItem(item)
        layout.addWidget(self._list)

    def selected(self) -> List[str]:
        out = []
        for i in range(self._list.count()):
            item = self._list.item(i)
            if item.checkState() == Qt.CheckState.Checked:
                out.append(item.text().split(" — ")[0])
        return out


class FirstRunWizard(QWizard):
    """Top-level wizard. After accept(), .selected_emulators is the chosen list."""

    def __init__(self, default_project_dir: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Schemulator — first run")
        self.resize(640, 460)
        self.addPage(_ProjectPage(default_project_dir))
        self.addPage(_SdCardPage())
        self._emu_page = _EmulatorsPage()
        self.addPage(self._emu_page)

    def selected_emulators(self) -> List[str]:
        return self._emu_page.selected()

    def project_dir(self) -> str:
        return self.field("project_dir") or ""
