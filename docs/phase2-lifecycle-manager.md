# Phase 2: Emulator Lifecycle Manager

Expand `setup.py` into a CLI that can install, update, migrate, and backup emulators across Windows, macOS, and Linux.

## Core Principle: Read-Only Source, Symlink Outward

The project directory is the **source of truth** and is effectively read-only from the program's perspective:

- `setup.py` **never modifies** files inside the project directory. It only reads manifests and creates symlinks on the host that point into this directory.
- Emulators write their configs/saves through the symlinks back into the project directory — that's expected. But the tool itself only creates symlinks, never copies or edits config files.
- When an emulator is installed, its **pristine default config** is captured as an immutable snapshot (an "original"). Originals are append-only — they can never be deleted, only added to.
- The user can browse and revert to any previous original at any time.

### Data Flow

```
Project Dir (cloud-synced)              Host Filesystem
================================        ================================
emulators/Dolphin/config/        <----  ~/Library/Application Support/dolphin-emu/
                  (symlink points here)

emulators/Dolphin/originals/
  2603a/config/                         (immutable snapshot, never touched)
  2412/config/                          (immutable snapshot, never touched)
```

1. `setup.py install Dolphin` → downloads binary, extracts to `${bin}`, captures default config as `originals/<version>/`
2. `setup.py symlink Dolphin` → creates symlinks from host config paths into `emulators/Dolphin/config/`
3. User plays games → emulator writes through symlinks into `emulators/Dolphin/config/`
4. `setup.py update Dolphin` → downloads new version, captures new defaults as `originals/<new_version>/`, replaces binary. User's active config is untouched.
5. `setup.py revert Dolphin 2412` → copies `originals/2412/config/` over the active config dir (after backing up current)

### Originals: Immutable Version History

```
emulators/Dolphin/originals/
├── 2412/                    # captured when v2412 was first installed
│   ├── config/
│   └── data/
├── 2603a/                   # captured when v2603a was installed
│   ├── config/
│   └── data/
└── manifest.json            # tracks which version was captured when
```

```json
// originals/manifest.json
[
  { "version": "2412", "captured": "2026-03-15T10:30:00Z" },
  { "version": "2603a", "captured": "2026-04-01T14:00:00Z" }
]
```

Rules:
- **Append-only**: new originals are added, never removed or overwritten
- **Immutable**: files inside an originals directory are set read-only (`chmod -R a-w`)
- **No delete command**: there is no CLI or GUI action to delete an original
- **Revert = copy**: reverting copies files from an original over the active config (after auto-backup)
- **"Update originals"**: re-captures defaults from the currently installed version as a new entry — does not modify existing entries

## CLI Interface

```
python setup.py install [emulator...]         # Download + install latest
python setup.py update  [emulator...]         # Upgrade to latest release
python setup.py symlink [emulator...]         # Wire configs into host (current behavior)
python setup.py backup  [emulator...]         # Snapshot configs to backups/
python setup.py migrate <from> <to>           # Convert configs between emulators
python setup.py status                        # Show installed vs latest versions
python setup.py revert  <emulator> [version]  # Revert config to an original
python setup.py originals <emulator>          # List available originals
python setup.py gui                           # Launch GUI (Phase 3)
```

Omitting `[emulator...]` operates on all emulators.

## Config Redesign

### `config.json` (top-level, user-edited)

Replaces `setup.json`. Adds macOS and binary install location per platform.

```json
{
  "platforms": {
    "windows": {
      "host": "~/AppData/Roaming/",
      "portable": "~/Documents/ES-DE/",
      "bin": "~/Documents/ES-DE/Emulators/"
    },
    "macos": {
      "host": "~/Library/Application Support/",
      "portable": "~/Library/Application Support/ES-DE/",
      "bin": "/Applications/"
    },
    "linux": {
      "host": "~/.var/app/",
      "portable": "~/ES-DE/",
      "bin": "~/.local/bin/"
    }
  }
}
```

### Platform Detection

Replace `os.name` check with `sys.platform`:

```python
PLATFORM = {"win32": "windows", "darwin": "macos", "linux": "linux"}[sys.platform]
```

### `<emulator>/emulator.json` (per-emulator manifest)

Merges current `symlinks.json` with release source and install instructions.

```json
{
  "name": "Azahar",
  "source": {
    "type": "github",
    "repo": "azahar-emu/azahar",
    "asset_patterns": {
      "windows": "azahar-windows-msvc-*-.zip",
      "macos": "azahar-macos-universal-*.zip",
      "linux": "azahar.AppImage"
    }
  },
  "install": {
    "windows": { "method": "extract_zip", "dest": "${bin}/Azahar/" },
    "macos": { "method": "extract_zip", "dest": "${bin}" },
    "linux": { "method": "appimage", "dest": "${bin}/Azahar/" }
  },
  "symlinks": {
    "data": {
      "windows": "${portable}/Emulators/Azahar/user/",
      "macos": "${host}/azahar-emu/",
      "linux": "${host}/azahar-emu/"
    }
  },
  "originals_capture": ["config", "data/sysdata"],
  "migrates_from": {
    "Lime3DS": {
      "copy": ["data/sdmc", "data/nand", "data/sysdata", "data/cheats"],
      "ignore": ["data/screenshots", "data/states"]
    }
  }
}
```

The `originals_capture` field declares which directories to snapshot as originals when installing/updating. This avoids snapshotting large data dirs (saves, ROMs) that aren't part of the "default config."

## Release Sources

Each emulator has a different distribution model. The release resolver must handle all of them:

| Emulator | Source Type | API Endpoint | Notes |
|---|---|---|---|
| Azahar | GitHub releases | `api.github.com/repos/azahar-emu/azahar/releases/latest` | All 3 platforms |
| Dolphin | Direct URL | `dl.dolphin-emu.org/releases/{ver}/...` | No GitHub releases; version from website |
| PCSX2 | GitHub releases | `api.github.com/repos/PCSX2/pcsx2/releases/latest` | |
| Cemu | GitHub releases | `api.github.com/repos/cemu-project/Cemu/releases/latest` | |
| RetroArch | Buildbot | `buildbot.libretro.com/stable/{ver}/{platform}/...` | GitHub only has source tarballs |
| Ryujinx | Self-hosted GitLab | `git.ryujinx.app/api/v4/projects/1/releases` | Ryubing fork at git.ryujinx.app |
| ES-DE | GitLab.com | `gitlab.com/api/v4/projects/es-de%2Femulationstation-de/releases` | |

### Verified Asset Patterns (from actual releases)

```
Azahar (2125.0.1):
  windows: azahar-windows-msvc-2125.0.1.zip (also -installer.exe, -msys2 variants)
  macos:   azahar-macos-universal-2125.0.1.zip (also arm64, x86_64 specific)
  linux:   azahar.AppImage (also azahar-wayland.AppImage)

Dolphin (2603a):
  windows: dolphin-2603a-x64.7z, dolphin-2603a-ARM64.7z
  macos:   dolphin-2603a-universal.dmg
  linux:   Flatpak

PCSX2 (v2.6.3):
  windows: pcsx2-v2.6.3-windows-x64-Qt.7z, PCSX2-v2.6.3-windows-x64-installer.exe
  macos:   pcsx2-v2.6.3-macos-Qt.tar.xz
  linux:   pcsx2-v2.6.3-linux-appimage-x64-Qt.AppImage

Cemu (v2.6):
  windows: cemu-2.6-windows-x64.zip
  macos:   cemu-2.6-macos-12-x64.dmg
  linux:   Cemu-2.6-x86_64.AppImage

RetroArch (v1.22.2):
  windows: buildbot.libretro.com/stable/1.22.2/windows/x86_64/RetroArch.7z
  macos:   buildbot.libretro.com/stable/1.22.2/apple/osx/universal/RetroArch_Metal.dmg
  linux:   Flatpak or buildbot

Ryujinx (1.3.3) — Ryubing fork:
  windows: ryujinx-1.3.3-win_x64.zip
  macos:   ryujinx-1.3.3-macos_universal.app.tar.gz
  linux:   ryujinx-1.3.3-linux_x64.tar.gz, ryujinx-1.3.3-linux_arm64.tar.gz

ES-DE (v3.4.0):
  windows: ES-DE_3.4.0-x64.exe, ES-DE_3.4.0-x64_Portable.zip
  macos:   ES-DE_3.4.0-x64.dmg, ES-DE_3.4.0-arm64.dmg
  linux:   ES-DE_x64.AppImage
```

## Source Types

The release resolver needs these backends:

| Type | How it works |
|---|---|
| `github` | `GET /repos/{owner}/{repo}/releases/latest` -> match `asset_patterns` against asset names |
| `gitlab` | `GET /projects/{id}/releases` (first entry) -> match against `assets.links[].name` |
| `gitlab_custom` | Same as `gitlab` but with a custom base URL (for `git.ryujinx.app`) |
| `buildbot` | Construct URL from version + platform. Check latest version by fetching buildbot index. |
| `direct_url` | URL pattern with `{version}` placeholder. Version fetched from a separate check URL or API. |

## Install Methods

| Method | Platforms | How |
|---|---|---|
| `extract_zip` | All | Unzip to dest |
| `extract_tar` | All | Untar (handles `.tar.gz`, `.tar.xz`) to dest |
| `extract_7z` | All | Requires `7z` binary; Dolphin and PCSX2 use this on Windows |
| `dmg` | macOS | `hdiutil attach`, copy `.app` to dest, `hdiutil detach` |
| `appimage` | Linux | Download to dest, `chmod +x` |
| `flatpak` | Linux | `flatpak install -y <id>` + filesystem overrides |
| `exe_installer` | Windows | Run with silent flags (`/S` or `/VERYSILENT`) |

## Version Tracking

```json
{
  "Azahar": { "version": "2125.0.1", "installed": "2026-04-01" },
  "Dolphin": { "version": "2603a", "installed": "2026-03-15" },
  "PCSX2": { "version": "v2.6.3", "installed": "2026-03-20" },
  "Cemu": { "version": "v2.6", "installed": "2026-03-10" },
  "RetroArch": { "version": "v1.22.2", "installed": "2026-03-01" },
  "Ryujinx": { "version": "1.3.3", "installed": "2026-04-01" },
  "ES-DE": { "version": "v3.4.0", "installed": "2026-04-01" }
}
```

Auto-generated, gitignored.

## Backup Strategy

```
python setup.py backup
```

- Zips each emulator's config/data dirs (excludes ROMs and downloaded_media)
- Saves to `backups/<platform>-<timestamp>.zip`
- Keeps last N backups (configurable in `config.json`, default 3)
- Runs automatically before `update`, `migrate`, and `revert`

## Migration: Lime3DS -> Azahar

Azahar is a merge of Citra forks (PabloMK7's Citra + Lime3DS), so:

- **Saves** (`sdmc/`, `nand/`): direct copy, same format
- **System data** (`sysdata/`): direct copy
- **Config files**: mostly compatible INI format; may need key renames
- **Cheats**: same format, direct copy
- **ROMs**: must be **decrypted first** (see Phase 0). Azahar rejects encrypted ROMs by policy. Phase 0's `decrypt3ds.py` handles this as a batch operation before migration.

The migrate command:
1. Auto-backs up both emulators' configs
2. Copies declared paths from source to dest
3. Reports what was migrated

## Project Structure

```
.
├── config.json                  # user-edited platform paths (replaces setup.json)
├── setup.py                     # CLI entry point
├── core/                        # shared library (used by CLI and GUI)
│   ├── __init__.py
│   ├── config.py                # load config.json, platform detection
│   ├── resolve.py               # GitHub/GitLab/buildbot release resolution
│   ├── install.py               # download + extract/mount/flatpak
│   ├── symlink.py               # current symlink logic, extracted
│   ├── backup.py                # zip config dirs with rotation
│   ├── migrate.py               # cross-emulator config migration
│   ├── originals.py             # capture, list, revert originals
│   └── version.py               # versions.json read/write
├── emulators/                   # one dir per emulator (replaces top-level dirs)
│   ├── Azahar/
│   │   ├── emulator.json
│   │   ├── config/              # active config (symlinked outward, written by emulator)
│   │   ├── data/
│   │   └── originals/           # immutable, append-only version history
│   │       ├── manifest.json
│   │       └── 2125.0.1/
│   ├── Dolphin/
│   │   ├── emulator.json
│   │   ├── config/
│   │   ├── data/
│   │   └── originals/
│   │       ├── manifest.json
│   │       ├── 2412/
│   │       └── 2603a/
│   ├── ...
│   └── ES-DE/
│       ├── emulator.json
│       └── ES-DE/
├── versions.json                # auto-generated, gitignored
├── backups/
├── docs/
├── gui/                         # Phase 3
└── test/                        # Phase 1
```

## Implementation Order

1. **Platform detection** — replace `os.name` check with `sys.platform` mapping
2. **Refactor `setup.py`** into CLI with subcommands (argparse) + `core/` library
3. **Rename `setup.json` -> `config.json`**, add macOS paths and `bin` field
4. **Define `emulator.json` schema**, convert existing `symlinks.json` files
5. **Originals system** — capture, list, revert with immutable append-only storage
6. **Release resolver module** — GitHub, GitLab, custom GitLab, buildbot, direct URL
7. **Download + install module** — per-method installers (zip, tar, 7z, dmg, appimage, flatpak)
8. **`versions.json` tracking** — record installed versions
9. **`status` command** — compare local vs remote versions
10. **`update` command** — auto-backup, capture new originals, then re-install
11. **`backup` command** — zip configs with rotation, exclude ROMs/media
12. **`revert` command** — list originals, restore selected version
13. **`migrate` command** — starting with Lime3DS -> Azahar
14. **Replace Lime3DS dir with Azahar dir** — new manifest, updated symlinks
