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
        """Fetch the version manifest in the background, IF the user has
        opted in via setup.json's `check_for_updates` flag (default: false
        to keep launch offline-friendly; users in restricted networks won't
        see hangs from a background HTTPS request).

        Critic finding #34.
        """
        if not self._setting("check_for_updates", default=False):
            return
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

    def _setting(self, name: str, default):
        """Read a setting from setup.json, falling back to `default`."""
        config_path = self._config_path()
        if not os.path.exists(config_path):
            return default
        try:
            with open(config_path) as f:
                cfg = json.load(f)
        except (OSError, json.JSONDecodeError):
            return default
        return cfg.get(name, default)

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
        # Track every spawned worker so the QThread isn't GC'd mid-run, and
        # prune finished ones to avoid an unbounded list (critic finding #20).
        if not hasattr(self, "_workers"):
            self._workers = []
        self._workers = [w for w in self._workers if w.isRunning()]
        self._workers.append(worker)

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
        # Detect pre-existing user data at the OS-level config paths and
        # offer to migrate before silent failure (round-6 critic finding #3).
        # Without this, users with prior Homebrew/apt Dolphin installs see
        # "Installed 1/1" but their saves stay outside the project dir.
        migrate = self._maybe_offer_migration(self._emulators_arg(name))
        args = make_args(
            config=self._config_path(),
            emulators=self._emulators_arg(name),
        )
        if migrate:
            args.migrate = True
        self._run_worker(
            lifecycle.install,
            args,
            f"Installing {name or 'all emulators'}",
        )

    def _maybe_offer_migration(self, emulator_names) -> bool:
        """If any of the targeted emulators has existing user data at its OS
        link path, ask the user whether to migrate it. Returns True if the
        user opted in."""
        from core.symlinks import detect_existing_user_data, parse_config, filter_emulators
        try:
            parsed = parse_config(self._config_path(), self._project_dir)
        except Exception:
            return False
        parsed = filter_emulators(parsed, emulator_names or [])
        existing = detect_existing_user_data(parsed)
        if not existing:
            return False
        lines = []
        for emu, paths in existing.items():
            lines.append(f"<b>{emu}</b>")
            for p in paths:
                lines.append(f"  • {p}")
        body = (
            "Schemulator detected existing data at these locations:<br><br>"
            + "<br>".join(lines)
            + "<br><br>Move it into the project directory so it gets cloud-synced "
            "with the rest of your saves? (Anything already in the project dir "
            "wins — peers' saves aren't overwritten.)"
        )
        result = QMessageBox.question(
            self, "Migrate existing data?", body,
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        )
        return result == QMessageBox.StandardButton.Yes

    def _on_update(self, name: str):
        # Warn if any of the targeted emulators is currently running. Update
        # renames the result-<emu> symlink mid-run; on macOS this can crash
        # the live emulator process (mmap'd .app pages unmap). Round-6 #5.
        targets = self._emulators_arg(name)
        if not targets:
            from gui.manifest import EMULATORS
            targets = [m.name for m in EMULATORS]
        running = lifecycle.running_emulators(targets)
        if running:
            confirm = QMessageBox.warning(
                self, "Emulator running",
                f"These emulators are currently running and may crash if updated:\n  "
                + "\n  ".join(running)
                + "\n\nQuit them before continuing. Proceed anyway?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.Cancel,
                QMessageBox.StandardButton.Cancel,
            )
            if confirm != QMessageBox.StandardButton.Yes:
                return
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
        """If critical prereqs are missing, surface a warning AND disable
        install/update/rollback buttons on every card. Without disabling,
        the user clicks Install, waits for a worker spin-up, then sees
        'nix not found' inside a ProgressDialog (round-6 critic finding #10).
        """
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
        for card in self._cards.values():
            card.action_button.setEnabled(False)
            card.action_button.setText("Install Nix first")
            card.action_button.setToolTip(
                "Install Nix (https://nixos.org/download.html) and relaunch Schemulator."
            )

    def recover_interrupted_updates(self) -> None:
        """Detect and offer to complete any update interrupted by a crash.

        See core.lifecycle.detect_interrupted_updates for the failure mode.
        Round-2 critic finding #5.
        """
        pending = lifecycle.detect_interrupted_updates(self._project_dir)
        if not pending:
            return
        names = ", ".join(pending)
        result = QMessageBox.question(
            self,
            "Interrupted update detected",
            f"It looks like a previous update of {names} was interrupted "
            f"(probably from a crash or power loss). The new build is on disk "
            f"but wasn't promoted into place. Complete the update now?",
        )
        if result != QMessageBox.Yes:
            return
        for emu in pending:
            try:
                lifecycle.recover_interrupted_update(self._project_dir, emu)
            except Exception as e:
                self.statusBar().showMessage(f"Recovery failed for {emu}: {e}", 8000)
        self._refresh_status()

    def maybe_show_first_run_wizard(self) -> bool:
        """Run the first-run wizard if this looks like a fresh install. Returns
        True if the user completed it, False if they skipped/cancelled or it
        wasn't shown.

        Round-3 critic finding #3: previously the chosen project dir was
        assigned to self._project_dir but the install worker was invoked with
        config=None, which made resolve_config() fall back to the bundled
        setup.json next to setup.py — so install ran against the schemulator
        repo root, NOT the user's chosen dir. We now seed setup.json into the
        chosen dir and pass the explicit config path through to the worker.
        """
        if has_run_before(self._project_dir):
            return False
        wiz = FirstRunWizard(self._project_dir, parent=self)
        if wiz.exec() != 1:
            return False
        chosen_dir = wiz.project_dir() or self._project_dir
        if chosen_dir != self._project_dir:
            self._project_dir = chosen_dir
        # Seed a default setup.json in the chosen dir if none exists yet.
        self._seed_project_dir(self._project_dir)
        # Title bar / header text now references the new dir.
        self._refresh_status()

        # Wire the chosen SD card BEFORE install so the first launch of ES-DE
        # finds ROMs on the card (round-6 #7).
        sd_card = wiz.chosen_sd_card()
        if sd_card:
            from core import sdcard as _sd
            written = _sd.wire_es_de_to_card(sd_card, self._project_dir)
            if written:
                self.statusBar().showMessage(
                    f"Wired ES-DE ROM root to {sd_card.mount_path}", 8000,
                )

        emulators = wiz.selected_emulators()
        if emulators:
            self._run_worker(
                lifecycle.install,
                make_args(config=self._config_path(), emulators=emulators),
                f"Installing {len(emulators)} emulators",
            )
        return True

    def _seed_project_dir(self, project_dir: str) -> None:
        """Create a default setup.json + per-emulator dirs in the user's
        chosen project dir if they don't exist yet. We copy from the bundled
        repo's templates so the lifecycle's parse_config has something to
        work against."""
        os.makedirs(project_dir, exist_ok=True)
        setup_json = os.path.join(project_dir, "setup.json")
        if not os.path.exists(setup_json):
            defaults = {
                "host": {
                    "linux":   os.path.expanduser("~/.config"),
                    "macos":   os.path.expanduser("~/Library/Application Support"),
                    "windows": os.path.expanduser("~/AppData/Roaming"),
                },
                "portable": {
                    "linux":   os.path.expanduser("~/ES-DE"),
                    "macos":   os.path.expanduser("~/ES-DE"),
                    "windows": os.path.expanduser("~/Documents/ES-DE"),
                },
            }
            with open(setup_json, "w") as f:
                json.dump(defaults, f, indent=4)

        # Copy each bundled emulator dir into the project dir if missing.
        # We use a shallow copytree so the user's existing data isn't touched.
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        import shutil
        from gui.manifest import EMULATORS
        for meta in EMULATORS:
            src = os.path.join(repo_root, meta.name)
            dst = os.path.join(project_dir, meta.name)
            if os.path.isdir(src) and not os.path.exists(dst):
                try:
                    shutil.copytree(src, dst)
                except OSError:
                    pass

    @staticmethod
    def _open_in_file_manager(path: str):
        import subprocess
        if sys.platform == "darwin":
            subprocess.Popen(["open", path])
        elif sys.platform == "win32":
            os.startfile(path)  # type: ignore[attr-defined]
        else:
            subprocess.Popen(["xdg-open", path])
