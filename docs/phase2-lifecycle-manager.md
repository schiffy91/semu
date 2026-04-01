# Phase 2: Nix-Based Emulator Bundle

Bundle all emulators deterministically via a Nix flake. `nix run` gives you the full environment. Update emulators by updating the flake.

## Architecture Shift

**Before (old plan):** Download emulator binaries at runtime from GitHub/GitLab/buildbot.
**Now:** All emulators are Nix packages. The flake pins exact versions. No runtime downloading.

This eliminates:
- Release resolver (GitHub/GitLab/buildbot API clients)
- Download + install module (zip/tar/7z/dmg/appimage/flatpak handlers)
- Version tracking (versions.json) — the flake.lock is the version tracker
- Platform-specific install methods

What remains:
- `setup.py` symlink management (already works)
- Config migration (Lime3DS -> Azahar)
- Backup/restore
- Originals versioning
- `config.json` with platform paths

## Nix Flake

```nix
# flake.nix
{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    systems = [ "aarch64-darwin" "x86_64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
  in {
    packages = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      # Emulators from nixpkgs
      dolphin = pkgs.dolphin-emu;
      pcsx2 = pkgs.pcsx2;
      retroarch = pkgs.retroarch-bare;
      cemu = pkgs.cemu;
      azahar = pkgs.azahar;

      # Custom-packaged (not in nixpkgs)
      # ryujinx = ...;
      # es-de = ...;

      # The setup tool itself
      schemulator = pkgs.python3Packages.buildPythonApplication {
        pname = "schemulator";
        version = "0.1.0";
        src = ./.;
        propagatedBuildInputs = [ pkgs.python3Packages.pycryptodome ];
      };
    });
  };
}
```

## Emulator Status in nixpkgs

| Emulator | Package | macOS | Linux | Version |
|---|---|---|---|---|
| Dolphin | `dolphin-emu` | Yes | Yes | 2603a |
| PCSX2 | `pcsx2` | Yes | Yes | 2.6.3 |
| RetroArch | `retroarch-bare` | Yes | Yes | 1.22.2 |
| Cemu | `cemu` | Yes | Yes | 2.6 |
| Azahar | `azahar` | Yes | Yes | 2125.0.1 |
| Ryujinx | **needs packaging** | — | — | 1.3.3 |
| ES-DE | **needs packaging** | — | — | 3.4.0 |

## CLI Interface

```
schemulator symlink [emulator...]         # Wire configs into host paths
schemulator backup  [emulator...]         # Snapshot configs
schemulator revert  <emulator> [version]  # Revert to original config
schemulator originals <emulator>          # List available originals
schemulator status                        # Show bundled emulator versions
```

No `install` or `update` commands — Nix handles that:
- Install: `nix profile install .#dolphin`
- Update: `nix flake update` then rebuild
- Version check: `nix flake metadata`

## Config

### `config.json` (user-edited)

```json
{
  "platforms": {
    "macos": {
      "host": "~/Library/Application Support/",
      "portable": "~/Library/Application Support/ES-DE/"
    },
    "linux": {
      "host": "~/.var/app/",
      "portable": "~/ES-DE/"
    }
  }
}
```

### Platform Detection

```python
PLATFORM = {"darwin": "macos", "linux": "linux"}[sys.platform]
```

## Symlink-Only, Read-Only Source

Same as before:
- `setup.py` only creates symlinks, never modifies project files
- Emulators write through the symlinks
- Originals are immutable, append-only snapshots

## Implementation Order

1. Write `flake.nix` with the 5 nixpkgs emulators
2. Refactor `setup.py`: add `sys.platform` detection, macOS paths, argparse subcommands
3. Rename `setup.json` -> `config.json`, add macOS paths
4. Convert `symlinks.json` files to include macOS paths
5. Test `schemulator symlink` on macOS with Nix-provided emulators
6. Package Ryujinx as a Nix derivation
7. Package ES-DE as a Nix derivation
8. Add `backup` command
9. Add `originals` capture + `revert` command
10. Add `migrate` command (Lime3DS -> Azahar)
