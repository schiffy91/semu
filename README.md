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
| PCSX2      | PlayStation 2         | —     | Yes   |
| Cemu       | Wii U                 | —     | Yes   |
| RetroArch  | Multi-system (cores)  | —     | Yes   |
| Ryujinx    | Nintendo Switch       | Yes   | Yes   |
| ES-DE      | Frontend              | Yes   | Yes   |

All emulators are pinned to exact versions via `flake.lock`.

## How It Works

The project directory (cloud-synced via Google Drive / Syncthing) is the **source of truth**. `schemulator symlink` creates symlinks from each platform's config paths into this directory. Emulators read/write their configs through the symlinks.

```
~/Library/Application Support/dolphin-emu/ --> project/Dolphin/config/
~/Library/Application Support/azahar-emu/  --> project/Azahar/data/
...
```

## Commands

```sh
schemulator symlink [emulator...]         # Wire configs into host paths
schemulator status  [emulator...]         # Show symlink status
schemulator backup  [emulator...]         # Zip configs (auto-rotates, keeps 5)
schemulator capture <emulator> <version>  # Snapshot config as immutable original
schemulator originals <emulator>          # List captured originals
schemulator revert <emulator> [version]   # Restore from original (auto-backs up)
schemulator migrate <source> <target>     # Copy configs between emulators
```

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
