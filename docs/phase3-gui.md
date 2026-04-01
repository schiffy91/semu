# Phase 3: GUI

A cross-platform desktop GUI that wraps the Phase 2 CLI, providing a visual interface for managing emulators.

## Tooling

| Concern | Choice | Why |
|---|---|---|
| Framework | **Python + Qt (PySide6)** | Cross-platform, native look, no extra runtime. Same Python codebase as the CLI. |
| Packaging | **PyInstaller** or **Nuitka** | Single executable per platform, no Python install required for end users |
| Auto-update (self) | Built-in check | The manager can update itself the same way it updates emulators |

### Why not Electron/Tauri/web?
The CLI is Python. PySide6 keeps one language, one codebase, and avoids shipping a browser runtime. Qt looks native on all three platforms without extra theming work.

## Architecture

```
setup.py (CLI)          ← Phase 2, the source of truth for all operations
   ↑
core/                   ← Shared library: install, update, backup, migrate, resolve
   ↑
gui/                    ← Phase 3, thin UI layer that calls into core/
   ├── main_window.py
   ├── emulator_card.py
   ├── dialogs/
   └── workers.py       ← QThread wrappers for async operations
```

The GUI never implements business logic directly. Every action calls the same `core.*` functions the CLI uses. This means:
- CLI and GUI always behave identically
- GUI is optional — power users can stick with the CLI
- Phase 1 smoke tests cover the core logic for both interfaces

## Main Window

```
+-------------------------------------------------------+
|  Emulation Manager                          [Settings] |
+-------------------------------------------------------+
|                                                        |
|  +--------------------------------------------------+ |
|  | Dolphin                             v2603a        | |
|  | GameCube, Wii               Latest    [Update]    | |
|  +--------------------------------------------------+ |
|  +--------------------------------------------------+ |
|  | Azahar                              v2125.0.1     | |
|  | Nintendo 3DS                Latest    [Update]    | |
|  +--------------------------------------------------+ |
|  +--------------------------------------------------+ |
|  | PCSX2                               v2.6.3       | |
|  | PlayStation 2            -> v2.7.0   [Update]     | |
|  +--------------------------------------------------+ |
|  +--------------------------------------------------+ |
|  | Cemu                                v2.6          | |
|  | Wii U                      Latest    [Update]     | |
|  +--------------------------------------------------+ |
|  +--------------------------------------------------+ |
|  | RetroArch                           v1.22.2       | |
|  | Multi-system               Latest    [Update]     | |
|  +--------------------------------------------------+ |
|  +--------------------------------------------------+ |
|  | Ryujinx (Ryubing)                   v1.3.3        | |
|  | Nintendo Switch             Latest    [Update]    | |
|  +--------------------------------------------------+ |
|  +--------------------------------------------------+ |
|  | ES-DE                              v3.4.0         | |
|  | Frontend                    Latest    [Update]    | |
|  +--------------------------------------------------+ |
|                                                        |
|  [Install All]  [Update All]  [Backup All]             |
|                                                        |
+-------------------------------------------------------+
|  Ready.                                                |
+-------------------------------------------------------+
```

Platform availability shown per-card: if an emulator has no build for the current platform, the card shows "Not available on [platform]" with the Install/Update buttons disabled.

## Views / Dialogs

### Emulator Card (main list item)
- Name, systems, installed version
- Status indicator: up-to-date, update available, not installed, not available on platform
- Right-click context menu: Install, Update, Backup, Uninstall, Revert to Original, Open config folder, Open install folder

### Originals Browser Dialog
- Accessed via right-click "Revert to Original" on any emulator card
- Lists all captured originals with version and capture date (from `originals/manifest.json`)
- Preview: shows which files/dirs will be restored
- "Restore" button auto-backs up current config before overwriting
- "Update Originals" button: re-captures defaults from currently installed version as a new entry
- No delete button — originals are append-only, never removable

### Settings Dialog
- Platform paths (`bin`, `host`, `portable`) — edits `config.json`
- Backup retention count
- Check for updates on launch (toggle)
- Theme (system / light / dark)

### Migration Dialog
- Source emulator dropdown (auto-populated from `migrates_from` fields)
- Target emulator (pre-selected)
- Checklist of what will be migrated (saves, config, cheats, etc.)
- Preview of paths that will be copied
- "Backup first" checkbox (checked by default)
- Progress bar + log output

### Progress / Log Panel
- Shown during install, update, backup, migrate
- Streams output from the core library (same output the CLI prints)
- Cancel button for long operations

### First-Run Wizard (nice-to-have)
- Detects platform
- Asks where to store emulator binaries
- Shows which emulators are available on this platform
- Offers to install all in one go

## Async Operations

All install/update/backup/migrate operations run in `QThread` workers so the UI stays responsive:

```python
class InstallWorker(QThread):
    progress = Signal(str)
    finished = Signal(bool)

    def __init__(self, emulator_name):
        super().__init__()
        self.emulator_name = emulator_name

    def run(self):
        # Calls core.install() — same function the CLI uses
        # Emits progress signals for the log panel
        ...
```

## System Tray (nice-to-have)

- Minimize to tray
- Background update checks on a schedule
- Notification when updates are available
- Quick actions: update all, open ES-DE

## Packaging

| Platform | Format | Tool | Test in |
|---|---|---|---|
| Windows | `.exe` installer or portable `.zip` | PyInstaller | Phase 1 Windows VM |
| macOS | `.app` in `.dmg` | PyInstaller + `create-dmg` | Phase 1 host |
| Linux | AppImage | PyInstaller or Nuitka | Phase 1 Linux VM |

The packaged app bundles Python + PySide6 + the CLI core, so users don't need Python installed.

## Project Structure Addition

```
gui/                         # Phase 3 addition to the project
├── __init__.py
├── app.py                   # QApplication setup, entry point
├── main_window.py           # Main window with emulator list
├── emulator_card.py         # Individual emulator card widget
├── dialogs/
│   ├── settings.py          # Settings dialog (edits config.json)
│   ├── migration.py         # Migration wizard
│   └── progress.py          # Progress/log panel
├── workers.py               # QThread wrappers for core operations
└── resources/               # Icons, etc.
```

Entry point: `python setup.py gui` launches the GUI. All other subcommands remain CLI.

## Implementation Order

1. **Refactor Phase 2 into `core/` library** — separate CLI argument parsing from business logic so the GUI can import `core.install()`, `core.update()`, etc.
2. **Scaffold PySide6 app** — main window, emulator card widget, status bar
3. **Emulator list view** — reads `emulator.json` manifests + `versions.json`, renders cards with platform availability
4. **Install/Update actions** — QThread workers calling core, progress in status bar
5. **Settings dialog** — edit `config.json` paths
6. **Backup action** — with progress
7. **Migration dialog** — source/target selection, preview, execute
8. **Packaging** — PyInstaller configs for all three platforms, test in Phase 1 VMs
9. **Polish** — icons, theme support, first-run wizard, system tray
