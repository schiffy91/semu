# Semu

Semu is a Steam Deck-first emulation environment manager. Its goal is
RetroDeck-like plug-and-play behavior with a smaller, declarative compiler
core: targets, systems, emulators, input, rendering, launchers, ROM paths,
BIOS checks, Syncthing integration, screenshot hooks, and packaging metadata
flow from JSON definitions into generated ES-DE, emulator, Steam Input,
AppImage, and verification outputs.

The target definition tree is the source of truth for build planning. The BTRC
CLI is the executable compiler surface for build, verify, config, settings,
assets, sync, launch, and Deck checks.

## Current Status

This repo currently provides:

- A compiled BTRC CLI: `build/out/semu`.
- Declarative target definitions under `config/targets/`, `config/systems/`,
  `config/emulators/`, `config/input/`, and `config/assets/`.
- A generated compiler/UI manifest: `semu.json`.
- A generated C snapshot used by Nix builds: `build/generated/semu.c`.
- Steam Deck controller defaults with gyro disabled, right trackpad as mouse,
  left trackpad for radial hotkeys, and unified save/load/quit/menu actions.
- A custom keymap language in `config/input/keymaps/steam_deck.skm` with tokenizer, parser,
  code generators, and authoring errors.
- Declarative Syncthing config in `config/settings/sync.json`.
- Declarative Semu settings in `config/settings/semu-settings.json`, with BTRC-owned
  `settings get|put|toggle|ui|apply` access for ROMs, sync toggles, visual
  policy, and ES-DE settings entry rollout.
- Per-system rendering settings in `config/assets/systems/*.json`, with
  BTRC-owned `assets get|put` access for shader/effect files, bezel files,
  content viewport declarations, display layout, scale policy, emulator source,
  and generic renderer backend selection.
- Declarative screenshot verification config in `config/verification/screenshots.json`.
- Linux launcher shims and AppRun glue under `build/packaging/linux/`.
- Nix packages for the BTRC CLI, bundled emulator set, routed emulator wrappers,
  pinned visual asset packs, SteamOS graphics bridge, and a NixOS module.
- Fast host coverage for compiler verification, source-hook metadata, rendering
  contracts, generated ES-DE settings entries, routed input configs, payload
  audits, and whitespace checks.

Important remaining production gaps are tracked in `TODO.md` and
`tests/E2E.md`. The active production gate is the physical Steam Deck loop:
installed AppImage, ES-DE settings entries, every routed emulator, fullscreen
launch, input, unified quit, and screenshot evidence. Bazzite parity runs only
after the Steam Deck path is complete.
The input, shader, bezel, and ES-DE settings rollout is specified in
`docs/input-and-visuals.md`.

## Source And Project Roots

Semu keeps source code separate from emulation data.

Default local layout:

| Root | Purpose |
|---|---|
| `~/Drive/dev/semu` | Git repo, Nix/BTRC source, compiler definitions, tests, packaging, and source templates. |
| `~/Drive/media/Games/Emulation` | Semu project/data root: ES-DE config, generated launcher glue, runtime state, saves, states, screenshots, and local media paths. |
| `/run/media/deck/SD` or `/mnt/SD` | Steam Deck SD-card media root for ROMs, BIOS, keys, saves, and synced user content. |

The root `Makefile` defaults to:

```sh
SEMU_PROJECT="$HOME/Drive/media/Games/Emulation"
SEMU_ASSET_ROOT="$PWD"
```

Override `SEMU_PROJECT` when building against another data root:

```sh
make configs SEMU_PROJECT="$HOME/Drive/media/OtherEmulation"
```

Generated emulator configs are outputs. User-owned edits should go through
`$SEMU_PROJECT/semu.json` and `$SEMU_PROJECT/overrides/**`, or through the Semu
CLI/settings UI. The repo definitions under `config/targets/`,
`config/systems/`, `config/emulators/`, `config/input/`, and `config/assets/`
remain source-controlled defaults.

## Design Principles

- Steam Deck first, Linux out of the box second, macOS only where already useful.
- Declarative user-editable inputs over hidden procedural state.
- BTRC command logic stays in the compiler/generator surface: build, verify,
  config/apply, settings, assets, keymap metadata, sync status/toggle/open,
  doctor/bootstrap, Deck status, and launcher routing.
- Nix for reproducible emulator builds and AppImage payload assembly.
- Nix-built GUI emulators route through the bundled nixGL Mesa bridge on
  SteamOS/non-NixOS hosts so OpenGL/Vulkan drivers resolve cleanly.
- Shaders and bezels are Semu-owned process effects, not emulator-native
  rendering features.
- Routed wrappers place Linux emulator state under
  `.semu/appimage-state/<emulator>` via `HOME` and `XDG_*`.
- Small abstractions: controller model, backend, emitted input, emulator keymap.
- KISS over framework layering. Generated files are allowed; BTRC remains the
  single compiler source of truth.

## Repository Layout

| Path | Purpose |
|---|---|
| `src/main.btrc` | BTRC CLI entrypoint. |
| `src/cli.btrc` | Build, verify, settings, assets, sync, launcher, and Deck command surface. |
| `src/compiler/` | Compiler stages: lexer, parser, model, resolver, checker, and generator. |
| `src/generators/` | ES-DE, Steam Input, AppImage, and emulator output generators. |
| `src/lib/` | Semu-specific helpers for owned paths, template expansion, and merge policy. |
| `config/targets/` | Compiler target definitions such as `steam-deck`, `linux`, `bazzite`, and `macos`. |
| `config/systems/` | Declarative system catalog definitions. |
| `config/emulators/<name>/` | Per-emulator package, path, launch, input, and rendering definitions. |
| `config/input/` | Input action/device definitions and source Steam Input templates. |
| `config/assets/` | Asset declarations for shaders, bezels, hardware display targets, and defaults. |
| `docs/` | Architecture notes and production rollout plans for input, visuals, settings, and verification. |
| `semu.json` | Generated JSON manifest for UI/editor/tooling consumers. |
| `build/generated/semu.c` | Generated C snapshot compiled by Nix packages. |
| `config/input/keymaps/steam_deck.skm` | Editable Steam Deck keymap source. |
| `$PROJECT/.semu/generated/input/steam-input/*.vdf` | Generated Steam Input template files. |
| `config/settings/semu-settings.json` | Editable Semu visual/UI policy and settings defaults. |
| `config/assets/systems/*.json` | Editable per-system rendering profiles for shaders/effects, bezels, content viewport resolution, layouts, backend choices, and emulator emulators. |
| `config/settings/sync.json` | Editable Syncthing policy. |
| `config/verification/screenshots.json` | Editable screenshot hook policy. |
| `build/packaging/linux/AppRun` | AppImage entrypoint, bundled Nix-store mount wrapper, and generated ES-DE prep. |
| `build/packaging/linux/bin/semu-*` | Thin Linux launcher shims. |
| `build/packaging/nix/*.nix` | Emulator packaging, routed wrappers, CLI package, visual asset pack, NixOS module. |
| `tests/` | Local, VM, AppImage, Deck-style, and regression tests. |

## Project/Data Layout

The project root is selected with `--project` or `SEMU_PROJECT_DIR`. It is not
the Git repo.

| Project Path | Purpose |
|---|---|
| `.semu/semu.json` | Project-local Semu configuration. |
| `.semu/overrides/` | User-owned overrides for systems, emulators, input, and rendering. |
| `.semu/generated/` | Semu-generated compiler outputs. |
| `.semu/state/`, `.semu/cache/` | Runtime state and caches. |
| `ES-DE/` | ES-DE settings, gamelists, media, and generated custom systems glue. |
| `.semu/generated/bin/semu-settings` | Generated settings launcher used by the patched ES-DE Utilities menu entry. |
| `.semu/generated/emulators/` | Generated emulator profile outputs consumed by launchers. |

Edit compiler definitions for target behavior. Edit BTRC sources under `src/`
only for compiler or launcher behavior, then regenerate `semu.json` and
`build/generated/semu.c` when compiler behavior changes.

## Configuration Boundary

Repo definitions and Semu-owned source files are the only user-editable
configuration surface:

- `config/targets/*.json`
- `config/systems/*.json`
- `config/emulators/*/{emulator,package,paths,launch,input,rendering}.json`
- `config/input/actions.json`
- `config/input/devices/*.json`
- `config/settings/semu-settings.json`
- `config/settings/sync.json`
- `config/assets/*.json`
- `config/assets/systems/*.json`
- `config/input/keymaps/steam_deck.skm`
- `config/verification/screenshots.json`

Everything else that an emulator or ES-DE reads is compiled from those files by
`semu build target`, `semu build emulator`, `semu build configs`, or another
command that explicitly applies a generated change. That includes
`$PROJECT/.semu/generated/emulators/`, `$PROJECT/.semu/generated/input/steam-input/*.vdf`,
`$PROJECT/ES-DE/custom_systems/`, `$PROJECT/ES-DE/es_settings.xml`, systemd
user units, desktop entries, and generated launcher files.

Steam itself is external state. The compiler emits owned Steam Input templates
and selection metadata under generated project paths, but Steam userdata remains
outside the repo/source boundary. Deck verification reports the selected
template separately from shortcut existence, because a shortcut without the Semu
template does not prove the left-trackpad radial contract.

Runtime emulator config under `.semu/appimage-state/<emulator>` is emulator
state. Semu may read it to broadcast normalized rendering state, and launchers
may seed it from generated profiles, but it is not the source of truth. Do not
hand-edit emulator configs to change Semu policy; change the owned JSON/keymap
files and apply. Verification artifacts under `.semu/verification` are also
Semu emulator state; they record launcher evidence such as quit-watch logs, not
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

From the source repo root:

```sh
export SEMU_PROJECT="$HOME/Drive/media/Games/Emulation"
make btrc-build
make steam-deck
make configs
make verify
```

For a microSD ROM directory, pass the real mount path:

```sh
build/out/semu build target steam-deck \
  --project "$SEMU_PROJECT" \
  --roms "/run/media/deck/SD"
```

`/run/media/deck/SD` is auto-normalized when it contains the common
`Emulation/ES-DE/ES-DE/ROMs` layout.

What the `steam-deck` target does:

- Creates `$PROJECT/.semu/` state, cache, generated, and override directories.
- Writes `$PROJECT/.semu/semu.json` from `config/settings/semu-settings.json` if missing.
- Creates the ES-DE content tree.
- Creates ROM directories for every declared system.
- Creates BIOS target directories.
- Seeds Steam Deck controller defaults.
- Reads input policy from `config/input/`; no project-local keymap source is seeded.
- Renders Steam Input templates.
- Renders ES-DE systems and find rules.
- Renders `semu.json`.
- Installs desktop and systemd user sync helpers where applicable.

After install, add your legally dumped BIOS/firmware files and ROMs, then run:

```sh
build/out/semu doctor --project "$SEMU_PROJECT"
```

The doctor reports missing BIOS, controller defaults, keymap validity,
Steam Input templates, 3DS ROM NoCrypto status, screenshot hooks, sync status,
and Linux launchers.

For the physical Game Mode pass, launch Semu from Steam, open a representative
game for each routed emulator, use the left-trackpad radial Quit action, and use
Save State/Load State for each emulator with generated state support. Before
switching modes, run the readiness gate so missing Steam
shortcut/AppImage/checklist state is caught without confusing Desktop Mode with
Game Mode:

```sh
build/out/semu deck game-mode-ready --project "$SEMU_PROJECT" --prepare
```

After the physical pass, require Game Mode readiness, complete quit evidence,
generated state save/load evidence, owned source config, and the rendering asset
audit:

```sh
build/out/semu deck game-mode-ready --project "$SEMU_PROJECT" --require-evidence
build/out/semu deck game-mode-evidence --project "$SEMU_PROJECT"
build/out/semu deck state-evidence --project "$SEMU_PROJECT"
build/out/semu deck production-ready --project "$SEMU_PROJECT"
```

Missing or startup-only logs remain `PENDING`; a passing emulator needs a
quit-watch line with a `reason=` value plus child exit evidence. State evidence
is Semu emulator state under `.semu/verification/state-actions`; emulator-native
config remains compiled output.

## Linux Desktop Quick Start

Build the bundle:

```sh
make emulator
nix build .#default
```

Run the BTRC CLI from the bundle:

```sh
result/bin/semu build target linux --project "$SEMU_PROJECT"
result/bin/semu build configs --project "$SEMU_PROJECT"
result/bin/semu verify target linux --project "$SEMU_PROJECT"
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
with the bundled `nixGLDefault` host graphics bridge when available. This keeps
the emulators Nix-built while allowing them to use the host GPU stack; RetroArch
uses the host Mesa GL path on Steam Deck.

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
nix build --impure .#default
build/packaging/linux/build-appimage.sh \
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
- `build/packaging/linux/AppRun` detects a bundled `nix/store` payload and re-execs
  through bubblewrap with that payload mounted read-only at `/nix/store`.

Fallback behavior:

- If a routed Nix binary exists inside the AppImage, ES-DE find rules point at it.
- If the routed payload includes `nixGLDefault`, emulator launches use it for
  host GPU driver access on SteamOS/non-NixOS systems.
- Flatpak-backed launchers are the host fallback for supported standalone
  emulators when the AppImage is built without a routed Nix payload.

Automated tests validate AppImage assembly logic with fake ES-DE and fake
appimagetool. Physical Deck evidence currently covers Desktop Mode ES-DE launch,
AppImage-owned Syncthing, SD-card ROM detection, strict visual asset auditing,
and required-route process launch/input/quit automation. Game Mode, physical
Steam Input radial quit, and ES-DE return flow remain the final manual gates
listed in `tests/E2E.md`.

## Declarative Configuration

### `src/main.btrc`, `src/cli.btrc`, And `src/compiler/`

This is the canonical BTRC compiler tree. It defines:

- Target build and verify commands.
- JSON definition parsing and resolution.
- ES-DE commands and find rules.
- Settings entries and stable launcher wrappers.
- ROM/BIOS path discovery.
- Steam Deck input and generated Steam Input selection data.
- Sync status/open/toggle commands.
- AppImage launcher routing, rendering wrapper handoff, and quit-watch routing.

Regenerate artifacts:

```sh
make btrc-build
build/out/semu manifest --output semu.json
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

### `config/input/keymaps/steam_deck.skm`

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

Inspect keymap ownership:

```sh
build/out/semu keymap --project "$SEMU_PROJECT"
```

Input actions and device declarations are compiler inputs under
`config/input/**`. Generated emulator and Steam Input outputs are rebuilt by
`build configs`, `settings apply`, `config apply`, or a mutating command that
passes `--apply`.

### `config/settings/sync.json`

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
build/out/semu sync status --project "$SEMU_PROJECT"
build/out/semu sync open --project "$SEMU_PROJECT"
```

### `config/settings/semu-settings.json`

Editable Semu policy for the settings entrypoint, visual defaults, and settings
CLI/UI. It is intentionally key/value-shaped so ES-DE entries, scripts, and the
terminal UI all use the same `get` and `put` contract.

Default visual policy:

- Native integer scaling is enabled for classic systems.
- CRT effects and bezels are enabled by policy for classic systems.
- The default modern exclusions are `wiiu` and `switch`.
- Launched emulator processes receive the generic Semu renderer environment;
  ES-DE and Semu settings entries do not.
- Generated emulator configs set fullscreen, graphics API, scaling/aspect,
  layout, and emulator-state paths. They do not receive renderer asset
  references or emulator-native render settings.
- The renderer uses the Semu content viewport resolver before applying any
  shader/effect or bezel. `vkBasalt` is the Vulkan process backend; gamescope
  ReShade is the compositor fallback.

Commands:

```sh
build/out/semu settings get roms.dir --project "$SEMU_PROJECT"
build/out/semu settings put roms.dir /run/media/deck/SD apply --project "$SEMU_PROJECT"
build/out/semu settings toggle visual.bezels apply --project "$SEMU_PROJECT"
build/out/semu settings ui --project "$SEMU_PROJECT"
build/out/semu settings apply --project "$SEMU_PROJECT"
```

`roms.dir` uses the same normalization as the target build; `/run/media/deck/SD`
resolves to `Emulation/ES-DE/ES-DE/ROMs` when that layout is present.
The terminal UI is dependency-free BTRC output around Semu-owned settings.
`apply` compiles those settings into generated ES-DE entries, Steam Input
selection data, launcher wrappers, and emulator-facing outputs.

### `config/assets/systems/*.json`

Editable per-system rendering profiles. Each system has its own JSON file with
a shader/effect file, bezel file, content viewport declaration, hardware-display
target, layout, scale policy, dynamic aspect policy, renderer backend, and
emulator emulator source.

Examples:

```sh
build/out/semu assets get gb renderer.shader.source_asset --project "$SEMU_PROJECT"
build/out/semu assets get gb renderer.bezel.composition_effect_file --project "$SEMU_PROJECT"
build/out/semu assets put gb renderer.bezel.source_asset bezels/gb/classic-grey-game-boy.json --apply --project "$SEMU_PROJECT"
build/out/semu assets put ps2 renderer.bezel.widescreen_source_asset bezels/ps2/clean-wide.json --apply --project "$SEMU_PROJECT"
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

The launcher reads these definitions, resolves the content viewport, and hands
the chosen shader/effect, bezel, and backend config to `semu-render`.
`semu-render` wraps only emulator processes, not ES-DE or Semu settings entries,
so settings UI launches are not filtered or bezeled.

Dynamic 4:3/16:9 systems can keep CRT defaults for 4:3 while editing
`widescreen_shader_file` and `widescreen_bezel_file` for widescreen output.
Missing optional art packs stay visible as empty/unresolved fields instead of
silently pretending an asset exists.

Deck screenshot verification is the source of truth for whether each selected
effect is actually visible around a running emulator.

### `config/verification/screenshots.json`

Editable screenshot policy. Defaults declare:

- `capture_before_launch`
- `capture_after_spawn`
- `capture_after_exit`
- output pattern:
  `${paths.project_screenshots}/verification/${emulator}/${hook}.png`
- auto tool selection from `grim`, `spectacle`, `gnome-screenshot`, or
  ImageMagick `import`

Screenshot policy is consumed by compiler verification and the Deck harness.
There is no separate screenshot command surface in the flattened compiler CLI.

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

Supported extensions are declared per system in `config/systems/*.json` and emitted
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
| `wiiu_keys` | Wii U | yes | `keys.txt` | `ES-DE/ES-DE/bios/wiiu` |
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

Semu reports 3DS ROM status through `settings n3ds status` / `n3ds status`.
It does not mutate ROMs during the Deck verification loop.

## Command Reference

Compiler facade:

```sh
build/out/semu build target steam-deck --project "$SEMU_PROJECT"
build/out/semu build emulator retroarch --project "$SEMU_PROJECT"
build/out/semu build configs --project "$SEMU_PROJECT"
build/out/semu verify target steam-deck --project "$SEMU_PROJECT"
```

Compiler support commands:

```sh
build/out/semu doctor --project "$SEMU_PROJECT"
build/out/semu bootstrap --project "$SEMU_PROJECT"
build/out/semu apprun prepare --project "$SEMU_PROJECT"
build/out/semu deck --project "$SEMU_PROJECT"
build/out/semu manifest --project "$SEMU_PROJECT"
build/out/semu e2e status --project "$SEMU_PROJECT"
```

Config:

```sh
build/out/semu config env --project "$SEMU_PROJECT"
build/out/semu config apply --project "$SEMU_PROJECT"
build/out/semu config compile --project "$SEMU_PROJECT"
```

Use `settings put roms.dir VALUE --apply` to change ROM roots and regenerate
emulator/ES-DE files.

Settings:

```sh
build/out/semu settings get roms.dir --project "$SEMU_PROJECT"
build/out/semu settings put roms.dir "/path/to/ROMs" --apply --project "$SEMU_PROJECT"
build/out/semu settings put visual.integer_scaling true --project "$SEMU_PROJECT"
build/out/semu settings put visual.bezels true --project "$SEMU_PROJECT"
build/out/semu settings toggle visual.bezels --project "$SEMU_PROJECT"
build/out/semu settings ui --project "$SEMU_PROJECT"
build/out/semu settings apply --project "$SEMU_PROJECT"
build/out/semu settings sync status --project "$SEMU_PROJECT"
build/out/semu settings n3ds status --project "$SEMU_PROJECT"
```

Assets:

```sh
build/out/semu assets get gb renderer.shader.source_asset --project "$SEMU_PROJECT"
build/out/semu assets get gb renderer.bezel.composition_effect_file --project "$SEMU_PROJECT"
build/out/semu assets put ps2 renderer.bezel.source_asset "bezels/ps2/sony-crt.json" --apply --project "$SEMU_PROJECT"
build/out/semu assets put ps2 renderer.bezel.widescreen_source_asset "/path/to/wide-frame.json" --apply --project "$SEMU_PROJECT"
build/out/semu assets put wii content_viewport.default_aspect "16:9" --apply --project "$SEMU_PROJECT"
```

Keymaps:

```sh
build/out/semu keymap --project "$SEMU_PROJECT"
```

The keymap command reports where Semu input actions are declared. The compiler
owns generated emulator and Steam Input files; Steam userdata selection remains
external Deck verification state.

Sync:

```sh
build/out/semu sync status --project "$SEMU_PROJECT"
build/out/semu sync open --project "$SEMU_PROJECT"
build/out/semu sync toggle --project "$SEMU_PROJECT"
```

Launchers:

```sh
build/out/semu launcher retroarch --project "$SEMU_PROJECT" -- -L core.so game.gba
build/out/semu launcher dolphin --project "$SEMU_PROJECT" -- game.iso
build/out/semu launcher routed retroarch /path/to/retroarch --project "$SEMU_PROJECT" -- -L core.so game.gba
```

Fast compiler checks:

```sh
make payload-audit
make compiler-build
make compiler-verify
make esde-settings-entry
make source-hook-metadata
make rendering-contract
make cemu-ryujinx-input
make retroarch-n64-config
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
nix run .#default -- doctor --project "$SEMU_PROJECT"
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
make configs
make steam-deck
```

Run fast tests:

```sh
make test
```

Run the full local verification:

```sh
make verify
```

Run the fast host check suite:

```sh
make test
```

### BTRC Compiler Dependency

Normal Nix builds compile the committed `build/generated/semu.c` snapshot. The flake
also exposes `.#btrcpy`, backed by the pinned `btrc` input, so `make
btrc-build` can regenerate artifacts without a machine-local compiler path.

That keeps the production package tied to committed generated C while BTRC
development remains reproducible through the Nix flake/dev shell. Compiler
experiments should update or override the flake input intentionally in a
separate compiler-work branch, not through local Makefile paths.

Run Deck-oriented checks first against the physical Steam Deck:

```sh
make deck-ssh-smoke
```

Bazzite is deferred until the physical Steam Deck path is complete and
screenshot/loop verified:

```sh
make bazzite-vm-smoke
make bazzite-desktop-vm-smoke
```

`make test` currently covers:

- Compiler target verification.
- ES-DE settings entry generation.
- Payload audits that keep VM images, ROMs, BIOS, AppImages, saves, and
  screenshots out of the repo.
- Source-hook package metadata.
- Rendering contracts.
- Cemu/Ryujinx generated input configs.
- RetroArch N64 route config.
- `git diff --check`.

## Source Ownership

Build, verify, config, settings, assets, sync, keymap metadata, launcher
routing, tests, and utility behavior belong in the BTRC source tree under
`src/`.
Packaging, generated manifests, generated C, Steam Input templates, and tests
consume that BTRC-owned compiler model.

## Known Gaps

See `tests/E2E.md` for the active verification matrix. The short version:

- Physical Steam Deck Game Mode validation.
- Manual Steam Input radial validation in Game Mode.
- Two-device Syncthing conflict/resolution testing.
- Future editors for `config/verification/screenshots.json`, BIOS status, and advanced
  per-emulator settings.
