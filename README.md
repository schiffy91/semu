# Semu

Semu is a Steam Deck-first emulation environment manager. Its goal is
RetroDeck-like plug-and-play behavior with a smaller, declarative core:
systems, launchers, controller defaults, keymaps, ROM paths, BIOS checks,
Syncthing integration, screenshot hooks, and packaging metadata all flow from
the BTRC runtime in `semu.btrc`.

The runtime source of truth is BTRC, not Python and not per-emulator symlink
manifests. `setup.py`, `setup.json`, `symlinks.json`, and the old Python
find-rules generator have been removed. Python remains only for the test
harness and the standalone `decrypt3ds.py` NoCrypto utility.

## Current Status

This repo currently provides:

- A compiled BTRC CLI: `build/semu`.
- A generated runtime/UI manifest: `semu.json`.
- A generated C snapshot used by Nix builds: `generated/semu.c`.
- Steam Deck controller defaults with gyro disabled, right trackpad as mouse,
  left trackpad for radial hotkeys, and unified save/load/quit/menu actions.
- A custom keymap language in `keymaps/steam_deck.skm` with tokenizer, parser,
  code generators, and authoring errors.
- Declarative Syncthing config in `sync/sync.json`.
- Declarative screenshot verification config in `verification/screenshots.json`.
- Linux launcher shims and AppRun glue under `linux/`.
- Nix packages for the BTRC CLI, bundled emulator set, routed emulator wrappers,
  and a NixOS module.
- Automated smoke coverage for bootstrap, lifecycle, launcher routing,
  screenshot hooks, sync setup, AppImage assembly wiring, and Nix routed wrappers.

Important remaining production gaps are tracked in `test/E2E.md`. The biggest
ones are the physical Steam Deck Game Mode pass, a real SteamOS/AppImage pass
with actual emulator binaries, and a true two-device Syncthing conflict test.

## Design Principles

- Steam Deck first, Linux out of the box second, macOS only where already useful.
- Declarative user-editable inputs over hidden procedural state.
- BTRC runtime logic only for install, bootstrap, doctor, keymaps, sync,
  screenshots, lifecycle, sandboxing, and launcher routing.
- Nix for reproducible emulator builds and AppImage payload assembly.
- No host symlink farm for Linux routed emulators. Routed wrappers place emulator
  state under `.semu/appimage-state/<emulator>` via `HOME` and `XDG_*`.
- Small abstractions: controller model, backend, emitted input, emulator keymap.
- KISS over framework layering. Generated files are allowed, duplicate sources
  of truth are not.

## Repository Layout

| Path | Purpose |
|---|---|
| `semu.btrc` | Canonical BTRC runtime and manifest generator. |
| `semu.json` | Generated JSON manifest for UI/editor/runtime consumers. |
| `generated/semu.c` | Generated C snapshot compiled by Nix packages. |
| `keymaps/steam_deck.skm` | Editable Steam Deck keymap source. |
| `sync/sync.json` | Editable Syncthing policy. |
| `verification/screenshots.json` | Editable screenshot hook policy. |
| `linux/AppRun` | AppImage entry point and bundled Nix-store mount wrapper. |
| `linux/bin/semu-*` | Thin Linux launcher shims. |
| `linux/ES-DE/*.xml` | Linux ES-DE systems/find-rules assets. |
| `nix/*.nix` | Emulator packaging, routed wrappers, CLI package, NixOS module. |
| `steam-input/*.vdf` | Generated Steam Input template files. |
| `test/` | Local, VM, AppImage, Deck-style, and regression tests. |
| `ES-DE/ES-DE/` | User content root for ROMs, BIOS, saves, states, media, themes. |
| `.semu/` | Runtime state created by launcher and AppImage routes. |

Do not hand-edit `semu.json` or `generated/semu.c`. Edit
`semu.btrc`, then regenerate.

## Steam Deck Quick Start

From the project root:

```sh
make btrc-build
build/semu deck install --project "$PWD" --roms "$PWD/ES-DE/ES-DE/ROMs"
build/semu doctor --project "$PWD"
build/semu keymap validate --project "$PWD"
build/semu screenshot status --project "$PWD"
```

For a microSD ROM directory, pass the real mount path:

```sh
build/semu deck install \
  --project "$PWD" \
  --roms "/run/media/mmcblk0p1/Emulation/ROMs"
```

What `deck install` does:

- Writes `sync/sync.json` if missing and creates sync state directories.
- Creates the ES-DE content tree.
- Creates ROM directories for every declared system.
- Creates BIOS target directories.
- Seeds Steam Deck controller defaults.
- Seeds `keymaps/steam_deck.skm` if missing.
- Renders Steam Input templates.
- Renders ES-DE systems and find rules.
- Renders `semu.json`.
- Installs desktop and systemd user sync helpers where applicable.

After install, add your legally dumped BIOS/firmware files and ROMs, then run:

```sh
build/semu doctor --project "$PWD"
```

The doctor reports missing BIOS, controller defaults, keymap validity,
Steam Input templates, 3DS ROM NoCrypto status, screenshot hooks, sync status,
and Linux launchers.

## Linux Desktop Quick Start

Build the bundle:

```sh
nix build .#default
```

Run the BTRC CLI from the bundle:

```sh
result/bin/semu doctor --project "$PWD"
result/bin/semu deck install --project "$PWD" --roms "$PWD/ES-DE/ES-DE/ROMs"
```

Run routed emulator wrappers directly:

```sh
nix run .#semu-retroarch -- --help
nix run .#semu-dolphin -- --help
nix run .#semu-pcsx2 -- --help
```

Each routed wrapper sets:

- `SEMU_PROJECT_DIR`
- `SEMU_ROMS_DIR`
- `HOME`
- `XDG_CONFIG_HOME`
- `XDG_DATA_HOME`
- `XDG_CACHE_HOME`

The wrapper state lives under:

```text
<project>/.semu/appimage-state/<emulator>/
```

That state path is included in the Syncthing folder declarations as
`emulator_state`.

## AppImage Build

The AppImage path wraps ES-DE, AppRun, the compiled BTRC CLI, Linux launcher
shims, and optionally a copied Nix closure for routed emulator wrappers.
ROMs and BIOS are never bundled.

```sh
nix build .#default
linux/build-appimage.sh \
  --nix-package result \
  --esde-appimage ./ES-DE.AppImage \
  --output ./Semu-x86_64.AppImage \
  --arch x86_64
```

Why bubblewrap is used:

- Nix-built binaries reference absolute `/nix/store/...` paths.
- An AppImage cannot rewrite those paths safely.
- `linux/AppRun` detects a bundled `nix/store` payload and re-execs through
  bubblewrap with that payload mounted read-only at `/nix/store`.

Fallback behavior:

- If a routed Nix binary exists inside the AppImage, ES-DE find rules point at it.
- If no routed Nix payload is bundled, Flatpak-backed launchers remain the host
  fallback for supported standalone emulators.

Current automated tests validate AppImage assembly logic with fake ES-DE and
fake appimagetool. A real SteamOS/Game Mode AppImage pass is still listed in
`test/E2E.md`.

## Declarative Configuration

### `semu.btrc`

This is the canonical runtime file. It defines:

- JSON manifest generation.
- Systems catalog and ES-DE commands.
- BIOS requirements.
- Linux and macOS launcher paths.
- Controller model metadata.
- Steam Deck profile metadata.
- Keymap compiler and renderers.
- Bootstrap and doctor.
- Lifecycle install/reconfigure/change/uninstall/reinstall/upgrade.
- Syncthing setup/status/force commands.
- Screenshot hook setup/status/capture.
- Sandbox and launcher routing.
- BTRC-native E2E smoke tests.

Regenerate artifacts:

```sh
make btrc-build
build/semu manifest --output semu.json
```

### `semu.json`

Generated manifest used by tests, UI work, and external tooling. It should be
treated as a build artifact even though it is committed for consumers that do
not compile BTRC locally.

Top-level sections include:

- `paths`
- `es_de`
- `input`
- `controller_profiles`
- `keymaps`
- `screenshot_verification`
- `sync`
- `bios`
- `launchers`
- `systems`

### `keymaps/steam_deck.skm`

Editable keymap source:

```text
action state.save = Ctrl+S
action state.load = Ctrl+A
action app.quit = Ctrl+Q

bind HKB + R1 -> ${state.save}
bind HKB + L1 -> ${state.load}
bind HKB + Start -> ${app.quit}
```

Validate and render:

```sh
build/semu keymap validate --project "$PWD"
build/semu keymap render --project "$PWD" --target manifest
build/semu keymap render --project "$PWD" --target retroarch
build/semu keymap render --project "$PWD" --target dolphin
build/semu keymap render --project "$PWD" --target pcsx2
build/semu keymap render --project "$PWD" --target steam-input
```

The compiler catches duplicate actions, duplicate controller combos, missing
required actions, unsupported modifiers, missing arrows, unterminated `${...}`
references, and unknown action references.

### `sync/sync.json`

Editable Syncthing policy. Defaults:

| Folder | Enabled | Watch | Rescan |
|---|---:|---:|---:|
| `saves` | yes | yes | 900s |
| `states` | yes | yes | 900s |
| `emulator_state` | yes | yes | 900s |
| `screenshots` | yes | yes | 1800s |
| `gamelists` | yes | yes | 1800s |
| `roms` | no | yes | 3600s |
| `bios` | no | yes | 3600s |

Commands:

```sh
build/semu sync setup --project "$PWD"
build/semu sync status --project "$PWD"
build/semu sync force all --project "$PWD"
build/semu sync force saves --project "$PWD"
build/semu sync tray --project "$PWD"
build/semu sync open --project "$PWD"
```

`sync setup` writes systemd user units:

- `semu-syncthing.service`
- `semu-sync-force.service`
- `semu-sync-force.timer`

It also uses Syncthing's CLI/API when available to add the declared folders.

### `verification/screenshots.json`

Editable screenshot policy. Defaults declare:

- `capture_before_launch`
- `capture_after_spawn`
- `capture_after_exit`
- output pattern:
  `${paths.project_screenshots}/verification/${emulator}/${hook}.png`
- auto tool selection from `grim`, `spectacle`, `gnome-screenshot`, or
  ImageMagick `import`

Commands:

```sh
build/semu screenshot setup --project "$PWD"
build/semu screenshot status --project "$PWD"
SEMU_SCREENSHOT_HOOKS=1 build/semu launcher retroarch --project "$PWD" game.gba
build/semu screenshot capture --project "$PWD" --emulator retroarch --hook manual_visual_checkpoint
```

Screenshot hooks are deliberately declarative so VM/Deck verification can
toggle them without editing launcher code.

## Controller Model

The input abstraction has four layers:

1. `controller_model`
2. `emulation_backend`
3. `emitted_input`
4. `emulator_keymap`

Declared controller models:

| ID | Layout | Preferred Backend | Gyro Policy |
|---|---|---|---|
| `steam_deck` | Xbox-like Deck layout | `inputplumber` | disabled by default |
| `steam_controller` | Steam Controller | `inputplumber` | disabled by default |
| `xbox_xinput` | Xbox/XInput | `uinput` | not available |
| `dualshock4` | PlayStation | `uinput` | disabled by default |
| `dualsense` | PlayStation | `uinput` | disabled by default |
| `switch_pro` | Nintendo | `uinput` | disabled by default |

Declared backends:

| Backend | Purpose | Automated |
|---|---|---:|
| `uinput` | Linux virtual gamepad/keyboard/mouse events | yes |
| `evemu` | Replay recorded evdev events | yes |
| `uhid` | Userspace HID shape testing | yes |
| `inputplumber` | Deck-style routing daemon | yes |
| `steam_input` | Steam Game Mode / Gamescope final pass | visual |

Steam Deck defaults:

- Gyro disabled.
- Right trackpad is mouse.
- Left trackpad is radial hotkeys.
- Hotkey button is `HKB` (`View`, with L4/R4 optional in Steam Input).
- Unified quit is `Ctrl+Q`.
- Unified load state is `Ctrl+A`.
- Unified save state is `Ctrl+S`.

Default hotkeys:

| Combo | Action | Keyboard Command |
|---|---|---|
| `HKB + A` | pause/resume | `Ctrl+P` |
| `HKB + B` | screenshot | `Ctrl+X` |
| `HKB + X` | fullscreen | `Ctrl+Enter` |
| `HKB + Y` | menu | `Ctrl+M` |
| `HKB + Start` | quit emulator | `Ctrl+Q` |
| `HKB + D-Pad Left` | previous state slot | `Ctrl+J` |
| `HKB + D-Pad Right` | next state slot | `Ctrl+K` |
| `HKB + L1` | load state | `Ctrl+A` |
| `HKB + R1` | save state | `Ctrl+S` |
| `HKB + L2` | rewind | `Ctrl+-` |
| `HKB + R2` | fast forward | `Ctrl++` |
| `HKB + L3` | swap screens | `Ctrl+Tab` |
| `HKB + R3` | escape | `Esc` |

## Supported Systems

| System ID | Platform | ROM Directory | Linux Default |
|---|---|---|---|
| `gb` | Game Boy | `gb` | RetroArch Gambatte |
| `gbc` | Game Boy Color | `gbc` | RetroArch Gambatte |
| `gba` | Game Boy Advance | `gba` | RetroArch mGBA |
| `nes` | NES | `nes` | RetroArch Nestopia |
| `snes` | Super Nintendo | `snes` | RetroArch Snes9x |
| `genesis` | Sega Genesis / Mega Drive | `genesis` | RetroArch Genesis Plus GX |
| `n64` | Nintendo 64 | `n64` | Gopher64 |
| `nds` | Nintendo DS | `nds` | melonDS, DeSmuME fallback |
| `dreamcast` | Sega Dreamcast | `dreamcast` | Flycast |
| `psx` | Sony PlayStation | `psx` | RetroArch Beetle PSX |
| `ps2` | Sony PlayStation 2 | `ps2` | PCSX2 |
| `psp` | Sony PSP | `psp` | PPSSPP |
| `n3ds` | Nintendo 3DS | `n3ds` | Azahar |
| `gc` | Nintendo GameCube | `gc` | Dolphin |
| `wii` | Nintendo Wii | `wii` | Dolphin |
| `wiiu` | Nintendo Wii U | `wiiu` | Cemu |
| `switch` | Nintendo Switch | `switch` | Ryujinx |

Supported extensions are declared per system in `semu.btrc` and emitted
into `semu.json` and ES-DE systems XML.

## BIOS And Firmware

Semu does not bundle BIOS, firmware, keys, copyrighted ROMs, or scraped
media. Place user-owned files in the declared locations and run `doctor`.

| ID | System | Required | Files | Target |
|---|---|---:|---|---|
| `psx` | PlayStation | yes | `scph5500.bin`, `scph5501.bin`, `scph5502.bin` | `ES-DE/ES-DE/bios` |
| `ps2` | PlayStation 2 | yes, any one | `ps2-0230a-20080220.bin`, `ps2-0230e-20080220.bin`, `ps2-0230j-20080220.bin` | `ES-DE/ES-DE/bios/ps2` |
| `switch_keys` | Switch | yes | `prod.keys`, `title.keys` | `ES-DE/ES-DE/bios/switch` |
| `wiiu_keys` | Wii U | yes | `keys.txt` | `Cemu/data` |
| `dreamcast` | Dreamcast | optional | `dc_boot.bin`, `dc_flash.bin` | `ES-DE/ES-DE/bios/dc` |

3DS ROM preflight is built into `doctor`. It detects:

- `OK`: NoCrypto flag is already set.
- `NEEDS_FIX`: content looks decrypted but NoCrypto flag is missing.
- `ENCRYPTED`: content does not look decrypted and must be redumped/decrypted.
- `INVALID`: missing or malformed NCSD/NCCH headers.

`decrypt3ds.py` can fix the NoCrypto flag on already-decrypted `.3ds` files.
It does not perform full AES decryption.

## Command Reference

Manifest and bootstrap:

```sh
build/semu manifest --output semu.json
build/semu bootstrap --project "$PWD"
build/semu doctor --project "$PWD"
build/semu deck install --project "$PWD" --roms "$PWD/ES-DE/ES-DE/ROMs"
build/semu deck verify --project "$PWD"
build/semu deck launch --project "$PWD"
```

Lifecycle:

```sh
build/semu lifecycle install --project "$PWD" --roms "$PWD/ES-DE/ES-DE/ROMs"
build/semu lifecycle setup --project "$PWD"
build/semu lifecycle reconfigure --project "$PWD"
build/semu lifecycle change --project "$PWD" --roms "/new/ROMs"
build/semu lifecycle uninstall --project "$PWD"
build/semu lifecycle reinstall --project "$PWD"
build/semu lifecycle upgrade --project "$PWD"
build/semu lifecycle status --project "$PWD"
```

Config:

```sh
build/semu config env --project "$PWD"
build/semu config set-roms --project "$PWD" --roms "/path/to/ROMs"
```

Keymaps:

```sh
build/semu keymap validate --project "$PWD"
build/semu keymap render --project "$PWD" --target manifest
build/semu keymap render --project "$PWD" --target retroarch
build/semu keymap render --project "$PWD" --target dolphin
build/semu keymap render --project "$PWD" --target pcsx2
build/semu keymap render --project "$PWD" --target steam-input
```

Steam Input:

```sh
build/semu steam-input install --project "$PWD"
build/semu steam-input status --project "$PWD"
```

Sync:

```sh
build/semu sync setup --project "$PWD"
build/semu sync start --project "$PWD"
build/semu sync stop --project "$PWD"
build/semu sync status --project "$PWD"
build/semu sync force all --project "$PWD"
build/semu sync autostart-on --project "$PWD"
build/semu sync autostart-off --project "$PWD"
build/semu sync tray --project "$PWD"
build/semu sync open --project "$PWD"
```

Screenshots:

```sh
build/semu screenshot setup --project "$PWD"
build/semu screenshot status --project "$PWD"
build/semu screenshot capture --project "$PWD" --emulator retroarch --hook manual_visual_checkpoint
```

Launchers and sandbox:

```sh
build/semu sandbox prepare --project "$PWD" --emulator retroarch --scratch /tmp/semu-retroarch
build/semu launcher retroarch --project "$PWD" -- -L core.so game.gba
build/semu launcher dolphin --project "$PWD" -- game.iso
build/semu launcher flatpak --project "$PWD" org.ppsspp.PPSSPP game.iso
```

E2E:

```sh
build/semu e2e all
build/semu e2e lifecycle
build/semu e2e launcher
build/semu e2e sandbox
build/semu e2e sync
```

## Nix Outputs

Packages:

```sh
nix build .#default
nix build .#semu-routed-emulators
nix build .#semu-retroarch
nix build .#semu-dolphin
nix build .#semu-ppsspp
nix build .#semu-flycast
nix build .#semu-gopher64
nix build .#semu-melonds
nix build .#semu-pcsx2
nix build .#semu-cemu
nix build .#semu-azahar
nix build .#semu-ryujinx
nix build .#semu-es-de
nix build .#btrcpy
```

Apps:

```sh
nix run .#default -- doctor --project "$PWD"
nix run .#semu-retroarch -- --help
nix run .#semu-dolphin -- --help
```

Checks:

```sh
nix flake check
nix build .#checks.x86_64-linux.routed-emulator-mock
```

The flake supports `aarch64-darwin`, `x86_64-darwin`, `x86_64-linux`, and
`aarch64-linux`, but the routed emulator/AppImage path is Linux-focused.

## NixOS Module

```nix
{
  inputs.semu.url = "github:schiffy91/semu";

  outputs = { self, nixpkgs, semu, ... }: {
    nixosConfigurations.myhost = nixpkgs.lib.nixosSystem {
      modules = [
        semu.nixosModules.default
        {
          programs.semu = {
            enable = true;
            configDir = "/home/deck/Emulation";
          };
        }
      ];
    };
  };
}
```

## Development Workflow

Build and regenerate:

```sh
make btrc-build
build/semu manifest --output semu.json
build/semu screenshot setup --project "$PWD"
```

Run fast tests:

```sh
python3 -m pytest test/ -v
build/semu e2e all
bash test/appimage/smoke.sh
make nix-e2e
```

Run the full local verification:

```sh
make verify
```

### BTRC Compiler Dependency

Normal Nix builds compile `generated/semu.c` and do not need a BTRC
compiler checkout. The flake exposes a local `.#btrcpy` wrapper backed by the
`btrc` input. Development builds use that flake-pinned compiler by default, so
fresh checkouts can regenerate artifacts without an adjacent BTRC checkout.

```make
BTRC_FLAKE ?=
BTRC_USE_FLAKE ?= 1
```

That keeps the production package independent from a live compiler worktree. To
test unpublished BTRC compiler changes, override the flake input explicitly:

```sh
make btrc-build BTRC_FLAKE=path:/absolute/path/to/btrc
```

The legacy local checkout escape hatch is still available:

```sh
make btrc-build BTRC_USE_FLAKE=0 BTRC_ROOT=/absolute/path/to/btrc
```

I would not make BTRC a git submodule right now. A submodule would pin the
compiler source, but it would also add nested git friction to the Steam
Deck/user-facing repo and would not help normal users because they build from
the generated C snapshot. The cleaner long-term options are:

- keep `BTRC_FLAKE` as the normal local development override;
- keep `inputs.btrc` pinned to a known-good BTRC commit;
- only add a submodule under something like `vendor/btrc` if offline compiler
  hacking inside this repo becomes more important than repository simplicity.

Run VM/deck-oriented checks:

```sh
make deck-vm-verify
make deck-vm-verify-strict
make bazzite-vm-smoke
make bazzite-desktop-vm-smoke
```

`make verify` currently covers:

- BTRC build.
- Manifest generation.
- Manifest determinism.
- Keymap validation/rendering.
- Bootstrap/doctor invariants.
- Linux launcher shell syntax.
- BTRC lifecycle/sandbox/launcher/sync E2E smokes.
- AppImage smoke wiring.
- Nix routed wrapper smoke.
- Runtime no-Python guard for Linux/Nix runtime paths.
- Regression tests.
- `git diff --check`.

## Removed Legacy Path

The following old path is gone:

- `setup.py`
- `setup.json`
- per-emulator `symlinks.json`
- `generate_find_rules.py`
- stale `docs/phase*.md`
- tests that only exercised the old Python setup/symlink manager

Do not reintroduce these as runtime dependencies. New install, setup,
reconfigure, sync, screenshot, keymap, launcher, and lifecycle behavior belongs
in `semu.btrc`.

## Known Gaps

See `test/E2E.md` for the active verification matrix. The short version:

- Physical Steam Deck Game Mode validation is still required.
- Real AppImage execution on SteamOS with actual ES-DE and emulator binaries is
  still required.
- Installed Bazzite VM verification is not yet complete.
- Two-device Syncthing conflict/resolution testing is not yet complete.
- A UI editor for `keymaps/steam_deck.skm`, `sync/sync.json`, and
  `verification/screenshots.json` is still future work.
- `decrypt3ds.py` remains a Python utility; port or remove it if the repo must
  become strictly zero-Python outside tests.
