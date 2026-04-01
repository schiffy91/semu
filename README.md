# Emulation

Portable, cloud-synced emulation setup that works across Windows, macOS, and Linux machines.

## Goal

Store all emulator configuration, save data, and ROMs in a single cloud-synced directory (Google Drive / Syncthing) so that any machine can be set up with a fully configured emulation environment by running one script.

## How It Works

The project directory is the **read-only source of truth**. The setup tool only creates symlinks from host filesystem locations into this directory — it never modifies files here. Emulators read and write their configs through the symlinks.

Each emulator has a manifest declaring where its files should be symlinked on each platform. Running `setup.py` resolves platform-specific paths and creates the symlinks.

## Supported Emulators

| Emulator   | Systems               |
|------------|-----------------------|
| Azahar     | Nintendo 3DS          |
| Dolphin    | GameCube, Wii         |
| PCSX2      | PlayStation 2         |
| RetroArch  | Multi-system (cores)  |
| Cemu       | Wii U                 |
| Ryujinx    | Nintendo Switch       |
| ES-DE      | Frontend              |

## Roadmap

| Phase | Description | Plan |
|---|---|---|
| 0 | Decrypt encrypted 3DS ROMs for Azahar compatibility | [docs/phase0-rom-decryption.md](docs/phase0-rom-decryption.md) |
| 1 | QEMU-based VM test environment (Linux, Windows, macOS) | [docs/phase1-test-environment.md](docs/phase1-test-environment.md) |
| 2 | CLI lifecycle manager (install, update, backup, migrate, revert) | [docs/phase2-lifecycle-manager.md](docs/phase2-lifecycle-manager.md) |
| 3 | PySide6 GUI wrapping the CLI | [docs/phase3-gui.md](docs/phase3-gui.md) |

## Usage (current)

```sh
python setup.py
```
