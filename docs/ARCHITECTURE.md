# Architecture

Schemulator is split into three layers:

```
┌─────────────────────────────────────────────────┐
│  gui/   PySide6 desktop UI (optional)           │
│         main_window, emulator_card, dialogs/    │
├─────────────────────────────────────────────────┤
│  setup.py    CLI entry (argparse shim)          │
│  core/cli.py  command dispatcher                │
├─────────────────────────────────────────────────┤
│  core/    pure-Python library, no Qt imports    │
│           lifecycle, sdcard, steam, syncthing,  │
│           controllers, updater, backup, …       │
└─────────────────────────────────────────────────┘
```

The **core** layer is the source of truth. Both the CLI and the GUI call into
it; neither layer duplicates business logic. This is what keeps `schemulator
backup` (CLI) and clicking "Backup" in the UI byte-for-byte identical.

## Data flow

A typical "click Install on Dolphin" flow:

```
EmulatorCard         emits install_clicked("Dolphin")
   ↓
MainWindow._on_install("Dolphin")
   makes argparse.Namespace(config=<project>/setup.json, emulators=["Dolphin"])
   ↓
CoreWorker (QThread)
   redirects stdout/stderr → progress signal
   calls core.lifecycle.install(args)
   ↓
core.lifecycle.install
   parse_config → filter_emulators(["Dolphin"])
   for each emulator:
     _nix_build  (nix build .#dolphin --out-link result-dolphin)
     setup_flatpak (Linux only)
     create_symlinks (host config dir → project/Dolphin/...)
     _capture_original_if_first (snapshot baseline for rollback)
   ↓
ProgressDialog streams every print() as a log line
   ↓
CoreWorker emits finished_ok(True)
   ↓
MainWindow._refresh_status reloads installed_versions and updates each card
```

## File layout

```
core/
├── state.py         Globals: PLATFORM, DRY_RUN, NUM_ERRORS, HOST, PORTABLE
├── exec.py          Centralised dispatch for filesystem / subprocess ops
├── console.py       console_log/console_error → stdout AND stdlib logging
├── logger.py        File logger at ~/.cache/schemulator/schemulator.log
├── symlinks.py      parse_config, create_symlinks, find_emulator_dir
├── flatpak.py       Linux-only flatpak install/override/reset
├── backup.py        backup, capture, revert, migrate (atomic zip writes)
├── lifecycle.py     install/update/uninstall/rollback (with prev-build retention)
├── sdcard.py        SD detection, ROM scan, BIOS/firmware check
├── steam.py         shortcuts.vdf binary KeyValues codec
├── controllers.py   Apply bundled controller-profile fragments
├── syncthing.py     Sidecar lifecycle + REST helpers (with shutdown via API)
├── updater.py       Version manifest fetch + atomic-swap helper
├── prereqs.py       Detect nix / flatpak / syncthing availability
└── cli.py           Argparse subcommands

gui/
├── app.py           QApplication entry; runs prereq + first-run checks
├── main_window.py   Card grid + global actions
├── emulator_card.py One per emulator
├── manifest.py      Static EmulatorMeta entries
├── workers.py       CoreWorker QThread; redirects stdout to progress signal
└── dialogs/
    ├── progress.py     Streaming log + close
    ├── settings.py     Edit setup.json's host/portable paths
    ├── originals.py    List captured snapshots; capture/revert
    ├── migration.py    Cross-emulator config copy (worker-backed)
    ├── steamdeck.py    SD scan, firmware check, Steam shortcut, layout install
    ├── syncthing.py    Device ID display, peer pairing, share folder
    ├── first_run.py    3-page wizard: project dir → SD → emulator picker
    └── about.py        Version, prereqs, log file

controllers/
├── xbox/        per-emulator binding fragments (Dolphin.ini, PCSX2.ini, …)
├── dualsense/   same shape, PS5 mappings
└── steamdeck/   steam_input_template.vdf

presets/
└── steamdeck/   Deck-tuned RetroArch + ES-DE defaults

bin/             (gitignored) bundled syncthing binary, downloaded by CI
```

## Key invariants

1. **One source of truth per project.** The project directory holds every
   emulator's config under `<project>/<Emulator>/`. The OS-level paths
   (`~/.config/dolphin-emu/`, `~/Library/Application Support/Dolphin/`) are
   symlinks back into the project. This is what makes Google Drive / Syncthing
   sync work transparently.

2. **Lifecycle is reversible.** Every install captures a `baseline` original
   on first run; every update keeps the previous build at `result-<emu>-prev`.
   A rollback always has somewhere to go back to.

3. **Uninstall preserves project-dir data.** Other devices sharing the same
   cloud folder must not lose their saves because one device uninstalled.

4. **Atomic writes.** Backups are built at `<name>.zip.tmp` and renamed only
   on success. Updates rename `result-<emu>` to `-prev` only after the new
   build succeeds; on failure the previous symlink is restored.

5. **No silent data loss.** Both lifecycle.update and rollback refuse to
   overwrite a non-symlink at the swap target — if the user dropped a real
   directory at `result-<emu>-prev/`, the operation aborts with a warning
   instead of clobbering it.

6. **Progress is a first-class citizen.** Every long operation emits to
   stdout via `console_log`. The CLI prints; the GUI's `CoreWorker` redirects
   stdout to a Qt signal. Both interfaces show identical output.

## Cross-platform paths

`setup.json` defines per-platform path roots:

```json
{
  "host":     {"linux": "~/.config/", "macos": "~/Library/Application Support/", "windows": "~/AppData/Roaming/"},
  "portable": {"linux": "~/ES-DE/",   "macos": "~/ES-DE/",                       "windows": "~/Documents/ES-DE/"}
}
```

Each emulator's `symlinks.json` references these via `${host}` and `${portable}`:

```json
{
  "config": {
    "linux":   "${host}/dolphin-emu/",
    "macos":   "${host}/Dolphin Emulator/",
    "windows": "${portable}/Emulators/Dolphin-x64/User/Config/"
  }
}
```

`core.symlinks.parse_config` resolves the platform-specific entry and produces
a `{EMULATOR_UPPER: [(flatpak_id, link_path, source_path), …]}` map. Missing
platform entries are silently skipped — platform-unavailable emulators just
don't appear in the result.

## Threading model

- **GUI thread**: all Qt widgets, signal connections, dialog construction.
- **Worker thread (`CoreWorker`)**: runs one core operation at a time. Stdout
  and stderr from the operation are captured and emitted as a `progress(str)`
  signal back to the GUI thread.
- **Syncthing process**: spawned as a child of the parent Python process. Owned
  by `core.syncthing`; auto-stopped via REST `/system/shutdown` then SIGTERM.

`MainWindow._run_worker` keeps a single-worker guard: if a worker is running,
new operations show "Busy" rather than starting concurrently. This prevents
two installs racing on the same `result-<emu>` symlink.

## Testing strategy

Five layers:

| Layer | Files | What it covers |
|---|---|---|
| Unit | `test/test_setup.py`, `test_commands.py`, `test_steam.py`, `test_steam_extra.py`, `test_sdcard.py`, `test_controllers.py`, `test_updater.py`, `test_syncthing.py` (offline) | Pure functions, no I/O beyond tmp_path |
| Cross-platform | `test_cross_platform.py` | parse_config under monkeypatched PLATFORM = linux/macos/windows |
| Edge cases | `test_edge_cases.py` | Corrupt JSON, dangling symlinks, ROM exclusion in backup, max_depth, unreadable subdirs |
| Lifecycle | `test_lifecycle.py` | install/update/uninstall/rollback with mocked nix; baseline capture; failure recovery |
| GUI integration | `test_gui_integration.py`, `test_gui_workflow.py`, `test_dialogs_extra.py` | Offscreen Qt; full button-click → worker → core → result chain |
| CLI E2E | `test_cli_e2e.py` | Subprocess-invokes setup.py for every subcommand against a temp project |
| Live (opt-in) | `test_syncthing.py` (`@live`) | Real syncthing binary, real REST API, init/start/share/pair |

CI runs all of these on macOS + Linux + (where applicable) Windows via
`.github/workflows/test.yml`. The `live` tests run on Linux + macOS with a
syncthing binary downloaded into `bin/` during the workflow.
