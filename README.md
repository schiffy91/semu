# Schemulator

Deterministic, reproducible emulation environment powered by Nix. All emulators bundled, all configs cloud-synced via symlinks.

## Quick Start

```sh
# Build and install everything
nix build github:schiffy91/schemulator

# Or run directly
nix run github:schiffy91/schemulator -- symlink

# Dev shell (for contributing)
nix develop
```

## Emulators

| Emulator   | Systems               | macOS | Linux |
|------------|-----------------------|-------|-------|
| Dolphin    | GameCube, Wii         | Yes   | Yes   |
| Azahar     | Nintendo 3DS          | Yes   | Yes   |
| PCSX2      | PlayStation 2         | Yes   | Yes   |
| Cemu       | Wii U                 | Yes   | Yes   |
| RetroArch  | Multi-system (cores)  | Yes   | Yes   |
| Ryujinx    | Nintendo Switch       | Yes   | Yes   |
| Lime3DS    | Nintendo 3DS (legacy) | —     | Yes   |
| ES-DE      | Frontend              | Yes   | Yes   |

All emulators are pinned to exact versions via `flake.lock`. macOS variants
live in `nix/<emulator>-mac.nix`; Linux uses upstream nixpkgs.

## How It Works

The project directory (cloud-synced via Google Drive / Syncthing) is the **source of truth**. `schemulator symlink` creates symlinks from each platform's config paths into this directory. Emulators read/write their configs through the symlinks.

```
~/Library/Application Support/dolphin-emu/ --> project/Dolphin/config/
~/Library/Application Support/azahar-emu/  --> project/Azahar/data/
...
```

## Commands

```sh
# Lifecycle
schemulator install [emulator...]            # Build emulators + symlink configs (captures baseline)
schemulator update  [emulator...]            # Atomic update; keeps prev build for rollback
schemulator uninstall [emulator...]          # Remove symlinks (preserves project-dir data)
schemulator rollback [emulator...]           # Swap back to previous build + revert config

# Configs
schemulator symlink [emulator...]            # Re-wire configs without rebuilding
schemulator status  [emulator...]            # Show symlink status
schemulator backup  [emulator...]            # Zip configs (auto-rotates, keeps 5)
schemulator capture <emulator> <version>     # Snapshot config as immutable original
schemulator originals <emulator>             # List captured originals
schemulator revert <emulator> [version]      # Restore from original (auto-backs up)
schemulator migrate <source> <target>        # Copy configs between emulators

# Steam Deck / controllers / sync
schemulator sd-scan                          # Detect SD cards + scan ROMs + check firmware
schemulator controllers <profile> [emu]      # Apply xbox / dualsense / steamdeck profile
schemulator steam-shortcut --exe PATH        # Add ES-DE as a non-Steam game
schemulator sync {init,start,status,id,share,pair}  # Manage Syncthing sidecar

# UI
schemulator gui                              # Launch the desktop installer/updater
```

## Desktop GUI

```sh
schemulator gui   # or: make gui
```

The GUI (PySide6) shows one card per emulator with install / update / rollback /
revert actions. From the main window:

- **Steam Deck setup…** — scan an SD card for ROMs, write a non-Steam shortcut
  for ES-DE, install the bundled Steam Input layout, surface missing
  BIOS / firmware.
- **Sync saves…** — bootstrap the bundled Syncthing sidecar and pair other
  devices via their device ID. Syncs only `<project_dir>/saves/` so ROM paths
  can differ across devices.
- Per-card menu: Apply controller defaults (Xbox / DualSense / generic-XInput /
  Deck), open config folder, uninstall.

See [docs/phase3-gui.md](docs/phase3-gui.md) for architecture details.

## NixOS

```nix
# In your flake.nix
{
  inputs.schemulator.url = "github:schiffy91/schemulator";

  outputs = { self, nixpkgs, schemulator, ... }: {
    nixosConfigurations.myhost = nixpkgs.lib.nixosSystem {
      modules = [
        schemulator.nixosModules.default
        {
          programs.schemulator = {
            enable = true;
            configDir = "/home/user/GoogleDrive/media/Games/Emulation";
          };
        }
      ];
    };
  };
}
```

## Development

```sh
make test             # Run tests locally (macOS)
make container-test   # Run tests in Arch Linux container
make linux            # Start QEMU VM for full-system testing
make linux-test       # Sync + run tests in VM
make help             # Show all targets
```

## Tools

- `decrypt3ds.py` — Fixes NoCrypto flag on already-decrypted 3DS ROMs for Azahar compatibility
