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
- Declarative Semu settings in `settings/semu-settings.json`, with BTRC-owned
  `settings list|get|put|ui|apply` access for ROMs, sync toggles, visual
  policy, and ES-DE settings entry rollout.
- Per-system presentation settings in `settings/presentation/*.json`, with
  BTRC-owned `presentation plan|audit|get|put|defaults` access for shader files,
  bezel files, display layout, scale policy, adapter source, and dynamic aspect
  policy. `presentation plan` reports selected policy, resolved asset files,
  launcher-effective shader choice, and missing/disabled asset status.
  `presentation audit` records required/missing visual assets under
  `.semu/verification` without editing emulator-native config.
- Declarative screenshot verification config in `verification/screenshots.json`.
- Linux launcher shims and AppRun glue under `packaging/linux/`.
- Nix packages for the BTRC CLI, bundled emulator set, routed emulator wrappers,
  SteamOS graphics bridge, and a NixOS module.
- Automated smoke coverage for bootstrap, lifecycle, launcher routing,
  screenshot hooks, sync setup, AppImage assembly wiring, and Nix routed wrappers.

Important remaining production gaps are tracked in `TODO.md` and
`tests/E2E.md`. The SteamOS/AppImage path has a physical Deck Desktop Mode pass
over the routed emulator set with screenshot, input, and structured quit-watch
evidence. The current installed Deck AppImage was rebuilt from commit `d674470`
and smoke-tested at
`4481bb59323b7ba5106dbf2639a53f1a10070036e09b74ee2be22d3d93a581fb`.
The final production gate is still a Game Mode pass that exercises the Steam
Input left-trackpad radial menu and representative games across the routed
emulator set.
The input, shader, bezel, and ES-DE settings rollout is specified in
`docs/input-and-visuals.md`.

## Design Principles

- Steam Deck first, Linux out of the box second, macOS only where already useful.
- Declarative user-editable inputs over hidden procedural state.
- BTRC runtime logic only for install, bootstrap, doctor, keymaps, sync,
  screenshots, lifecycle, sandboxing, and launcher routing.
- Nix for reproducible emulator builds and AppImage payload assembly.
- Nix-built GUI emulators route through the bundled nixGL Mesa bridge on
  SteamOS/non-NixOS hosts so OpenGL/Vulkan drivers resolve cleanly.
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
| `docs/` | Architecture notes and production rollout plans for input, visuals, settings, and verification. |
| `semu.json` | Generated JSON manifest for UI/editor/runtime consumers. |
| `generated/semu.c` | Generated C snapshot compiled by Nix packages. |
| `emulators/profiles/` | Generated Semu-owned emulator profile targets compiled from owned settings/keymaps. |
| `emulators/es-de/custom_systems/` | Generated ES-DE systems catalog snapshot. |
| `input/keymaps/steam_deck.skm` | Editable Steam Deck keymap source. |
| `input/steam-input/*.vdf` | Generated Steam Input template files. |
| `settings/semu-settings.json` | Editable Semu visual/UI policy and settings defaults. |
| `settings/presentation/*.json` | Editable per-system presentation profiles for shaders, bezels, layouts, and emulator adapters. |
| `sync/sync.json` | Editable Syncthing policy. |
| `verification/screenshots.json` | Editable screenshot hook policy. |
| `packaging/linux/AppRun` | AppImage entry point and bundled Nix-store mount wrapper. |
| `packaging/linux/bin/semu-*` | Thin Linux launcher shims. |
| `packaging/linux/ES-DE/*.xml` | Linux ES-DE systems/find-rules assets. |
| `packaging/nix/*.nix` | Emulator packaging, routed wrappers, CLI package, NixOS module. |
| `tests/` | Local, VM, AppImage, Deck-style, and regression tests. |
| `ES-DE/ES-DE/` | User content root for ROMs, BIOS, saves, states, media, themes. |
| `.semu/` | Runtime state created by launcher and AppImage routes. |

Edit the BTRC sources under `src/`, then regenerate `semu.json` and
`generated/semu.c`.

## Configuration Boundary

Semu-owned source files are the only user-editable configuration surface:

- `settings/semu-settings.json`
- `settings/presentation/*.json`
- `input/keymaps/steam_deck.skm`
- `sync/sync.json`
- `verification/screenshots.json`

Everything else that an emulator or ES-DE reads is compiled from those files by
`lifecycle compile`, `lifecycle reconfigure`, `settings compile`,
`settings apply`, or a command that explicitly passes `--apply`. That includes
`emulators/profiles/`, `input/steam-input/*.vdf`,
`emulators/es-de/custom_systems/`, `ES-DE/es_settings.xml`, systemd user units,
desktop entries, and launcher runtime files.

Runtime emulator config under `.semu/appimage-state/<emulator>` is adapter
state. Semu may read it to broadcast normalized presentation state, and launchers
may seed it from generated profiles, but it is not the source of truth. Do not
hand-edit emulator configs to change Semu policy; change the owned JSON/keymap
files and apply. Verification artifacts under `.semu/verification` are also
Semu adapter state; they record launcher evidence such as quit-watch logs, not
emulator policy.

## Steam Deck Quick Start

For a first-run Deck setup from Desktop Mode, run the bootstrap script:

```sh
curl -fsSL https://raw.githubusercontent.com/schiffy91/semu/main/utils/steam-deck-bootstrap.sh \
  | bash -s -- install --yes
```

That script prepares a persistent SteamOS `/nix` bind mount, installs official
multi-user Nix if needed, clones or updates this repo under `~/semu`, builds the
Semu flake, runs Deck install, copies Steam Input templates, starts Syncthing
helpers, and enables SSH for remote verification. For a microSD ROM directory:

```sh
curl -fsSL https://raw.githubusercontent.com/schiffy91/semu/main/utils/steam-deck-bootstrap.sh \
  | bash -s -- install --yes --roms /run/media/deck/SD
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
build/semu deck game-mode-ready --project "$PWD" --prepare
build/semu deck game-mode-evidence --project "$PWD" --prepare
```

For a microSD ROM directory, pass the real mount path:

```sh
build/semu deck install \
  --project "$PWD" \
  --roms "/run/media/deck/SD"
```

`/run/media/deck/SD` is auto-normalized when it contains the common
`Emulation/ES-DE/ES-DE/ROMs` layout.

What `deck install` does:

- Writes `sync/sync.json` if missing and creates sync state directories.
- Writes `settings/semu-settings.json` if missing.
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

For the physical Game Mode pass, launch Semu from Steam, open a representative
game for each routed emulator, use the left-trackpad radial Quit action, then
audit the Semu-owned launcher evidence. Before switching modes, run the
readiness gate so missing Steam shortcut/AppImage/checklist state is caught
without confusing Desktop Mode with Game Mode:

```sh
build/semu deck game-mode-ready --project "$PWD" --prepare
```

After the physical pass, require both Game Mode readiness and complete quit
evidence:

```sh
build/semu deck game-mode-ready --project "$PWD" --require-evidence
build/semu deck game-mode-evidence --project "$PWD"
```

Missing or lifecycle-only logs remain `PENDING`; a passing emulator needs a
quit-watch line with a `reason=` value plus child exit evidence.

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

On SteamOS and other non-NixOS Linux hosts, routed GUI emulators are wrapped
with the bundled `nixGLIntel` Mesa bridge when available. This keeps the
emulators Nix-built while allowing them to use the host GPU stack; RetroArch
uses `glcore` by default for hardware-rendered cores such as N64.

The wrapper state lives under:

```text
<project>/.semu/appimage-state/<emulator>/
```

That state path is included in the Syncthing folder declarations as
`emulator_state`.

## AppImage Build

The AppImage path wraps ES-DE, AppRun, the compiled BTRC CLI, Linux launcher
shims, and optionally a copied Nix closure for routed emulator wrappers.
ROMs and BIOS stay in user-owned project folders.

```sh
nix build .#default
packaging/linux/build-appimage.sh \
  --nix-package result \
  --esde-appimage ./ES-DE.AppImage \
  --output ./Semu-x86_64.AppImage \
  --arch x86_64
```

`--nix-package` may point at the usual `result` symlink; the builder resolves it
before `nix copy`. On SteamOS, the builder defaults temporary AppDir work to
`$HOME/.cache/semu-appimage-work` when `TMPDIR` is unset, because `/tmp` can be
too small for the copied Nix closure.

Why bubblewrap is used:

- Nix-built binaries reference absolute `/nix/store/...` paths.
- AppRun preserves those paths by mounting a bundled Nix store payload at the
  expected `/nix/store` location.
- `packaging/linux/AppRun` detects a bundled `nix/store` payload and re-execs
  through bubblewrap with that payload mounted read-only at `/nix/store`.

Fallback behavior:

- If a routed Nix binary exists inside the AppImage, ES-DE find rules point at it.
- If the routed payload includes `nixGLIntel`, emulator launches use it for
  host GPU driver access on SteamOS/non-NixOS systems.
- Flatpak-backed launchers are the host fallback for supported standalone
  emulators when the AppImage is built without a routed Nix payload.

Automated tests validate AppImage assembly logic with fake ES-DE and fake
appimagetool. Physical Deck evidence currently covers Desktop Mode ES-DE launch,
AppImage-owned Syncthing, SD-card ROM detection, and required-route process
launch/quit automation. Game Mode, physical Steam Input radial quit, controller
input in-game, fullscreen presentation, and ES-DE return flow remain the final
manual gates listed in `tests/E2E.md`.

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

Steam Input renders the quit action as `Select+Start`, `Esc`, `Ctrl+Q`, and
`Alt+F4` so the left-trackpad radial and `HKB + Start` can use emulator-native
quit handling first, then fall back to closing standalone emulator windows in
Game Mode.

Validate and render:

```sh
build/semu keymap validate --project "$PWD"
build/semu keymap capabilities --project "$PWD"
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

### `settings/semu-settings.json`

Editable Semu policy for the settings entrypoint, visual defaults, and settings
CLI/UI. It is intentionally key/value-shaped so ES-DE entries, scripts, and the
terminal UI all use the same `get` and `put` contract.

Default visual policy:

- Native integer scaling is enabled for classic systems.
- CRT shaders and bezels are enabled by policy for classic systems.
- The default modern exclusions are `n3ds`, `wiiu`, and `switch`.
- RetroArch receives native integer-scaling config and the configured shader
  preset path.
- The AppImage bundles `libretro-shaders-slang`; RetroArch shader/bezel
  screenshot proof is tracked in `tests/E2E.md`.

Commands:

```sh
build/semu settings list --project "$PWD"
build/semu settings get roms.dir --project "$PWD"
build/semu settings put roms.dir /run/media/deck/SD --apply --project "$PWD"
build/semu settings put visual.integer_scaling true --project "$PWD"
build/semu settings put visual.bezels true --project "$PWD"
build/semu settings put sync.roms false --project "$PWD"
build/semu settings ui --project "$PWD"
build/semu settings ui presentation --project "$PWD"
build/semu settings ui input --project "$PWD"
build/semu settings ui sync --project "$PWD"
build/semu settings apply --project "$PWD"
```

`roms.dir` uses the same normalization as `deck install`; `/run/media/deck/SD`
resolves to `Emulation/ES-DE/ES-DE/ROMs` when that layout is present.
The terminal UI is dependency-free BTRC output/input: numbered rows edit string
settings, toggle boolean settings, and apply by running lifecycle reconfigure.
The focused presentation, input, and sync UIs edit only Semu-owned source files;
`compile` and `apply` compile those sources into emulator profiles, ES-DE
entries, Steam Input templates, desktop files, and systemd user units.

### `settings/presentation/*.json`

Editable per-system presentation profiles. Each system has its own JSON file
with a shader file, bezel file, runtime preset, hardware-display target, layout,
scale policy, dynamic aspect policy, and emulator adapter source.

Examples:

```sh
build/semu presentation defaults --project "$PWD"
build/semu presentation list --project "$PWD"
build/semu presentation plan --system gb --project "$PWD"
build/semu presentation state --system ps2 --project "$PWD"
build/semu presentation broadcast --system ps2 --project "$PWD"
build/semu presentation get gb shader_file --project "$PWD"
build/semu presentation put gb bezel_file bezels/gb/classic-grey-game-boy.json --project "$PWD"
build/semu presentation put ps2 widescreen_bezel_file bezels/ps2/clean-wide.json --project "$PWD"
```

The defaults encode the current visual target:

- `gb`: DMG green LCD tint, pixel persistence, and classic grey Game Boy shell.
- `gbc`: Game Boy Color LCD behavior and frost purple shell target.
- `gba`: original wide purple GBA target with AGB-001 LCD behavior.
- `nes`, `snes`, `genesis`, `psx`, `n64`: 4:3 CRT station with scanlines,
  analog softness, and Panasonic/Sony-style CRT bezel target.
- `dreamcast`, `gc`, `wii`, `ps2`: dynamic 4:3 or 16:9 TV station driven by
  emulator/game config where available.
- `nds`, `n3ds`: dual-screen LCD station with top/bottom regions and maximized
  handheld bezel target.
- `psp`: 16:9 LCD station with red God of War PSP or original black PSP target.
- `wiiu`, `switch`: modern fullscreen station; no default CRT treatment.

`presentation state` reads known emulator adapter config files and normalizes
the observed display state. `presentation broadcast` writes that normalized
state under `settings/presentation-state/<system>.json` for launchers, settings
entrypoints, and future compositor wrappers. `presentation plan` then exposes
both declared station policy and effective runtime decisions:
`effective_aspect`, `presentation_mode`, `selected_shader_file`,
`selected_bezel_file`, and `selected_runtime_preset`.

Dynamic 4:3/16:9 systems can keep CRT defaults for 4:3 while editing
`widescreen_shader_file`, `widescreen_bezel_file`, and
`widescreen_runtime_preset` for widescreen output. Missing optional art packs
stay visible as empty/unresolved fields instead of silently pretending a preset
exists.

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
- Steam Input quit emits `Select+Start`, `Esc`, `Ctrl+Q`, and `Alt+F4`;
  RetroArch uses native `Start+Select` quit handling.
- `semu-quit-watch` records quit evidence from the launcher layer, so Semu can
  prove the unified quit contract without editing emulator-native config.
  Routed launches write to `.semu/verification/quit-watch/<emulator>.log` by
  default; `deck game-mode-evidence` audits those logs for a `reason=` event,
  and tests can override with `SEMU_QUIT_WATCH_LOG`.
- Unified load state is `Ctrl+A`.
- Unified save state is `Ctrl+S`.

Default hotkeys:

| Combo | Action | Keyboard Command |
|---|---|---|
| `HKB + A` | pause/resume | `Ctrl+P` |
| `HKB + B` | screenshot | `Ctrl+X` |
| `HKB + X` | fullscreen | `Ctrl+Enter` |
| `HKB + Y` | menu | `Ctrl+M` |
| `HKB + Start` | quit emulator | `Select+Start`, `Esc`, `Ctrl+Q`, `Alt+F4` in Steam Input |
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
| `n64` | Nintendo 64 | `n64` | RetroArch Mupen64Plus-Next |
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

On Steam Deck, Semu also detects the common SD-card emulation root derived from
the ROM path, such as `/run/media/deck/SD/Emulation`. Routed emulator state
seeding uses that external root for host-owned files including PCSX2 BIOS,
Cemu `keys.txt`, and Ryujinx `prod.keys`/`title.keys` when present.

3DS ROM preflight is built into `doctor`. It detects:

- `OK`: NoCrypto flag is already set.
- `NEEDS_FIX`: content looks decrypted but NoCrypto flag is missing.
- `ENCRYPTED`: content is encrypted and should be redumped/decrypted.
- `INVALID`: missing or malformed NCSD/NCCH headers.

The BTRC utility in `src/semu/utils/n3ds.btrc` fixes the NoCrypto flag on
already-decrypted `.3ds`/`.cci` files.

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
build/semu config set-roms --project "$PWD" --roms "/path/to/ROMs" --apply
build/semu config show --project "$PWD"
```

`config set-roms` edits only the owned `sync/sync.json` source; pass `--apply`
or run `settings apply` to regenerate emulator and ES-DE files.

Settings:

```sh
build/semu settings list --project "$PWD"
build/semu settings get roms.dir --project "$PWD"
build/semu settings put roms.dir "/path/to/ROMs" --apply --project "$PWD"
build/semu settings put visual.integer_scaling true --project "$PWD"
build/semu settings put visual.bezels true --project "$PWD"
build/semu settings ui --project "$PWD"
build/semu settings apply --project "$PWD"
```

Presentation:

```sh
build/semu presentation plan --system gb --project "$PWD"
build/semu presentation state --system ps2 --project "$PWD"
build/semu presentation broadcast --system ps2 --project "$PWD"
build/semu presentation put ps2 bezel_file "/path/to/sony-crt.slangp" --project "$PWD"
build/semu presentation put ps2 widescreen_bezel_file "/path/to/wide-frame.json" --project "$PWD"
build/semu presentation put wii aspect_policy "4:3_or_16:9" --project "$PWD"
```

Keymaps:

```sh
build/semu keymap validate --project "$PWD"
build/semu keymap ui --project "$PWD"
build/semu keymap capabilities state.save --project "$PWD"
build/semu keymap get state.save --project "$PWD"
build/semu keymap put state.save Ctrl+S --apply --project "$PWD"
build/semu keymap bind app.quit left_trackpad.radial.quit --project "$PWD"
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
build/semu steam-input status --project "$PWD" \
  --shortcuts-file "$HOME/.local/share/Steam/userdata/52445373/config/shortcuts.vdf"
```

`steam-input status` renders the owned templates and parses Steam's binary
`shortcuts.vdf` to report the Semu shortcut appid, derived 64-bit launch id,
and `steam://rungameid/...` URI. It does not edit Steam userdata.

Sync:

```sh
build/semu sync setup --project "$PWD"
build/semu sync ui --project "$PWD"
build/semu sync get sync_saves --project "$PWD"
build/semu sync put sync_saves true --apply --project "$PWD"
build/semu sync start --project "$PWD"
build/semu sync stop --project "$PWD"
build/semu sync status --project "$PWD"
build/semu sync force all --project "$PWD"
build/semu sync autostart enable --project "$PWD"
build/semu sync autostart disable --project "$PWD"
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
make test
make nix-e2e
```

Run the full local verification:

```sh
make verify
```

### BTRC Compiler Dependency

Normal Nix builds compile the committed `generated/semu.c` snapshot. The flake
also exposes `.#btrcpy`, backed by the pinned `btrc` input, so `make
btrc-build` can regenerate artifacts without a machine-local compiler path.

That keeps the production package tied to committed generated C while BTRC
development remains reproducible through the Nix flake/dev shell. Compiler
experiments should update or override the flake input intentionally in a
separate compiler-work branch, not through local Makefile paths.

Run VM/deck-oriented checks:

```sh
make deck-vm-verify
make deck-vm-verify-strict
make bazzite-vm-smoke
make bazzite-desktop-vm-smoke
make e2e-graph-run E2E_GRAPH_NODES="bazzite-installed-ssh" \
  E2E_GRAPH_ARGS="--arg bazziteSshPort=2224"
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
- Graph-owned installed Bazzite VM provisioning and Deck SSH verification.
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
- Manual Steam Input radial validation in Game Mode.
- Two-device Syncthing conflict/resolution testing.
- Future editors for `verification/screenshots.json`, BIOS status, and advanced
  per-emulator settings.
