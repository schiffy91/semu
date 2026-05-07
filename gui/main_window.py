"""Main installer window: emulator cards + global actions + status bar."""

from __future__ import annotations

import json
import os
import sys

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QFileDialog,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QStatusBar,
    QVBoxLayout,
    QWidget,
)

import atexit

from core import controllers, lifecycle, state, steam, syncthing, updater
from core.backup import cmd_backup, cmd_revert
from gui.dialogs.about import AboutDialog
from gui.dialogs.first_run import FirstRunWizard, has_run_before
from gui.dialogs.migration import MigrationDialog
from gui.dialogs.originals import OriginalsDialog
from gui.dialogs.progress import ProgressDialog
from gui.dialogs.settings import SettingsDialog
from gui.dialogs.steamdeck import SteamDeckDialog
from gui.dialogs.syncthing import SyncthingDialog
from gui.emulator_card import EmulatorCard
from gui.manifest import EMULATORS
from gui.workers import CoreWorker, make_args


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Schemulator")
        self.resize(820, 720)
        self._project_dir = self._discover_project_dir()
        self._cards = {}
        self._current_worker = None
        self._progress = None

        central = QWidget()
        self.setCentralWidget(central)

        layout = QVBoxLayout(central)

        layout.addWidget(self._build_header())
        layout.addWidget(self._build_card_list(), stretch=1)
        layout.addLayout(self._build_global_actions())

        self.setStatusBar(QStatusBar())
        self._latest_versions = {}
        self._syncthing_proc = None
        self._refresh_status()
        # Background tasks: manifest fetch + (optional) syncthing autostart.
        self._kick_off_manifest_fetch()
        self._maybe_autostart_syncthing()
        atexit.register(self._stop_syncthing)

    def _discover_project_dir(self) -> str:
        env = os.environ.get("SCHEMULATOR_PROJECT_DIR")
        if env and os.path.isdir(env):
            return env
        return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    def _build_header(self) -> QWidget:
        wrap = QWidget()
        layout = QHBoxLayout(wrap)
        title = QLabel("<h2>Schemulator</h2>")
        sub = QLabel(f"Project: <code>{self._project_dir}</code>  |  Platform: <b>{state.PLATFORM}</b>")
        sub.setStyleSheet("color: #aaa;")
        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)

        change_btn = QPushButton("Change project…")
        change_btn.clicked.connect(self._choose_project_dir)
        settings_btn = QPushButton("Settings…")
        settings_btn.clicked.connect(self._open_settings)
        about_btn = QPushButton("About")
        about_btn.clicked.connect(self._open_about)

        layout.addWidget(title)
        layout.addWidget(sub)
        layout.addWidget(spacer)
        layout.addWidget(change_btn)
        layout.addWidget(settings_btn)
        layout.addWidget(about_btn)
        return wrap

    def _build_card_list(self) -> QWidget:
        container = QWidget()
        layout = QVBoxLayout(container)
        layout.setSpacing(8)
        for meta in EMULATORS:
            card = EmulatorCard(meta, current_platform=state.PLATFORM)
            card.install_clicked.connect(self._on_install)
            card.update_clicked.connect(self._on_update)
            card.uninstall_clicked.connect(self._on_uninstall)
            card.backup_clicked.connect(self._on_backup)
            card.rollback_clicked.connect(self._on_rollback)
            card.revert_clicked.connect(self._on_revert)
            card.open_config_clicked.connect(self._on_open_config)
            card.open_install_clicked.connect(self._on_open_install)
            card.apply_controller_clicked.connect(self._on_apply_controller)
            layout.addWidget(card)
            self._cards[meta.name] = card
        layout.addStretch()
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(container)
        return scroll

    def _build_global_actions(self) -> QHBoxLayout:
        row = QHBoxLayout()
        install_all = QPushButton("Install All")
        install_all.clicked.connect(lambda: self._on_install(""))
        update_all = QPushButton("Update All")
        update_all.clicked.connect(lambda: self._on_update(""))
        backup_all = QPushButton("Backup All")
        backup_all.clicked.connect(lambda: self._on_backup(""))

        migrate_btn = QPushButton("Migrate…")
        migrate_btn.clicked.connect(self._open_migration)

        steamdeck_btn = QPushButton("Steam Deck setup…")
        steamdeck_btn.clicked.connect(self._open_steamdeck)
        if state.PLATFORM != "linux":
            steamdeck_btn.setEnabled(False)

        sync_btn = QPushButton("Sync saves…")
        sync_btn.clicked.connect(self._open_syncthing)

        row.addWidget(install_all)
        row.addWidget(update_all)
        row.addWidget(backup_all)
        row.addWidget(migrate_btn)
        row.addStretch()
        row.addWidget(steamdeck_btn)
        row.addWidget(sync_btn)
        return row

    # -------- versions --------

    def _refresh_status(self):
        installed = updater.installed_versions(self._project_dir)
        for name, card in self._cards.items():
            v = installed.get(name.lower())
            latest = self._latest_versions.get(name.lower()) if hasattr(self, "_latest_versions") else None
            card.set_installed(version=v, update_available_to=latest)
        self.statusBar().showMessage(
            f"{sum(1 for v in installed.values() if v)}/{len(self._cards)} installed"
        )

    def _kick_off_manifest_fetch(self):
        """Fetch the version manifest in the background so update availability
        shows up without blocking startup."""
        from PySide6.QtCore import QThread, Signal

        if hasattr(self, "_manifest_thread") and self._manifest_thread.isRunning():
            return

        class ManifestFetcher(QThread):
            done = Signal(dict)

            def run(self):
                m = updater.fetch_manifest()
                self.done.emit(m.emulators if m else {})

        self._manifest_thread = ManifestFetcher()
        self._manifest_thread.done.connect(self._on_manifest_loaded)
        self._manifest_thread.start()

    def _on_manifest_loaded(self, emulators: dict):
        self._latest_versions = {
            name.lower(): info.get("version", "") for name, info in emulators.items()
        }
        self._refresh_status()

    def _maybe_autostart_syncthing(self):
        """Start the bundled syncthing sidecar if a binary is available and
        the user hasn't disabled it via setup.json or the env var."""
        if os.environ.get("SCHEMULATOR_SKIP_AUTOSTART") == "1":
            return
        config_path = self._config_path()
        if os.path.exists(config_path):
            try:
                with open(config_path) as f:
                    cfg = json.load(f)
                if not cfg.get("autostart_syncthing", True):
                    return
            except (OSError, ValueError, json.JSONDecodeError):
                pass
        if syncthing.find_binary() is None:
            return
        # Don't block startup; spawn in a thread.
        from PySide6.QtCore import QThread, Signal

        class Starter(QThread):
            done = Signal(object)

            def run(self):
                proc = syncthing.start(wait_for_ready=8)
                self.done.emit(proc)

        self._st_starter = Starter()
        self._st_starter.done.connect(self._on_syncthing_started)
        self._st_starter.start()

    def _on_syncthing_started(self, proc):
        self._syncthing_proc = proc
        if proc and proc.poll() is None:
            self.statusBar().showMessage("Syncthing sidecar running.", 5000)

    def _stop_syncthing(self):
        if self._syncthing_proc is not None:
            syncthing.stop(self._syncthing_proc)
            self._syncthing_proc = None

    def closeEvent(self, event):
        self._stop_syncthing()
        super().closeEvent(event)

    def _choose_project_dir(self):
        chosen = QFileDialog.getExistingDirectory(self, "Choose project directory", self._project_dir)
        if chosen:
            self._project_dir = chosen
            self._refresh_status()

    # -------- worker plumbing --------

    def _run_worker(self, fn, args, label: str):
        if self._current_worker and self._current_worker.isRunning():
            QMessageBox.information(self, "Busy", "Another operation is in progress.")
            return
        self._progress = ProgressDialog(label, parent=self)
        self._progress.show()

        worker = CoreWorker(fn, args)
        worker.progress.connect(self._progress.append)
        worker.finished_ok.connect(self._on_worker_finished)
        self._current_worker = worker
        worker.start()

    def _on_worker_finished(self, ok: bool):
        if self._progress:
            self._progress.set_finished(ok)
        self._refresh_status()

    # -------- card slots --------

    def _emulators_arg(self, name: str):
        return [name] if name else []

    def _config_path(self) -> str:
        return os.path.join(self._project_dir, "setup.json")

    def _on_install(self, name: str):
        self._run_worker(
            lifecycle.install,
            make_args(config=self._config_path(), emulators=self._emulators_arg(name)),
            f"Installing {name or 'all emulators'}",
        )

    def _on_update(self, name: str):
        self._run_worker(
            lifecycle.update,
            make_args(config=self._config_path(), emulators=self._emulators_arg(name)),
            f"Updating {name or 'all emulators'}",
        )

    def _on_uninstall(self, name: str):
        if QMessageBox.question(
            self, "Uninstall", f"Remove symlinks for {name}? Project-dir config is preserved."
        ) != QMessageBox.Yes:
            return
        self._run_worker(
            lifecycle.uninstall,
            make_args(config=self._config_path(), emulators=self._emulators_arg(name)),
            f"Uninstalling {name}",
        )

    def _on_backup(self, name: str):
        self._run_worker(
            cmd_backup,
            make_args(config=self._config_path(), emulators=self._emulators_arg(name)),
            f"Backing up {name or 'all emulators'}",
        )

    def _on_rollback(self, name: str):
        if QMessageBox.question(
            self, "Rollback", f"Roll back {name} to the previous build?"
        ) != QMessageBox.Yes:
            return
        self._run_worker(
            lifecycle.rollback,
            make_args(config=self._config_path(), emulators=self._emulators_arg(name)),
            f"Rolling back {name}",
        )

    def _on_revert(self, name: str):
        # Open the originals browser instead of blindly reverting to latest.
        OriginalsDialog(name, self._project_dir, parent=self).exec()
        self._refresh_status()

    def _on_open_config(self, name: str):
        target = os.path.join(self._project_dir, name)
        if not os.path.isdir(target):
            QMessageBox.information(self, "Open config", f"{target} doesn't exist yet.")
            return
        self._open_in_file_manager(target)

    def _on_open_install(self, name: str):
        # nix's `result-<emu>` symlink is what we hand to the file manager.
        from core import lifecycle
        target = lifecycle._result_dir(self._project_dir, name)
        if not os.path.lexists(target):
            QMessageBox.information(self, "Open install folder",
                                    f"{name} isn't installed yet.")
            return
        # Resolve the symlink so the file manager opens the actual store path.
        real = os.path.realpath(target)
        self._open_in_file_manager(real)

    def _on_apply_controller(self, name: str):
        profiles = controllers.list_profiles()
        if not profiles:
            QMessageBox.information(self, "Controllers", "No bundled controller profiles found.")
            return
        chosen, ok = QInputDialog.getItem(
            self, "Apply controller defaults",
            f"Choose a profile for {name}:", profiles, 0, False,
        )
        if not ok:
            return
        applied = controllers.apply(self._project_dir, name, chosen)
        msg = "Applied." if applied else "No bundled fragment for that combination."
        QMessageBox.information(self, "Controllers", msg)

    # -------- global dialogs --------

    def _open_steamdeck(self):
        SteamDeckDialog(self._project_dir, parent=self).exec()
        self._refresh_status()

    def _open_syncthing(self):
        SyncthingDialog(self._project_dir, parent=self).exec()

    def _open_settings(self):
        if SettingsDialog(self._project_dir, parent=self).exec():
            self._refresh_status()

    def _open_migration(self):
        MigrationDialog(self._project_dir, parent=self).exec()
        self._refresh_status()

    def _open_about(self):
        AboutDialog(parent=self).exec()

    def show_prereq_warnings(self) -> None:
        """If critical prereqs are missing, surface a warning. Non-blocking."""
        from core import prereqs
        critical = prereqs.critical_missing()
        if not critical:
            return
        names = ", ".join(p.name for p in critical)
        hints = "\n\n".join(p.install_hint for p in critical)
        QMessageBox.warning(
            self,
            "Missing prerequisite",
            f"Schemulator needs {names} for install/update operations.\n\n{hints}",
        )

    def maybe_show_first_run_wizard(self) -> bool:
        """Run the first-run wizard if this looks like a fresh install. Returns
        True if the user completed it, False if they skipped/cancelled or it
        wasn't shown."""
        if has_run_before(self._project_dir):
            return False
        wiz = FirstRunWizard(self._project_dir, parent=self)
        if wiz.exec() != 1:
            return False
        chosen_dir = wiz.project_dir() or self._project_dir
        if chosen_dir != self._project_dir:
            self._project_dir = chosen_dir
        emulators = wiz.selected_emulators()
        if emulators:
            from core.lifecycle import install
            self._run_worker(
                install,
                make_args(config=None, emulators=emulators),
                f"Installing {len(emulators)} emulators",
            )
        return True

    @staticmethod
    def _open_in_file_manager(path: str):
        import subprocess
        if sys.platform == "darwin":
            subprocess.Popen(["open", path])
        elif sys.platform == "win32":
            os.startfile(path)  # type: ignore[attr-defined]
        else:
            subprocess.Popen(["xdg-open", path])
