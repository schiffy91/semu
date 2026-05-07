"""Migration dialog: copy configs from one emulator to another.

Used for things like "I'm switching from Lime3DS to Azahar" or "let me move
my configs from one fork's directory layout to its successor."
"""

from __future__ import annotations

import json
import os
from typing import List

from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
)

from core.backup import cmd_migrate
from gui.workers import CoreWorker, make_args


class MigrationDialog(QDialog):
    def __init__(self, project_dir: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Migrate configs between emulators")
        self.resize(640, 460)
        self._project_dir = project_dir

        layout = QVBoxLayout(self)

        layout.addWidget(QLabel(
            "<i>Copies overlapping config entries from one emulator to another. "
            "A backup of both emulators is taken first.</i>"
        ))

        form = QFormLayout()
        self._source = QComboBox()
        self._target = QComboBox()
        emulators = self._discover_emulators()
        self._source.addItems(emulators)
        self._target.addItems(emulators)
        if len(emulators) >= 2:
            self._target.setCurrentIndex(1)
        self._source.currentIndexChanged.connect(self._refresh_preview)
        self._target.currentIndexChanged.connect(self._refresh_preview)
        form.addRow("Source emulator:", self._source)
        form.addRow("Target emulator:", self._target)
        layout.addLayout(form)

        self._backup_first = QCheckBox("Take a backup before migrating")
        self._backup_first.setChecked(True)
        self._backup_first.setEnabled(False)  # always on; cmd_migrate does this
        layout.addWidget(self._backup_first)

        layout.addWidget(QLabel("<b>Entries that will be migrated:</b>"))
        self._preview = QListWidget()
        layout.addWidget(self._preview)

        row = QHBoxLayout()
        row.addStretch()
        cancel = QPushButton("Cancel")
        cancel.clicked.connect(self.reject)
        migrate = QPushButton("Migrate")
        migrate.setDefault(True)
        migrate.clicked.connect(self._migrate)
        row.addWidget(cancel)
        row.addWidget(migrate)
        layout.addLayout(row)

        self._refresh_preview()

    def _discover_emulators(self) -> List[str]:
        """Find emulator subdirectories in the project dir."""
        out = []
        if os.path.isdir(self._project_dir):
            for entry in sorted(os.listdir(self._project_dir)):
                p = os.path.join(self._project_dir, entry, "symlinks.json")
                if os.path.isfile(p):
                    out.append(entry)
        return out

    def _refresh_preview(self):
        self._preview.clear()
        src = self._source.currentText()
        tgt = self._target.currentText()
        if not src or not tgt or src == tgt:
            self._preview.addItem("(pick a different source and target)")
            return
        src_path = os.path.join(self._project_dir, src, "symlinks.json")
        tgt_path = os.path.join(self._project_dir, tgt, "symlinks.json")
        if not (os.path.isfile(src_path) and os.path.isfile(tgt_path)):
            self._preview.addItem("(missing symlinks.json on source or target)")
            return
        try:
            with open(src_path) as f:
                src_cfg = json.load(f)
            with open(tgt_path) as f:
                tgt_cfg = json.load(f)
        except (OSError, json.JSONDecodeError) as e:
            self._preview.addItem(f"(couldn't read manifest: {e})")
            return
        overlap = [k for k in src_cfg if k != "flatpak" and k in tgt_cfg]
        if not overlap:
            self._preview.addItem("(no overlapping entries — nothing to migrate)")
            return
        for entry in overlap:
            src_dir = os.path.join(self._project_dir, src, entry)
            present = "✓" if os.path.exists(src_dir) else "✗ (missing on source)"
            self._preview.addItem(f"{present}  {entry}")

    def _migrate(self):
        src = self._source.currentText()
        tgt = self._target.currentText()
        if not src or not tgt or src == tgt:
            QMessageBox.warning(self, "Migrate", "Pick a different source and target.")
            return
        confirm = QMessageBox.question(
            self, "Migrate",
            f"Copy overlapping configs from {src} → {tgt}? Both will be backed up first.",
        )
        if confirm != QMessageBox.Yes:
            return
        # Run on a worker so the UI stays responsive during big copies.
        from gui.dialogs.progress import ProgressDialog
        progress = ProgressDialog(f"Migrating {src} → {tgt}", parent=self)
        progress.show()

        worker = CoreWorker(cmd_migrate, make_args(
            config=os.path.join(self._project_dir, "setup.json"),
            source=src, target=tgt,
        ))
        self._worker = worker  # keep reference so it isn't GC'd

        def on_progress(text: str):
            progress.append(text)

        def on_finished(ok: bool):
            progress.set_finished(ok)
            if ok:
                QMessageBox.information(self, "Migrate", "Migration complete.")
                self.accept()
            else:
                QMessageBox.critical(self, "Migrate", "Migration failed — see log.")

        worker.progress.connect(on_progress)
        worker.finished_ok.connect(on_finished)
        worker.start()
