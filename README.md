# Semu

Semu is a Steam Deck-first emulation environment manager. Its goal is
RetroDeck-like plug-and-play behavior with a smaller, declarative core:
systems, launchers, controller defaults, keymaps, ROM paths, BIOS checks,
Syncthing integration, screenshot hooks, and packaging metadata all flow from
the BTRC runtime in `src/semu.btrc` and `src/semu/`.

The BTRC source tree is the runtime source of truth for setup, install,
reconfigure, sync, screenshot, keymap, launcher, lifecycle, test, and utility
behavior. Generated artifacts and packaging inputs are derived from that source.

## Current Status

This repo currently provides:

- A compiled BTRC CLI: `build/semu`.
- A generated runtime/UI manifest: `semu.json`.
- A generated C snapshot used by Nix builds: `generated/semu.c`.
- Steam Deck controller defaults with gyro disabled, right trackpad as mouse,
  left trackpad for radial hotkeys, and unified save/load/quit/menu actions.
- A custom keymap language in `input/keymaps/steam_deck.skm` with tokenizer, parser,
  code generators, and authoring errors.
- Declarative Syncthing config in `sync/sync.json`.
- Declarative screenshot verification config in `tests/verification/screenshots.json`.
- Linux launcher shims and AppRun glue under `packaging/linux/`.
- Nix packages for the BTRC CLI, bundled emulator set, routed emulator wrappers,
  and a NixOS module.
- Automated smoke coverage for bootstrap, lifecycle, launcher routing,
  screenshot hooks, sync setup, AppImage assembly wiring, and Nix routed wrappers.

Important remaining production gaps are tracked in `tests/E2E.md`. The biggest
ones are the physical Steam Deck Game Mode pass, a real SteamOS/AppImage pass
with actual emulator binaries, and a true two-device Syncthing conflict test.

## Design Principles

- Steam Deck first, Linux out of the box second, macOS only where already useful.
- Declarative user-editable inputs over hidden procedural state.
- BTRC runtime logic only for install, bootstrap, doctor, keymaps, sync,
  screenshots, lifecycle, sandboxing, and launcher routing.
- Nix for reproducible emulator builds and AppImage payload assembly.
- Routed wrappers place Linux emulator state under
  `.semu/appimage-state/<emulator>` via `HOME` and `XDG_*`.
- Small abstractions: controller model, backend, emitted input, emulator keymap.
- KISS over framework layering. Generated files are allowed; BTRC remains the
  single runtime source of truth.

## Repository Layout

| Path | Purpose |
|---|---|
| `src/semu.btrc` | Small BTRC entrypoint and command dispatcher. |
| `src/semu/` | BTRC runtime modules grouped by manifest, input, emulators, sync, verification, utilities, and tests. |
| `semu.json` | Generated JSON manifest for UI/editor/runtime consumers. |
| `generated/semu.c` | Generated C snapshot compiled by Nix packages. |
| `emulators/profiles/` | Curated emulator profile defaults and user-owned per-emulator config targets. |
| `emulators/es-de/custom_systems/` | Generated ES-DE systems catalog snapshot. |
| `input/keymaps/steam_deck.skm` | Editable Steam Deck keymap source. |
| `input/steam-input/*.vdf` | Generated Steam Input template files. |
| `sync/sync.json` | Editable Syncthing policy. |
| `tests/verification/screenshots.json` | Editable screenshot hook policy. |
| `packaging/linux/AppRun` | AppImage entry point and bundled Nix-store mount wrapper. |
| `packaging/linux/bin/semu-*` | Thin Linux launcher shims. |
| `packaging/linux/ES-DE/*.xml` | Linux ES-DE systems/find-rules assets. |
| `packaging/nix/*.nix` | Emulator packaging, routed wrappers, CLI package, NixOS module. |
| `packaging/nix/flake/*.nix` | Flake package, app, check, and dev-shell modules. |
| `mk/build.mk` | Build/bootstrap Make targets. |
| `tests/Makefile` | Fast test, verification, and E2E graph targets. |
| `tests/vm/` | Arch VM harness and cloud-init fixtures. |
| `tests/bazzite/` | Bazzite VM harness. |
| `tests/steam-deck/` | Physical Deck SSH smoke helpers. |
| `ES-DE/ES-DE/` | User content root for ROMs, BIOS, saves, states, media, themes. |
| `.semu/` | Runtime state created by launcher and AppImage routes. |

Edit the BTRC sources under `src/`, then regenerate `semu.json` and
`generated/semu.c`.

## Steam Deck Quick Start

For a first-run Deck setup from Desktop Mode, run the bootstrap script:

```sh
curl -fsSL https://raw.githubusercontent.com/schiffy91/semu/main/utils/steam-deck-bootstrap.sh \
  | bash -s -- install --yes
```

That script handles the shell-only host setup: persistent SteamOS `/nix`,
official multi-user Nix if needed, repo clone/update, dev-shell/tooling
realization, flake build, and SSH enablement. Product setup then delegates to
the BTRC CLI (`deck install`, Steam Input, keymap, screenshot, sync, and
doctor). For a microSD ROM directory:

```sh
curl -fsSL https://raw.githubusercontent.com/schiffy91/semu/main/utils/steam-deck-bootstrap.sh \
  | bash -s -- install --yes --roms /run/media/mmcblk0p1/Emulation/ROMs
```

After the first install, update the Deck with:

```sh
~/semu/utils/steam-deck-bootstrap.sh update --yes
```

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
- Seeds `input/keymaps/steam_deck.skm` if missing.
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
shims, bundled Syncthing/curl, and optionally a copied Nix closure for routed
emulator wrappers. ROMs and BIOS stay in user-owned project folders.

```sh
nix build .#default
packaging/linux/build-appimage.sh \
  --nix-package result \
  --esde-appimage ./ES-DE.AppImage \
  --output ./Semu-x86_64.AppImage \
  --arch x86_64
```

Why bubblewrap is used:

- Nix-built binaries reference absolute `/nix/store/...` paths.
- AppRun preserves those paths by mounting a bundled Nix store payload at the
  expected `/nix/store` location.
- `packaging/linux/AppRun` detects a bundled `nix/store` payload and re-execs
  through bubblewrap with that payload mounted read-only at `/nix/store`.

Fallback behavior:

- If a routed Nix binary exists inside the AppImage, ES-DE find rules point at it.
- Flatpak-backed launchers are the host fallback for supported standalone
  emulators when the AppImage is built without a routed Nix payload.
- `sync setup` from the AppImage installs user systemd units whose Syncthing
  daemon starts through the stable AppImage path, not the temporary AppImage
  mount path.

Current automated tests validate AppImage assembly logic with fake ES-DE and
fake appimagetool. A real SteamOS/Game Mode AppImage pass is still listed in
`tests/E2E.md`.

## Declarative Configuration

### `src/semu.btrc` And `src/semu/`

This is the canonical BTRC runtime tree. It defines:

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
treated as a build artifact even though it is committed for tooling that reads
the manifest directly.

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

### `input/keymaps/steam_deck.skm`

Editable keymap source:

```text
action state.save = Ctrl+S
action state.load = Ctrl+A
action app.quit = Ctrl+Q

bind HKB + R1 -> ${state.save}
bind HKB + L1 -> ${state.load}
bind HKB + Start -> ${app.quit}
```

Steam Input renders the quit action as `Alt+F4` so the left-trackpad radial
and `HKB + Start` close standalone emulator windows consistently in Game Mode.
Emulator-native keymap renderers keep `Ctrl+Q` where that is the supported
in-emulator quit binding.

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

It starts `semu-syncthing.service`, waits for the local API, and adds the
declared folders. When setup is run from the AppImage, the service runs
`sync daemon` through the AppImage so bundled Syncthing is available after the
initial AppImage process exits.

### `tests/verification/screenshots.json`

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
toggle them while launcher code stays stable.

## Controller Model

The input abstraction has four layers:

1. `controller_model`
2. `emulation_backend`
3. `emitted_input`
4. `emulator_keymap`

Declared controller models:

| ID | Layout | Preferred Backend | Gyro Policy |
|---|---|---|---|
| `steam_deck` | Xbox-like Deck layout | `inputplumber` | opt-in |
| `steam_controller` | Steam Controller | `inputplumber` | opt-in |
| `xbox_xinput` | Xbox/XInput | `uinput` | hardware absent |
| `dualshock4` | PlayStation | `uinput` | opt-in |
| `dualsense` | PlayStation | `uinput` | opt-in |
| `switch_pro` | Nintendo | `uinput` | opt-in |

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
- Steam Input quit is `Alt+F4`; emulator-native quit remains `Ctrl+Q` where
  supported.
- Unified load state is `Ctrl+A`.
- Unified save state is `Ctrl+S`.

Default hotkeys:

| Combo | Action | Keyboard Command |
|---|---|---|
| `HKB + A` | pause/resume | `Ctrl+P` |
| `HKB + B` | screenshot | `Ctrl+X` |
| `HKB + X` | fullscreen | `Ctrl+Enter` |
| `HKB + Y` | menu | `Ctrl+M` |
| `HKB + Start` | quit emulator | `Alt+F4` in Steam Input |
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

Supported extensions are declared per system in `src/semu/manifest/` and emitted
into `semu.json` and ES-DE systems XML.

## BIOS And Firmware

Semu expects BIOS, firmware, keys, ROMs, and scraped media to live in
user-owned project folders. Place those files in the declared locations and run
`doctor`.

| ID | System | Required | Files | Target |
|---|---|---:|---|---|
| `psx` | PlayStation | yes | `scph5500.bin`, `scph5501.bin`, `scph5502.bin` | `ES-DE/ES-DE/bios` |
| `ps2` | PlayStation 2 | yes, any one | `ps2-0230a-20080220.bin`, `ps2-0230e-20080220.bin`, `ps2-0230j-20080220.bin` | `ES-DE/ES-DE/bios/ps2` |
| `switch_keys` | Switch | yes | `prod.keys`, `title.keys` | `ES-DE/ES-DE/bios/switch` |
| `wiiu_keys` | Wii U | yes | `keys.txt` | `emulators/profiles/Cemu/data` |
| `dreamcast` | Dreamcast | optional | `dc_boot.bin`, `dc_flash.bin` | `ES-DE/ES-DE/bios/dc` |

3DS ROM preflight is built into `doctor`. It detects:

- `OK`: NoCrypto flag is already set.
- `NEEDS_FIX`: content looks decrypted but NoCrypto flag is missing.
- `ENCRYPTED`: content is encrypted and should be redumped/decrypted.
- `INVALID`: missing or malformed NCSD/NCCH headers.

The BTRC utility in `src/semu/utils/n3ds_nocrypto.btrc` fixes the NoCrypto flag
on already-decrypted `.3ds`/`.cci` files.

```sh
build/semu utils n3ds-nocrypto ROMs/n3ds --check
build/semu utils n3ds-nocrypto ROMs/n3ds -o ROMs/n3ds-fixed
```

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
nix develop
make btrc-build
build/semu manifest --output semu.json
build/semu screenshot setup --project "$PWD"
```

Run fast tests:

```sh
make test
make tests TESTNAME=launcher
make nix-e2e
```

Run the full local verification:

```sh
make verify
```

### BTRC Compiler Dependency

Normal Nix builds compile the committed `generated/semu.c` snapshot. The dev
shell exposes the pinned BTRC compiler as `btrcpy` and `SEMU_BTRCPY`, so source
regeneration does not depend on a local Mac checkout.

```make
BTRC_FLAKE ?=
BTRCPY ?= btrcpy
```

That keeps the production package tied to committed generated C while BTRC
development remains explicit. To test unpublished BTRC compiler changes,
override the flake input:

```sh
make btrc-build BTRC_FLAKE=path:/absolute/path/to/btrc
```

The current dependency model keeps BTRC in the flake input and keeps Semu
focused on generated runtime artifacts. Long-term options:

- keep `BTRC_FLAKE` as the normal local development override;
- keep `inputs.btrc` pinned to a known-good BTRC commit;
- add a `vendor/btrc` submodule only for offline compiler development inside
  this repo.

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
- BTRC 3DS NoCrypto utility smoke.
- AppImage smoke wiring.
- Nix routed wrapper smoke.
- BTRC runtime source guard for Linux/Nix runtime paths.
- Generated-C smoke tests.
- `git diff --check`.

## Runtime Ownership

Install, setup, reconfigure, sync, screenshot, keymap, launcher, lifecycle,
tests, and utility behavior belongs in the BTRC source tree under `src/`.
Packaging, generated manifests, generated C, Steam Input templates, and tests
consume that BTRC-owned runtime model.

## Known Gaps

See `tests/E2E.md` for the active verification matrix. The short version:

- Physical Steam Deck Game Mode validation.
- Real AppImage execution on SteamOS with actual ES-DE and emulator binaries.
- Installed Bazzite VM verification.
- Two-device Syncthing conflict/resolution testing.
- UI editor for `input/keymaps/steam_deck.skm`, `sync/sync.json`, and
  `tests/verification/screenshots.json`.
