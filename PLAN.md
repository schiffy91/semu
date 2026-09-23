# Semu Plan

This file is the goal contract. It supersedes `AGENTS.md`, `README.md`, and
everything under `docs/` until milestone 1 replaces them. On every start,
resume, or context compaction, reread this file and the **Status** block at the
bottom before deciding what to do next.

## Vision

One declarative, Nix-built emulation environment that launches every system in
`config/systems/` from ES-DE with correct fullscreen, controls, save/load, and
Start+Select quit-to-frontend, on two production targets:

- `linux-desktop`: the NixOS host FRACTAL-NORTH (RTX 4090, Plasma Wayland,
  Xbox controllers via xone, ROMs at `~/Games/Emulation/ES-DE/ES-DE/ROMs`).
- `steam-deck`: the physical Deck (Game Mode, Steam Input, SD-card ROM layout).

The two targets share the platform string and nothing else by default. Neither
is "linux". Every emulator is built by Nix from a pinned source revision.

## Standing directives

- Every emulator, every libretro core, RetroArch, ES-DE and the renderer
  is its own flake (`flake.nix` plus `flake.lock` in its directory) that
  pins its upstream source as an input, borrows only build wiring from
  nixpkgs, sets `allowSubstitutes = false`, and declares
  `semu.platforms` for linux, macos and windows (windows stays `"planned"`
  until a port is built). The root flake composes them through relative
  path inputs; the `platform-matrix` check enforces all of this. A nixpkgs
  package used as-is, or a cache binary, is a regression.

These come from the owner and do not change without an explicit new message:

- One emulator at a time. Get one working end to end, then the next.
- Only Semu-owned files are edited by hand. Emulator-native files are compiled
  output. Never hand-edit generated output to make something pass.
- ROMs, BIOS, keys, firmware, and existing saves under `~/Games/Emulation` are
  read-only. Never delete, move, or rewrite them.
- Agnostic of X11, Wayland, and gamescope in production paths.
- Start+Select at roughly the same time quits the emulator immediately and
  returns to ES-DE.
- One shader/bezel system for everything, but it is built last (milestone 6),
  after every system launches plain.
- Bazzite only after the complete Deck matrix passes.
- Code style: BTRC with the current stdlib, dense class methods, descriptive
  names, no block comments, trailing comments of 5 to 10 words only.

## Audit summary (2026-09-19)

The tree is 42.6k lines of BTRC in `src/` and 32k in `tests/`, last touched
2026-07-23, pinned to BTRC `964ffe7` (2026-07-22). Findings that drive the plan:

- **BTRC drift is small.** Five mechanical edits port the whole tree to the
  current BTRC (see *Port recipe*). The port transpiles, compiles, and emits
  23 config files against the real `config/`. The rewrite is for design, not
  language.
- **52% of `src/` is AppImage packaging and provenance** (`src/generators/
  appimage/**`, ~22k lines). It reimplements appimagetool by hand and re-proves
  properties the Nix store already guarantees. No deployable artifact was ever
  produced. Delete it.
- **The compiler is a JSON validator that writes a build plan nothing reads.**
  It hand-rolls SHA-256, result types, and three JSON accessors. Keep its
  four-stage shape, its settings precedence ladder, and its owned-paths
  boundary as design. Discard the code.
- **`platform: linux` means Steam Deck.** `config/settings/defaults.json` sets
  linux ROM and BIOS roots to `/run/media/deck/SD/...`; `config/input/` has
  only `steam-deck`; `steam_input.btrc` rejects any controller model but the
  Deck. There is no desktop target.
- **The renderer patches ten emulators at exact upstream function bodies**
  and every `rendering.json` says `runtime_proven: false`. The Ryujinx patch
  hardcodes P/Invoke struct sizes. This is the most fragile code in the tree
  and it was built before one ROM ever launched.
- **The input supervisor is 3.5k lines, a third of it self-tests** compiled
  into the production binary. The quit/journal core is about 600 lines.
- **Nix packaging is the best code in the repo**: `overrideAttrs` over nixpkgs
  recipes onto pinned revisions with patches beside them, eval-time
  assertions. Pins are stale (nixpkgs 2025-05-04) and the container build
  passes `--option sandbox false`.
- **Tests:** about a fifth pin exact emulator config keys and hotkeys and are
  the spec for the rewrite. The rest test fakes of the unbuilt AppImage
  pipeline, enforce directory layout, or are orphaned entry points.
- **Ledger state:** 0 of 17 systems accepted, 88 open todo items, the last
  live Deck session captured a black frame.

## What to keep, what to delete

Keep as the declarative source of truth:

- `config/systems/*/{system,shaders,bezels}.json`
- `config/emulators/*/package.json`, `package.nix`, and the patches listed as
  essential: `retroarch/retroarch_commands.patch`,
  `retroarch/retroarch_get_status_null_safety.patch`. Other patches are kept
  in git history and reintroduced only when milestone 6 needs them.
- `config/emulators/*/emulator.json` slices `platforms.<os>.{executable,args,
  state}` and `input.actions`. Drop `aspect_probe`, `doc`, and the
  `profile.json` `compiler.{bindings,line_groups}` DSL; keep the literal file
  bodies and substitute with one generic resolver.
- `config/assets/{shaders,bezels}.json`, `config/assets/bezels/**`,
  `config/assets/esde/templates/*.xml`
- `config/settings/defaults.json` sections, with the Deck paths moved into
  the `steam-deck` target.
- `packaging/nix/emulators.nix`, `render-hook.nix`, `renderer.nix`,
  `visual-assets.nix`, `semu_app.nix`, `checks.nix`, `packaging/esde/package.nix`
- Spec-bearing tests, kept as fixtures for the new contract tests:
  `tests/compiler/emulator_runtime_contract_test.btrc`,
  `retroarch_production_contract_test.btrc`,
  `steam_input_contract_test.btrc`, `esde_contract_test.btrc`,
  `gb_definition_contract_main.btrc`, `melonds_touch_contract_test.btrc`,
  `azahar_touch_contract_test.btrc`, `cemu_production_contract_test.btrc`,
  `standalone_emulator_production_contract_test.btrc`,
  `tests/fixtures/retroarch/*.cfg`
- The only test that ran a real emulator, as the template for all integration
  tests: `tests/integration/retroarch/{run.sh,scenario.sh,runtime.nix,
  synthetic_core.btrc,glass_scenario.sh}`
- From the Deck harness, two facts only: gamescope screenshot type 3 with the
  stabilization loop (`tests/targets/steamdeck/remote_system.btrc:500-528`)
  and the uinput device construction (`uinput_driver.btrc:110-166`).

Design to carry over without the code:

- Pipeline shape: parse, resolve, check, emit (`src/compiler/pipeline.btrc`).
- Settings precedence: `defaults < target < $SEMU_HOME/semu.json <
  $SEMU_HOME/overrides/**/*.json < --settings-json` (`src/lib/merge.btrc`).
- Owned-paths boundary: no symlinks, no traversal, never write into `src/` or
  `config/` (`src/lib/owned_paths.btrc:77-268`), stdlib-backed.
- Target as data with inheritance (`config/targets/*.json`).
- The action ABI record and the quit process-group termination
  (`src/generators/input/action_abi.btrc`, the 250 ms bounded kill in
  `linux_supervisor.btrc`).

Delete: `src/generators/appimage/**`, `src/generators/appimage.btrc`,
`packaging/appimage/**`, `packaging/install/**`, `nixGL` input and
`nixGLRuntime`, `src/generators/rendering/native/**` and
`rendering/retroarch/**` (returns in milestone 6 from history),
`src/generators/{steam_input,sync,settings}.btrc` (return in milestones 5
and 7), `src/generators/input/steam_install.btrc`, `tests/core/tree_audit/**`,
`tests/compiler/source_architecture_contract_test.btrc`,
`tests/compiler/appimage_*`, `tests/targets/steamdeck/**` except the two facts
above, `tests/targets/bazzite/**`, every orphaned `tests/compiler/*_main.btrc`
not wired in `tests/Makefile`, `docs/**`, `AGENTS.md` (replaced), `README.md`
(rewritten).

## Port recipe

To get a compiling baseline of the old tree for extracting generators, apply
to a scratch copy of `src/`:

1. `import std.{a, b}` becomes one line per module: `import Library.A;`
   (`fs` is `FileSystem`, `cli` is `CLI`, `json` is `JSON`, `io` is `IO`,
   the rest capitalize). Relative imports get a trailing semicolon.
2. `JsonValue` becomes `JSONValue`.
3. `CliArgs` becomes `CLIArgs`, `CliCommandLine` becomes `CLICommandLine`.
4. `Directory.entriesBounded(n)` becomes `Directory.entries()`.
5. `sync.btrc` needs a local `HttpFraming` class with `requestTarget`,
   `token`, `headerLine` returning bool. `Library.http_framing` is gone.

Then `btrcpy src/main.btrc -o build/semu.c --strict-imports --no-cache
--no-stdlib` and `cc build/semu.c -std=c11 -o build/semu -lm`.

## Milestones

Each milestone has a done criterion. A milestone is done only when its
criterion is observed, not when its code is written. Do not start the next
milestone's code while the current criterion is unmet, except for bounded
prep that the current milestone needs.

### M1. Reset the repo on current BTRC

- New `src/` written against the current BTRC stdlib (`Library.JSON`,
  `Library.FileSystem`, `Library.Process`, `Library.Digest`, `Library.Map`,
  `Library.Result`, `Library.CLI`). No hand-rolled hashing, result types, or
  JSON accessors.
- Modules: `model` (typed records loaded from `config/`), `resolve` (target
  inheritance, settings ladder), `check` (one diagnostic list with file, field,
  message), `emit` (ES-DE files, emulator profiles), `launch` (argv, env,
  process group, quit chord), `cli`. Keep each under about 500 lines.
- `config/targets/linux-desktop.json` and `steam-deck.json`. Desktop ROM root
  defaults to `${home}/Games/Emulation/ES-DE/ES-DE/ROMs`; the Deck keeps its
  SD-card paths. BIOS, keys, and firmware are read from the existing
  per-emulator directories under `~/Games/Emulation` on the desktop.
- Apply the keep/delete lists. Replace `AGENTS.md` with a short file that
  points here. Rewrite `README.md` for the two targets.
- `flake.nix`: BTRC follows the same input as `/etc/nixos`; nixpkgs pinned to
  a release branch; the sandbox stays on.
- Tests: `make test` runs the ported contract tests against the new emitters.
- **Done when:** `make build && make test` pass on this host in under one
  minute and `build/semu build configs --target linux-desktop` emits ES-DE and
  RetroArch files with no Deck path in them.

### M2. Game Boy from ES-DE on the desktop

- Stock nixpkgs `retroarch` with `libretro.gambatte`, no source patches.
- Emit `es_systems.xml`, `es_find_rules.xml`, `es_settings.xml`,
  `retroarch.cfg` for `linux-desktop`. Keys from
  `retroarch_production_contract_test.btrc`: `-f`, `video_fullscreen = "true"`,
  `input_quit_gamepad_combo = "0"`, `input_driver = "sdl2"`, autoconfig toast
  suppressed, `network_cmd_enable = "true"` on port 55355.
- A flake package `semu` that wraps ES-DE and the launcher shims, and a module
  `nix/apps/gaming/semu/default.nix` in `/etc/nixos` gated on
  `settings.apps.semu.enable`, mirroring `steam.nix`: `runuser` activation
  that runs `semu prepare --target linux-desktop`, a desktop entry, persisted
  paths for `~/ES-DE` and the emulator config and save roots.
- Start+Select quit through the launcher's process-group kill, both chord
  orders, 250 ms window. Save state and load state on the generated hotkeys.
- **Done when:** on FRACTAL-NORTH, from a cold boot, ES-DE opens from the app
  menu, a real Game Boy ROM launches fullscreen with the Xbox pad, save then
  load restores state, Start+Select returns to ES-DE, and a reboot keeps the
  ES-DE settings and the save state.

### M3. Every RetroArch system on the desktop

- gbc, gba, nes, snes, genesis, n64, psx as primaries; nds (desmume), psp
  (ppsspp), dreamcast (flycast) as fallback cores. All driven by
  `config/systems/*/system.json`; no system id in BTRC.
- One headless integration test per system in the `tests/integration/
  retroarch` style: Xorg dummy plus llvmpipe inside a Nix check, real core,
  the smallest real ROM available, assert nonblank frame, save/load, and that
  the UDP quit returns exit 0 with no orphan processes.
- **Done when:** one real ROM per system launches from ES-DE on the desktop
  with the same hotkeys, and the integration checks pass in `nix flake check`.

### M4. Native emulators on the desktop, one at a time

Order: dolphin (gc, wii), pcsx2, ppsspp, flycast, melonds, azahar, cemu,
ryujinx. For each:

- Refresh the pin in `package.json` to a current release, rebuild through
  `package.nix`, drop the rendering patch.
- Emit the profile from the keys in `emulator_runtime_contract_test.btrc`
  and the per-emulator contract tests. Quit and save hotkeys through the
  emulator's own config where it supports them; a source patch only where it
  does not, and then the smallest possible one.
- An integration test in the M3 style where the emulator can run headless;
  where it cannot, a launch-and-quit test that checks the process tree.
- **Done when:** one real ROM per system launches from ES-DE on the desktop,
  fullscreen, with pad input, save/load where the emulator supports it, and
  Start+Select quit. Sixteen of seventeen systems are then desktop-complete
  (switch counts as done when Ryujinx boots a real title with the existing
  keys and firmware).

### M5. Steam Deck delivery

- Delivery is the Nix closure the old runtime builder already produced,
  shipped as a relocatable tarball plus a 20-line launcher shim. No hand-rolled
  AppImage. If a single-file artifact is later required, use
  `nixpkgs.appimageTools`, not custom code.
- Installer: verify sha256, extract to a digest-named release directory,
  switch a stable symlink, keep one previous release, `rollback` swaps back.
  ES-DE and the launchers resolve through the stable path. FUSE is not
  required.
- `config/input/steam-deck/`: Steam Input profile for the Deck controller
  with the binding table from `steam_input_contract_test.btrc`, published
  into Steam's directories with the existing atomic publish logic trimmed to
  what is needed.
- Deck harness: SSH deploy, gamescope screenshot type 3, uinput driver. Keep
  it under 1k lines.
- Accept on hardware in the M3 then M4 order, one system at a time, gated by
  the desktop integration tests passing on the same revision.
- **Done when:** every system that is desktop-complete launches from ES-DE in
  Game Mode on the physical Deck with pad input, save/load, and Start+Select
  quit, from one installed release, with a screenshot per system retained
  under `build/verification/steam-deck/<release>/`.

### M6. Unified shader and bezel renderer

- One shared library using librashader, exposing the two-hook ABI: after game
  pixels, before emulator UI; after emulator UI, before present.
- RetroArch first, using the hook patch from git history, then one native
  emulator to prove the model, then the rest one at a time. Evaluate whether
  the `GPU` and `SurfaceRenderer` groups in the BTRC stdlib can host the
  compositor before writing GL by hand.
- Bezel and shader selection from `config/systems/*/{shaders,bezels}.json`
  and `config/assets/*`, exposed as settings, with the Game Boy 18-case matrix
  from `gb_definition_contract_main.btrc` as the first visual test.
- **Done when:** the Game Boy row shows the default shader and bezel on both
  targets with inspected screenshots, and toggling to disabled and to one
  alternate works from settings without a rebuild.

### M7. Extras

ES-DE settings menu, Syncthing save sync, Steam shortcuts on the Deck. Each
returns from git history as its own milestone with its own done criterion.
None may block M1 to M6.

- M7a ES-DE settings menu. **Done when:** the real ES-DE shows a SEMU
  SETTINGS entry whose pages come from `semu settings ui`, and changing one
  value there lands in `$SEMU_HOME/semu.json` (observed, not inferred).
- M7b Syncthing save sync. **Done when:** `semu sync start` runs the bundled
  Syncthing with a Semu-owned home sharing `paths.content_root`, the device
  id and peers are settable from the menu, and a paired peer appears on the
  folder over Syncthing's own REST API.
- M7c Steam shortcuts. **Done when:** `semu steam shortcuts` adds the Semu
  launcher to every Steam profile's `shortcuts.vdf` without disturbing other
  entries, verified by an independent parser, and prints the launch URL.

### M8. Unified input and the native Semu menu

What the old repository specified (docs/production-goal.md, steam_input.json,
the input supervisor and the renderer's post-UI menu) and what the rewrite
had dropped: one action vocabulary for every emulator, a Semu-drawn in-game
menu, a plain-controller paradigm next to the Deck's trackpad radial, and
Semu executing the actions itself.

- One vocabulary in `config/input/<target>/input.json`: every action has a
  keyboard chord (`Ctrl+S` save, `Ctrl+A` load, `Ctrl+X` screenshot,
  `Ctrl+M` menu, `Ctrl+Q` quit, ...). The emulator profiles compile those
  chords into each emulator's own hotkey table; RetroArch is driven through
  its command port instead and its Select-button hotkeys are unbound so
  nothing double-fires.
- Plain controllers: hold Select and press a button (`gamepad_chords`):
  Y opens the Semu menu, R1 saves, L1 loads, B screenshots, D-pad left and
  right change the slot. Start+Select stays the quit chord. On the Deck the
  trackpad radial emits the keyboard chords; the supervisor reads keyboards
  too, so both paths produce the same actions.
- The native menu (`menu.items`): RESUME, SAVE STATE, LOAD STATE,
  SCREENSHOT, BEZEL ON/OFF, SHADER ON/OFF, QUIT GAME. Opening it pauses the
  emulator through its own pause action and closes it on resume. The
  renderer draws it from `SEMU_MENU_ITEMS` and the supervisor mirrors the
  same list, so the drawn selection and the executed action never diverge.
  Visual toggles flip the live renderer and persist to `semu.json`.
- **Done when:** on the desktop, a controller opens the menu with Select+Y,
  navigates it with the D-pad, saves a state from it (state file observed),
  toggles the bezel live, and Start+Select still quits, all captured
  headlessly through a truthfully named virtual gamepad; on the Deck the
  same through the trackpad radial (hardware pending).

### M9. Bezel and shader fidelity

The old repository baked bezels from declarative recipes (shell, panel,
glass, photo) with per-system glass reflections, tonemapped tube glow and
curated alternates. Today: photo-style PNG art per system, one generated
glass map (Game Boy), and the compositor's curvature, corner mask, vignette,
bloom and halo. Remaining: glass reflection maps for every shell, curated
disabled/default/alternate shader and bezel choices for every non-modern
system, the widescreen switch from emulator-reported aspect, and a recipe
pipeline that regenerates the committed art. **Done when:** every
non-modern system exposes disabled, default and one materially different
alternate for both shader and bezel, each captured and inspected.

### M10. Bezel packages: per-screen, per-bezel, declarative

Every bezel is a package under `config/bezels/<id>/bezel.json`: an art plate,
an optional background plate, a fixed or computed layout, an optional drawn
frame, and one entry per screen carrying the measured tube rectangle (in the
plate's own pixels), the corner shape (none, rounded, squircle with exponent),
the fit policy (aspect, fill, integer), inset, surround color, curvature,
vignette, bloom, glow, a glass plate with its reflection strength, and an
optional per-screen shader preset. Systems bind variants to packages in
`config/systems/<id>/bezels.json` (schema 4); the launcher compiles the
selected package into `SEMU_RENDER_SCREEN_<n>*` per screen and the compositor
runs one shader chain, one glass plate and one look per screen. Screen
openings are measured, never eyeballed: `tools/bezel-measure.py` (numpy,
pillow, scipy) finds tubes by seeded uniform regions, glass plates by opaque
bounds, outlined panels by raycast, and records the method next to the
numbers. Art is rendered from pinned upstream layers by `bezels.json` recipes
(`scene` multiplies Duimon's night lighting over Soqueroeu's living-room TVs
with the manufacturer badges; `flatten`/`glass` bake Duimon's DMG-01, Game
Boy Color and AGB-001 device plates and lenses) and baked into
`config/assets` with output metadata. Dual-screen systems default to
computed layouts (`dual-main-right`: the main screen at the largest integer
scale that still leaves the second screen at least 1x beside it, both with
drawn frames; also main-left, side-by-side, stacked) selectable per system
like any other bezel variant, with the Duimon shells as fixed alternates. The
3DS runs Semu's own build of the Azahar tree's libretro target
(`config/emulators/retroarch/cores/azahar`) with separate top and bottom
panel presets; the DS runs the libretro melonDS build so the renderer hooks
it. **Done when:** every system's default capture shows its measured tube
holding the game at the right aspect inside the right plate, with the
per-screen look applied and nothing bleeding over the bezel.

### M11. Bezel dimensions from the files, verified in a fast loop

The render-based calibration (RetroArch running each upstream preset with a
flat card) was the wrong tool: a build-time calculation became a slow, flaky
runtime dependency. Every number is determined by files already pinned in
`config/assets/bezels.json`: the Duimon and Soqueroeu layer PNGs and the
preset parameter chains. This milestone computes the dimensions from those
files, reviews them without any emulator, and keeps the emulator only for the
final visual check. Already in place from the earlier attempt and kept: the
`image` rectangle in the renderer, emitter and fast preview; the synthetic
core's flat-card and control-file modes; the gallery's verify mode.

**M11.1 Preset resolution.** `tools/bezel-dimensions.py` resolves each
package's upstream preset through its `#reference` chain (Duimon or Soqueroeu
tree, then the Mega Bezel base presets in the pinned shader tree), later
assignments overriding earlier ones, and produces the merged `HSM_*`
parameter set plus the layer image paths (BackgroundImage, DeviceImage,
DecalImage, CabinetGlassImage, TopLayerImage, LEDImage). Output:
`build/bezel-dimensions/<package>.params.json`. Done when every package
resolves with no missing file and the merged set is printed for inspection.

**M11.2 Placement math ported.** From Mega Bezel's own source
(`shaders/base/common/params-0-screen-scale.inc` for unit conversions,
`common-functions.inc` for screen scale, position and dual-screen offsets,
`params-4-image-layers.inc` for background and device layer placement) port
the closed-form placement to Python, in viewport units at the plate's aspect:
screen height from `HSM_NON_INTEGER_SCALE`, width from
`HSM_ASPECT_RATIO_MODE` (auto from the system aspect, explicit, 4:3, 3:2,
16:9, PAR from the native size), `HSM_SCREEN_POSITION_X/Y`, the
`HSM_DUALSCREEN_*` split and `HSM_2ND_SCREEN_*` scale, offset and crop, the
viewport flip, and for device plates the layer transform from
`HSM_DEVICE_SCALE`/`POS` with its follow-layer and scale-inherit modes and
`HSM_BG_FILL_MODE`. Scenes map viewport to plate 1:1; device plates map
through the device layer transform and the layer's alpha silhouette. Done
when unit tests reproduce, within 2 px at 3840x2160, the seven rectangles the
render-based run measured (nes, genesis, gc, dreamcast, gb, gbc, gba): those
captures are the ground truth for the port and the last thing RetroArch is
used for in this milestone.

**M11.3 Openings from the layers.** Lens windows are the alpha bounds of the
`*_Glass` layers (done for DMG, GBC, GBA). DS and 3DS windows are the two
largest rectangular regions on the `*_Decal` layers (new detector in
`tools/bezel-measure.py`; the vertical clamshells likewise). TV tubes are the
dark openings of the scene plates (done). Plates with no drawn opening (PSP
E1000) and scenes that paint their own tube use opening = image. Done when
every screen of every package has an opening with its method recorded.

**M11.4 Package emission.** The tool writes into `config/bezels/*/bezel.json`
per screen: `tube` (opening), `image`, the drawn inner ring from
`HSM_BZL_WIDTH/HEIGHT` and corner scales, the corner shape from the opening
mask, `surround` sampled from the plate between image and opening, and a
provenance block naming the preset, the parameters used, and the flip.
Recolor packages inherit from their base. The main (top) screen takes the
larger window when the two differ; the upper window when they match. Done
when `semu render-env` emits `SEMU_RENDER_SCREEN_<n>_IMAGE` for every
calibrated package and the contract tests assert image inside opening and
image aspect within 1 % of the system aspect for all of them.

**M11.5 Review without an emulator.** Two generated sheets: a dimensions
table (package, screen, canvas, opening, image, aspect, surround, preset,
flip, aspect mismatch) and an overlay sheet (each plate with the opening and
image outlined), both linked from the gallery page. Done when all 29
packages are on the sheets and inspected: image inside opening, aspect right,
main screen in the large window, nothing outside the plate.

**M11.6 Fast and real galleries, verified.** The fast gallery renders both
screen configurations from the packages in under a minute. The real gallery
runs on a worker pool (one RetroArch per private display, six at once) with
one launch per cell that also lights the flat card through the Semu renderer
and measures where it lands; pass is within 2 px of the package. Done when
both configurations render with zero verification failures and both sheets
are inspected.

**M11.7 Cleanup.** Delete `tools/bezel-calibrate.py` and the RetroArch
calibration path, update README and the memory notes, commit.

Order and expected cost: M11.1 and M11.2 are the work (a few hundred lines of
Python against Mega Bezel's source, half a day); M11.3 to M11.5 are an hour;
M11.6 runs in minutes on this machine. No emulator is launched before M11.6.

## Gap review (2026-09-22)

A full review of `src/`, `tests/`, `config/`, `packaging/` and a visual pass,
run on the Mac (mbp21, aarch64-darwin, macOS 27). FRACTAL-NORTH did not
resolve and no Deck was reachable, so nothing here was observed on either
production target. How it was run: the pinned BTRC compiler (c959dd0e) with
the host Python; the contracts compiled and run in the local podman Linux VM;
a scratch macOS `semu` (evdev headers stubbed, not committed) serving
`semu bezel edit` against the pinned nixpkgs `libretro-shaders-slang`, with
every package inspected on the editor's test card; the committed plates in
montages; `glslangValidator` on `config/render/compositor.frag`.

What held up: the contracts print `PASS: 371 checks`. The compositor GLSL
compiles. Every system's shader and bezel variant resolves to a package or
asset that exists. Both `input.json` files match the M8 vocabulary. The
editor puts the test card inside the opening for the DMG, GBC, GBA, DS, 3DS,
PSP and every Soqueroeu TV. Every CLI command PLAN names exists.

The gaps below are ordered by priority. Each has a done criterion, like a
milestone. Where a gap contradicts a Status line, that line is corrected
below.

### G1. The build and tests run on one host only (P0)

- `make build` fails on the Mac. The root flake lists only `x86_64-linux`
  (`flake.nix:47`), so the Makefile's `nix run .#btrcpy` fallback has
  nothing to run. The pinned `btrcpy`, built for darwin, aborts in libffi
  (`ffi_trampoline_table_alloc`) under nixpkgs Python 3.14 on macOS 27, and
  runs fine under the host's Python 3.13.
- Once transpiled, the C does not compile on macOS. `<linux/input.h>` and
  `<linux/uinput.h>` are included unconditionally (`src/launch/
  supervisor.btrc:23-24`, `process.btrc:16`). `check.btrc:61` also rejects
  every target whose platform is not `linux`.
- The `bezel` commands and the placement contract only work on a NixOS host
  with channels. `MegaBezelTree` (`src/bezel/preset.btrc:58`) finds the
  slang tree through `/run/current-system/sw/bin/semu`. It fetches the packs
  with `nix-build -E '(import <nixpkgs> {})…'` and discards stderr
  (`:76-79`), which is impure, not pinned, and slow: it is most of the
  contracts' 63 s.
- The owner's rulings conflict: the Vision names two Linux targets, while
  the memory notes name Deck plus Mac. **Ruling needed:** is macOS a
  development host only, or a product target? The answer decides whether
  the supervisor gets a platform gate or a macOS input backend.
- **Done when:** on this Mac, `make build && make test` passes (at least as
  a development host) with no BTRC on PATH. The placement and layer-stack
  contracts run in the `contracts` flake check from pinned inputs, with no
  `<nixpkgs>`, `/run/current-system` or network. `make test` takes under a
  minute (the M1 criterion; the run here took 63 s).

### G2. Tests that report PASS without testing (P0)

- None of the nine spec tests from the keep list run. `tests/spec/*.btrc`
  still use the old syntax (`import std.{…}`, `JsonValue`) and import
  `src/compiler/*` and `src/generators/*`, which no longer exist. Nothing
  imports them (`tests/contracts/main.btrc:9-23`), and the check copies only
  `src`, `tests/contracts` and `config` (`checks.nix:14`). This covers the
  gb 18-case matrix (named in M6), the Steam Input binding table,
  melonDS/Azahar touch, Cemu and the standalone contracts, plus the
  `tests/spec/retroarch/*.cfg` fixtures. **M1's "make test runs the ported
  contract tests" is therefore not met.**
- Two contracts skip and still count as passes. Layer stack:
  `main.btrc:363-366` passes when `layerCount == 0`. Bezel placement:
  `:444` adds `expect(true)` when the shader tree is missing. So the seven
  2 px ground-truth rectangles (M11.2) never run in any check.
  `BezelPackageContract` hardcodes `calibrated == 25` (`:429`).
- `NativeProfilesContract` (`main.btrc:177-201`) asserts keys for Dolphin
  and PCSX2 only. For PPSSPP, melonDS, Cemu, Flycast, Azahar and Ryujinx it
  checks only that the file is non-empty.
- Nothing tests the quit chord (either order, the 250 ms window) or the
  process-group kill. Only `chordWindowMs == 250` is asserted.
- `make nix-check` is `nix flake check --no-build` (`Makefile:48`), so it
  evaluates the checks but never runs them.
- There are no per-system integration tests (M3 and M4). The only one,
  `tests/integration/retroarch-headless.sh`, uses the synthetic core. It
  bypasses `semu launch`, the supervisor and the renderer, overrides
  `glcore`/`sdl2` with `gl`/`x`, never checks the exit code, and treats
  `mean > 0.05` as non-blank, which RetroArch's own menu can pass.
- `tests/visual/*` and `tests/deck/deploy.sh` need FRACTAL-NORTH (host
  ROMs, `semu` on PATH, Xvfb, IM6) and contain no assertions.
  `menu-e2e.sh` greps a log; it never checks the state file or the
  persisted `visual.bezels`.
- **Done when:** the spec tests are ported to the current BTRC and wired
  into `main.btrc`, or deleted with their assertions moved in. A skipped
  contract fails the run unless an explicit `SEMU_ALLOW_SKIP` is set.
  `make nix-check` builds the checks. There is one integration check per M3
  system through `semu launch` with a real core, asserting save/load, exit
  0 and no orphans. Quit-chord and process-group tests drive a fake child.

### G3. Renderer defects (P0 for hot reload, P1 otherwise)

- **Hot reload breaks every texture but the game.** Confirmed in code:
  `relinkIfChanged` (`renderer_compositor.btrc:1065-1093`) links the new
  program and looks the uniforms up again, but sets sampler units only in
  `initialize` (`:1190-1196`), and the GLSL has no `binding=` qualifiers.
  After the first successful edit, every sampler reads unit 0. The reload
  also runs every 60 frames rather than every second, and a compositor file
  that is broken at launch has no built-in fallback.
- If the game phase fails, the Semu menu goes with it
  (`libsemurenderer.btrc:197`, `:221-224`). A missing layer, a bad preset,
  or turning SHADER ON for a system with no preset removes the menu and its
  toggles.
- A preset that fails is recompiled every frame (`:1362-1374`). A failed
  `initialize` rereads both GLSL files every frame. `layer_textures[32]`
  leaks on a context change (a GBA layer is about 45 MB). The compositor
  source buffers leak on each reset (`:924`, `:940`).
- The GL state guard saves texture units 0-6 only. It does not save units 7
  and up, indexed UBO bindings (librashader and PCSX2 both use them), or
  the clear colour (`renderer_gl_api.btrc:468-548`). Needs a live check.
- Placement uses fixed fractions, against the standing ruling ("largest
  integer native scale vs the actual screen"):
  - fixed bezels are contain-fitted fractionally with integer scaling
    forced off (`:826`, `:845`)
  - the dual-layout gap is `areaWidth/64` (`:755`)
  - the frame width is a fraction of screen height (`:819`)
  - the inset is multiplied by 0.45 (`:376`)
  - the widescreen switch is fixed at aspect 1.55 (`:530`)
  - the menu is 2/5 of the screen width from a 384x468 texture scaled
    LINEAR, so its text is blurry (`renderer_post_ui.btrc:506-513`)
- Plates up to 4400 px are drawn with LINEAR and no mipmaps
  (`:969-984`), so they alias on the Deck's 1280x800. All four variants'
  plates load synchronously on the render thread.
- The preload shim assumes the emulator letterboxes the game centred at
  `SEMU_RENDER_ASPECT`, and reports that aspect as the live one, so the
  widescreen switch never fires. Its `context_generation` is always 1.
  PCSX2 reads back its already-stretched output and scales it again.
- The emitter and the renderer disagree on the environment.
  `SEMU_RENDER_BEZEL_ID` is emitted but never read. `SEMU_RENDER_FPS`,
  `SEMU_RENDER_FRAME_DELTA_MS` and variants `_C`/`_D` are read but never
  emitted. The rejected reflection "reach" survives as dead code
  (`uRingReach`, REFLECT fields 4-5, `editor.btrc:111`).
- The emulators link the renderer directly, so any edit under
  `src/renderer` rebuilds RetroArch and PCSX2. Root `emulator-pcsx2` does
  not follow `renderer` (`flake.nix:26`), so PCSX2 links a second
  libsemurenderer built with BTRC fad4b470.
- **Done when:** a compositor edit during a live game keeps bezel, glass,
  background, second screen and menu correct (capture inspected). Fault
  injection (a missing layer, a bad preset, a broken compositor file) keeps
  the menu and logs once. The listed fractions are replaced by
  integer-scale rules or settings. Plates are mipmapped. Every emitted
  `SEMU_RENDER_*` has a reader and vice versa, enforced by a contract. The
  renderer is loaded with dlopen against the ABI header, and PCSX2 follows
  the root renderer.

### G4. Bezel look and variants (P0: users see this)

- **Recolour alternates render as their defaults.** In the editor,
  `gba-arctic` showed the indigo GBA, `gbc-berry` the teal GBC and
  `psp-red` the black PSP. `semu render-env --system gba` emits the same
  seven upstream `SEMU_RENDER_LAYER_*` for `arctic` as for `shell`, and in
  layered mode the renderer ignores `SEMU_RENDER_ART`. The recoloured
  plates (`config/assets/bezels/{gba/arctic,gbc/berry,psp/red}.png`) look
  right on their own, but they are only drawn when the layers do not
  resolve. So a repo launch and an installed-bundle launch show different
  art. This violates "no fake variants".
- **Layered TV scenes lose the night lighting.** The committed Soqueroeu
  plates (`tv/soqueroeu/*.png`) are night-lit and vignetted, which is the
  M10 look. The layer stack draws the flat, daylight upstream background
  for nes, psx, wii-16x9 and genesis (seen in the editor), because Mega
  Bezel's night lighting is a shader texture, not a layer. **Ruling
  needed:** bake night lighting and recolours into layers (for example a
  `lighting`/`recolor` pass over the layer stack), or keep flat plates for
  those packages.
- **M9 is no longer met.** Since the crt-premium and crt-silver deletion,
  dreamcast, gc, genesis, n64, nes, ps2, psx and snes have one bezel
  variant each. Wii's two (`tv`, `tv_wide`) are the widescreen switch, not
  an alternate. No `bezels.json` has a per-system disabled variant; only
  the global `visual.bezels` switch exists. `settings_ui.btrc:88` hides the
  bezel picker below two variants.
- The layer files resolve only through `<repo>/build/bezel/shaders`
  (`rendering.btrc:46`), so an installed bundle never gets layers. This was
  already open in Status.
- Package geometry is computed at a fixed 3840x2160 viewport
  (`package.btrc:187`). The Deck's 16:10 screen gets 16:9 placement.
- The slang tree is nixpkgs `libretro-shaders-slang` used as-is
  (`config/assets/shaders.json:9`). The Mega Bezel `HSM_*` defaults, and
  with them the M11 geometry, move whenever nixpkgs moves.
- Suspected bug: `placement.btrc:303` has `resX = resX * (…)` (pixel²) on
  the dual-mode-2 integer path, which none of the seven captures covers.
- Package count: 26 on disk, 22 in the editor (the upstream and inheriting
  ones), 25 in the contract. PLAN says 29 and "3 Semu CRTs".
- M11.5 and M11.6 are not done. The dimensions and overlay sheets have no
  generator. `tools/bezel_fake.py` does not know `LAYER_*`, `CANVAS`,
  `RING` or `REFLECT`, and is Python (G8).
- Editor: it opens unfitted (at 12.5 %, Fit needed). `/api/save` checks no
  Origin or Host (any page in the browser can rewrite packages). Save
  reports success when the write fails (`editor.btrc:102`,
  `package.btrc:146`).
- **Done when:** every alternate is visibly distinct from its default in
  both the repo and installed-bundle launches (captures inspected side by
  side). The TV scenes look the same layered as the committed plates, or
  the owner rules otherwise. Every non-modern system again has disabled,
  default and one materially different alternate (M9). Layers ship in the
  asset tree from a pinned slang tree. Geometry follows the actual output
  aspect, with Deck and 4K cells both inspected. The M11.5 sheets exist in
  BTRC.

### G5. Launch, input, settings, owned paths (P1)

- The quit chord is hardcoded to `BTN_START`/`BTN_SELECT`
  (`supervisor.btrc:587-590`); `SemuInput.quitChord()` (`model.btrc:148`)
  is never read. `SemuQuitChord`'s pad logic in `process.btrc:128-248` is
  dead.
- Quit is SIGTERM then SIGKILL after 250 ms, RetroArch included, although
  its UDP `QUIT` is available (`process.btrc:114-125`). SRAM, memory-card
  and NAND flushes can be cut off.
- When the direct child exits normally, the rest of its process group keeps
  running (`supervisor.btrc:628`), against M3's "no orphans".
- Slot next and previous are journalled as `state.load`
  (`supervisor.btrc:315`). Device slots are never reused after unplug
  (`:654`), so after 64 plug events no new device is accepted.
- `settings put` over a malformed `semu.json` replaces it with `{}` and
  writes that out, losing the user's settings (`lib/settings.btrc:24`). A
  malformed `overrides/*.json` is ignored with no diagnostic
  (`resolve.btrc:67-70,88`).
- The owned-paths boundary is not what PLAN carried over. `inside()`
  compares string prefixes without normalizing `..` (`lib/paths.btrc:16,36`),
  and there is no deny list for `src/`/`config/`. `bezel emit` and editor
  Save write `config/bezels/*` directly. **Ruling needed:** is `config/`
  writable by `semu bezel`, as M11.4 requires?
- Steam Input icons are copied with `readText`/`writeText`
  (`steam/input_template.btrc:355`), which truncates PNGs at the first NUL.
  `shortcuts.vdf` is rewritten in place with no temp file and rename
  (`steam/shortcuts.btrc:60`).
- `sync stop` kills the process group of a PID from a pidfile without
  checking it is Syncthing. Peer ids go into the XML unescaped
  (`sync/syncthing.btrc:46-57,208`).
- Hardcoded ids in BTRC, against "no system id in BTRC":
  - `settings_ui.btrc:105` (`dolphin`) and `:138` (`input.systems.wii…`)
  - pack names duplicated in `rendering.btrc:30-34` and `preset.btrc:82-88`
  - the RetroArch port 55355 in `plan.btrc:166`
  - the X11 `dolphin_pointer` in `input.json`
- `supervisor.btrc` is 677 lines, over the roughly 500-line limit. Six
  files still have block comments, and `placement.btrc` uses one- and
  two-letter names (the style directive).
- **Done when:** the quit chord comes from config and is contract-tested.
  RetroArch quits through `QUIT` before any signal. There are no orphans
  after a normal exit. Settings writes never destroy unparseable input.
  Icons copy byte-for-byte. Shortcuts are written atomically. The
  owned-paths rule is decided and enforced in one helper.

### G6. Packaging and directive compliance (P1)

- The root nixpkgs tracks `nixos-unstable` (e554fab, 2026-09-17); M1 says a
  release branch. The ES-DE flake's nixpkgs is ac62194 (2026-01-02) and
  allows insecure FreeImage. Pins M4 would refresh: Cemu v2.6 (2025-02),
  Ryujinx 1.3.3 (2025-10), ES-DE (2025-11), RetroArch 1.22.2 (2025-11),
  PCSX2 v2.6.3 (2026-01). The sub-flake locks still pin BTRC fad4b470.
- nixpkgs packages shipped as-is, which the directive calls a regression:
  `librashader`, `syncthing`, `retroarch-joypad-autoconfig` and the slang
  shader tree.
- The `emulator.json` platform slices contradict the flakes:
  - RetroArch has a macOS slice, but its flake says `macos = false`.
  - Azahar, Flycast and melonDS flakes build for macOS, but their
    `emulator.json` has no macOS slice.
  - No macOS target exists.
- Leftovers from the delete list:
  - `gl_wrapper: ${asset_root}/bin/nixGL` in all 8 standalone
    `emulator.json` files
  - `config/settings/install.json`, which names the AppImage, defaults to
    `steam-deck`, and is read by nothing
  - AppImage `doc` strings
  - `aspect_probe`
  - the `compiler.{bindings,line_groups}` DSL. The keep list says drop it,
    but `emit/profiles.btrc:184,225` needs it. Decide which is right.
- The keep list names files that are gone: `packaging/nix/renderer.nix`,
  `render-hook.nix` and `semu_app.nix`. `config/assets/bezels.nix` cites a
  `nix run .#bake-bezels` app that does not exist.
  `pcsx2/system-cubeb.patch` is declared and never applied.
  `assets/shaders/fallback/crt.slangp` is referenced by nothing.
  `sharp`/`screen` naming is reversed in gc, dreamcast, ps2 and wii. The two
  target files and the two `input.json` files are full copies, not
  inheritance.
- **Done when:** the `platform-matrix` check also asserts that each
  `emulator.json` slice matches its flake's platforms. Nothing on the
  delete list is left. Every pin is current or noted as held back, with a
  reason.

### G7. Art licence (P0 before any release)

The Duimon and Soqueroeu art is CC-BY-NC-ND and ships in the repo and in
the `Semu-x86_64.tar.zst` release. The repo records no licence or
attribution for it (`config/assets/bezels.json` provenance has no licence
field). The recolours and night-lit scenes are derivatives, which ND
forbids distributing. **Done when:** a NOTICE carries the attribution and
licence for each pack, and the owner has ruled on shipping derivatives
(bake them at install time from the pinned upstream, or drop them).

### G8. Python in the tree (P1, standing rule)

The rule is no Python anywhere. Still present:

- `tools/bezel-gallery.py`, `tools/bezel_fake.py`, `tools/bezel_measure.py`
  and `tools/bezel-measure.py`
- `tests/visual/virtualpad.py`, run through `nix-shell -p
  python3Packages.evdev`
- `tests/integration/retroarch-headless.sh:8-17`, which embeds `python3`
  for UDP, with `pkgs.python3` in `checks.nix:51`
- the Status line "an independent Python parse" (M7c)
- M10 and M11 prose, and README lines 131-133, which still point at the
  Python tools

M11.7 is not done: `tools/bezel-calibrate.py` is gone, but PLAN still cites
`tools/bezel-dimensions.py`, which does not exist. **Done when:** `git
ls-files | grep '\.py$'` and a grep for `python` over `tests/`, `tools/`
and `packaging/` come back empty. The gallery, the virtual pad (the
supervisor already builds uinput devices) and the UDP client are BTRC.

### G9. Observation still missing (carried, needs hardware)

- On FRACTAL-NORTH: Xbox pad input, Start+Select on hardware, save and load
  by pad, reboot survival (M2 to M4). Captures of wii, ps2 and n3ds (M9). A
  16:9 title for the widescreen switch. Azahar and Cemu GL after a reboot.
  Steam launching the shortcut.
- On the Deck: everything in M5, plus the Deck halves of M6 and M8.
- The screenshots Status cites are not in the repo or in this checkout's
  `build/`. The verification rule wants them retained: keep inspected
  captures under `build/verification/<host>/<date>/` and commit a small
  index (host, revision, file, verdict).

## Verification rules

- A contract test asserts exact generated bytes or keys against the
  spec-bearing fixtures. An integration test launches a real emulator. A
  milestone needs both where the emulator can run headless.
- Never mark a criterion met from a passing fixture, a package build, or an
  uninspected screenshot. Say what was observed and on which host.
- Before any deploy to the Deck, the same revision's desktop tests must pass.
- Commit only coherent passing checkpoints. Push after each milestone.

## Status

Update this block whenever a milestone criterion changes state.

- M1 reset: partly met, see G1 and G2 (2026-09-22). Was: done 2026-09-19 (`make build && make test` pass in about ten
  seconds with 79 checks; `build configs --target linux-desktop` emits ES-DE
  and RetroArch files with no Deck path; `nix flake check --no-build` passes;
  commit eb909a7)
- M2 Game Boy on desktop: verified 2026-09-19 except the reboot, which the
  unattended session could not perform without killing itself. Observed on
  FRACTAL-NORTH: `nixos-rebuild switch` installed the bundle (semu, semu-es-de,
  semu-retroarch on PATH, semu.desktop in the app menu, persist mounts for
  ~/ES-DE, ~/.config/semu, ~/.local/share/semu); `semu launch retroarch
  --system gb` showed Tetris fullscreen (screenshot inspected); ES-DE started
  from the bundle with all 17 systems and the existing gamelists and media, no
  warnings; killing the launcher stops RetroArch; SAVE_STATE over the command
  port wrote `Semu/states/Gambatte/Tetris (World) (Rev 1).state` and
  LOAD_STATE left RetroArch playing. Not yet observed: Xbox pad input and
  Start+Select quit on hardware (no controller was connected during the
  session), and survival across a reboot.
- M3 RetroArch systems on desktop: 11 of 11 launched on FRACTAL-NORTH on
  2026-09-19 with an inspected screenshot each (gb, gbc, gba, nes, snes,
  genesis, n64, psx, nds, psp, dreamcast; psx needs about 20 s to boot; psp
  needs the vulkan per-core override because the PPSSPP core segfaults under
  glcore). `nix flake check` runs the contract tests and a sandboxed headless
  RetroArch run (Xvfb, llvmpipe, synthetic core) that must answer VERSION,
  write a non-blank screenshot and exit on QUIT. Not yet observed: pad input,
  save and load, and the Start+Select chord on hardware (no controller was
  connected). RetroArch 1.22 segfaults on GET_STATUS when the loaded core has
  no core-info entry, so the check uses VERSION.
- M4 native emulators on desktop: 8 of 8 launched a real ROM fullscreen on
  FRACTAL-NORTH on 2026-09-19 with an inspected screenshot each (dolphin gc
  and wii, pcsx2, ppsspp, flycast, melonds, azahar, cemu, ryujinx). All come
  from nixpkgs through thin `config/emulators/<id>/package.nix` recipes.
  Findings: Azahar takes `-f`; melonDS's GL display path leaves its window
  unmapped on Wayland+NVIDIA so it runs the software renderer; Ryujinx needs
  the firmware seeded from the existing `Ryujinx/config/bis` tree and then
  reaches the controller applet (no pad connected). Existing saves are
  copied once into the state root on first launch (GameCube memory cards,
  Wii NAND, PS2 memory cards and states, Cemu mlc01, Azahar nand and sdmc). Not yet
  observed: pad input, save and load, Start+Select on hardware. Pushes to
  GitHub are blocked behind a 1Password SSH authorization prompt on screen;
  the host was switched with `--override-input semu git+file://...`.
- M5 Steam Deck: delivery built and verified on the desktop, 0 of 17 systems
  accepted on hardware (no Deck was reachable from the session on
  2026-09-19). `nix build .#release` produces `Semu-x86_64.tar.zst` (about
  1.3 GB, the bundle's whole closure), its digest and `install.sh`. The
  installer's digest gate, digest-named releases, stable launchers, previous
  release, rollback and pruning are covered by the `installer` flake check.
  On FRACTAL-NORTH the installed release ran the CLI and launched Tetris
  through the bubblewrap launcher with the release mounted at /nix (screenshot
  inspected; the host's GPU driver closure is bound in because NixOS keeps it
  under its own store). `tests/deck/deploy.sh` copies, installs, prepares and
  screenshots over SSH once `DECK_HOST` is set. Steam Input publication and
  Steam shortcuts are not done.
- M6 renderer: done on the desktop 2026-09-19, Deck pending. `libsemurenderer`
  (librashader GL) is linked into RetroArch's gl3 driver by the Semu build;
  the launcher passes `SEMU_RENDER_*` from `config/systems/<id>/{shaders,bezels}.json`.
  Observed on FRACTAL-NORTH through the pure Nix bundle with inspected
  screenshots: Game Boy Tetris inside the DMG bezel with the DMG shader,
  `semu settings put visual.systems.gb.bezel_variant studio` plus
  `shader_variant pocket` switched to the alternate look, and
  `visual.bezels false` plus `visual.crt_shaders false` gave the raw frame,
  all without a rebuild. NES, SNES, N64 and PSX render inside the CRT bezel,
  NDS renders both screens inside the DS shell. Native emulators: the
  bundle ships `libsemupreload.so`, an LD_PRELOAD shim that composes through
  the same renderer at SDL_GL_SwapWindow, eglSwapBuffers and glXSwapBuffers
  (`render_preload` in emulator.json adds it to the launch environment).
  Observed with inspected screenshots: Flycast (Crazy Taxi), PPSSPP (Ape
  Escape) and Dolphin (Super Monkey Ball) draw inside their bezels. PCSX2
  and melonDS never reach an interposable swap (their frames stayed raw), so
  they are not opted in. Azahar and Cemu could not be judged: by the end of
  the session Azahar reported "OpenGL shared contexts are not supported" and
  Cemu showed a white window even when launched directly from the bundle
  without Semu, with the same packages that ran fine during M4; treat that
  as machine state to recheck after a reboot before blaming Semu. Ryujinx is
  Vulkan and stays unhooked. n3ds moved to RetroArch's citra core (nixpkgs
  builds libretro/citra; the Azahar core exists upstream but is not
  packaged) with Azahar as the alternate: BOXBOY! renders both screens
  inside the 3DS shell (screenshot inspected). glcore now hands
  core-profile cores a 4.6 context because Citra's version-gated GL loader
  crashed on a null 4.x entry point in a 3.3 context; N64 re-verified under
  it. Standalone emulators that remain required: Cemu (Wii U) and Ryujinx
  (Switch) have no libretro core at all; Dolphin (GC/Wii) and PCSX2 (PS2)
  keep their standalone builds because the nixpkgs libretro forks of both
  trail upstream. Not done: the Deck half of the criterion.
- M6 look (2026-09-19 evening): audited after the first captures looked
  flat. Three defects fixed in one pass: CRT systems declared a single
  `screen` preset the launcher never read (no shader at all on NES, SNES,
  N64, PSX, Genesis, Dreamcast), the hole fit applied integer scaling so the
  game sat inside the bezel with black margins, and the composite shader had
  no curvature, corner mask, vignette, bloom or halo although every
  `system.json` declared them. Now the launcher passes `corner_radius`,
  `curvature`, `glow`, `bloom` and `vignette`, a framed screen fills its
  hole fractionally, the shader curves the tube (edge midpoints stay on the
  hole edge, corners recede), rounds the corners over the bezel art, adds
  vignette and a bloom on highlights, and screens a blurred halo of the game
  onto the bezel around the hole. Verified headlessly with
  `tests/visual/capture.sh` (Xvfb, real GPU): NES with NTSC composite, SNES,
  N64, PSX, Game Boy DMG, DS dual-screen and Dreamcast through the preload
  all inspected.
  ES-DE settings menu: ES-DE 3.4.0 carries the settings-menu patch; the
  SEMU SETTINGS entry renders the document `semu settings ui` returns and
  saves through `semu settings put`. Observed: virtual-keyboard drive of the
  real ES-DE through MAIN MENU > SEMU SETTINGS > VISUALS, toggling BEZELS and
  backing out wrote `{"visual":{"bezels":false}}` to `~/.config/semu/semu.json`
  (five inspected screenshots). Syncthing: the bundle ships syncthing 2.1.3;
  `semu sync start` generated a Semu-owned home under the state root, its
  REST API reported the `semu-saves` folder at `~/Games/Emulation/Semu` and
  the same device id `semu sync device-id` prints; a second generated
  identity pasted into `sync.peer_device_ids` appeared as a device and as a
  folder member after restart; `semu sync stop` ends the whole session. The
  ES-DE launcher starts sync when enabled and NixOS adds the `semu-sync` user
  service. Steam shortcuts: `semu steam shortcuts` rewrites
  `userdata/<id>/config/shortcuts.vdf` (binary VDF walked entry by entry,
  foreign entries kept byte for byte, Steam's crc32 appid) and printed the
  `steam://rungameid/` URL against a scratch copy of the real profile; an
  independent Python parse read the entry back. Not observed: Steam itself
  launching the shortcut (Steam was running, so the live profile was left
  alone), and anything on a Deck.
- M8 unified input and native menu: done on the desktop 2026-09-19 (late).
  The supervisor in `src/launch/supervisor.btrc` reads gamepads and
  keyboards through evdev, turns Select-modifier chords and keyboard chords
  into the one action vocabulary, writes the renderer's action journal,
  executes RetroArch actions over its command port and types the chord into
  native emulators through a virtual keyboard. Observed headlessly
  (`tests/visual/menu-e2e.sh`, virtual gamepad, real RetroArch, captures
  inspected): Select+Y opened the Semu menu over the paused Game Boy, D-pad
  plus A saved a state (RetroArch's own toast and the state file), BEZEL
  ON/OFF removed the bezel live and persisted `visual.bezels=false`,
  Start+Select ended the session. Flycast: Select+R1 routed to a typed
  Ctrl+S and Flycast's compiled binding wrote its state file when the chord
  arrived (the virtual-keyboard hop itself cannot reach an app on bare Xvfb,
  so that last hop is proven only by the route log). `semu steam input`
  renders the Deck's Neptune templates (gamepad set, hotkey set, quick and
  menu trackpad radials with semantic icons, Wii controller layer) and
  copies the icons, covered by a contract test; not yet loaded on a Deck.
- M9 bezel and shader fidelity: REOPENED 2026-09-22 (G4). Was done on the desktop 2026-09-19 (late) for
  every capturable non-modern system. gb, gbc, gba, nes, snes, genesis,
  n64, psx, nds, psp, dreamcast, gc, wii, ps2 and n3ds each declare a
  default and a materially different alternate for both shader and bezel
  (Sharp CRT via easymode-halation for the CRT systems; authentic GBC,
  AGB-001, LCD 3x and LCD-grid presets for handhelds; silver 4:3 CRT, berry
  GBC, arctic GBA and deep-red PSP shells from the recolor recipes in
  `config/assets/bezels.json`, committed with output hashes).
  `tests/visual/matrix.sh` captured default, alternate shader, alternate
  bezel and disabled for 12 systems (48 cells, sheets inspected): distinct
  and correctly framed. Caveats: Flycast keeps a 640x480 window on bare
  Xvfb so its CRT mask aliases there (its fullscreen desktop run was
  inspected earlier); Dolphin's disabled cell caught a white transition
  frame; wii, ps2 and n3ds were not captured (no Wii title run, PCSX2
  unhooked, Azahar broken this session); the widescreen switch (variant B
  above 1.55) is implemented but not exercised with a 16:9 title.
- M10 bezel packages (2026-09-19 afternoon): 29 packages under
  `config/bezels` (10 Soqueroeu living-room TVs lit for night with the
  manufacturer badges, 3 Semu CRTs, DMG-01 / GBC / AGB-001 Duimon shells with
  their glass lenses, PSP E1000 with a drawn frame, 4 computed dual layouts,
  DS and 3DS Duimon shells plus the vertical shells). Every screen opening is
  measured by `tools/bezel-measure.py` and recorded with its method. The
  compositor takes per-screen tube, shape (rounded / squircle), fit, inset,
  surround, curvature, vignette, bloom, glow, glass and shader; draw order is
  background plate, chrome (art, drawn frames, glow, masked out of the tubes),
  game, menu. Headless captures (Xvfb, real GPU) inspected for nes, snes,
  genesis, n64, psx, gb, gbc, gba, nds, n3ds, psp, gc, wii, dreamcast and ps2:
  the game sits inside the measured tube at the system aspect on every TV
  scene, the handheld LCDs sit inside their lenses on the desk, the 3DS runs
  Semu's Azahar libretro core with its top screen centered at 2x and the
  touch screen at 1x beside it (both framed), PCSX2 is hooked directly through
  `semu_render_hook.patch` and shows in the Sony TV, and Cemu renders once
  pinned to XWayland (its GTK Wayland backend is the white window). Fixed on
  the way: the GL state guard overflowed when the texture-unit count grew
  (every frame black), the melonDS core needed a non-executable stack
  (`-z noexecstack`) before glibc would dlopen it, and high-resolution emulators are blitted with linear
  filtering before the shader chain. The DS runs the libretro melonDS build in the main-right
  layout (main screen at 3x, touch screen at 1x beside it, matte preset).
  Known limits: Flycast on bare Xvfb keeps
  a 640x480 window so its CRT mask aliases there; Wii U stays unframed
  (Cemu is unhooked); the PSP panel is placed from the device's physical
  proportions because the Duimon art has no drawn screen edge.
- Bezel gallery and closed loop (2026-09-19 evening): `tools/bezel-gallery.py`
  renders all 63 variant cells per screen configuration (Deck 1280x800, PC 4K
  3840x2160) in two modes: fast previews from `tools/bezel_fake.py` (the
  compositor's geometry in numpy from `semu render-env`, 63 cells in under
  30 s) and real renders through RetroArch plus the surface-aware synthetic
  test-card core. The RetroArch bridge now takes only the narrower screen's
  columns from a stacked dual frame (the 3DS bottom screen lost its side
  bars). DS and 3DS default to the Duimon shells with an inset drawn bezel
  around the fitted 4:3 game (`frame.around = "game"`); the computed layouts
  stay as alternates, prefer the larger main screen (centered main when it
  fits, else the pair centered) and use thinner frames. Every Deck and 4K
  preview was inspected; real renders confirm the same geometry.
- Calibration (2026-09-19 night): the user found the picture placement wrong
  in most packages; the openings were measured but the picture inside them
  was guessed. `tools/bezel-calibrate.py` now renders each package's upstream
  preset (Soqueroeu TV scenes, Duimon device presets, Standard tier) in real
  RetroArch at 3840x2160 with the synthetic core lighting one flat screen at
  a time, diffs dark and lit frames (scanline gaps closed), detects flipped
  viewports against the plate, maps device plates through their silhouette
  over a flat floor, and writes `screens[].image` (plate pixels) plus the
  measured surround color. Scenes use the image as their opening; device
  shells keep the lens as the opening. The DS and 3DS faces are Duimon's
  device+decal plates (two equal windows), and the main screen takes the
  larger window. `tools/bezel-gallery.py --mode real --verify` lights the
  flat card in the Semu renderer and checks the picture lands within 2 px of
  the package geometry; the page shows the upstream render beside each cell.
- M11 dimensions from the files (2026-09-19/20, rewritten in BTRC after the
  first Python pass): `src/bezel/preset.btrc` resolves a package's upstream
  preset the way RetroArch does (every `#reference` chain first, the
  referencing file wins, the first assignment inside one file wins: RetroArch
  1.22.2's `config_file` keeps the first key, which is why the GameCube preset
  scales at 64.37 and not its later 64.97), reads every `#pragma parameter`
  default from the pinned shader tree and makes the textures absolute.
  `src/bezel/placement.btrc` ports the closed-form placement (screen scale,
  crop, aspect table, tube and black-edge scales, bezel edge, position
  offsets, dual-screen split, layer transforms, zoom, pan and flips). The
  seven render-based captures are the contract (`BezelPlacementContract`,
  worst edge 1.1 px; scenes measured at the 10 % lit-difference crossing,
  LCD shells at the 55 % solid region because their bezel lights up too).
  `src/bezel/package.btrc` turns each package into Photoshop-style layers:
  every texture the preset draws (background, device, decal, glass, top,
  LED) with its Mega Bezel `LAYER_ORDER`, visibility (opacity > 0) and
  placement rectangle, plus the `screens` layer; `canvas_layer` names the
  layer whose pixel grid every rectangle is addressed in (device for the
  shells, background for the TV scenes). Per screen: `image` (picture),
  `ring` (black-edge outer to bezel outer, corner radii, the bezel colour
  from `HSM_BZL_COLOR_*`, `#171819` for the NES which matches the dark
  capture), `tube` (a measured hole or lens is kept when it holds the drawn
  bezel; otherwise the bezel's outer edge), `aspect`, `shape`, provenance
  with the parameters used. `semu bezel resolve|emit` write all 21 upstream
  and inheriting packages in half a second; `semu bezel edit` serves the
  editor (`config/editor/bezel-editor.html`) on loopback: layers listed top
  first with visibility, reorder and the canvas radio, every rectangle
  grabbable with handles at integer zooms (nearest-neighbour, pixel grid at
  8x), a Slides-style diamond that drags the corner radius (round or
  squircle exponent), `+ picture`/`+ ring` for plain plates, and Save writes
  the package (recolors follow). The renderer draws the ring
  (`SEMU_RENDER_SCREEN_<n>_RING`, compositor pass 1, fast preview too); the
  bundle that carries it is not rebuilt yet. Two BTRC stdlib additions
  landed upstream for this (`schiffy91/btrc` c959dd0e, pinned in
  `flake.lock`): `HTTPResponse.bytes` with `HTTPServer.respond` sending
  binary bodies, and `FileSystem.readBytes`; the system `btrcpy` on PATH
  predates them, so build with the flake's compiler (`nix run .#btrcpy`) or
  a checkout wrapper. Not done: baking the `art` plate from the layer stack
  (the renderer still draws the committed plate, so changing the canvas
  layer in the editor re-addresses rectangles without re-rendering art),
  the fast/real galleries (M11.6) and the glass assets, which are whole
  layers squeezed into the lens by the renderer and need a `crop` in their
  recipes.
- Layer stack in the renderer (2026-09-20): the compositor draws the
  package's layers itself (`SEMU_RENDER_CANVAS`, `SEMU_RENDER_LAYER_<n>` =
  file, extent or cover, above/below the screens, blend, opacity; up to
  eight per variant, JPEG and PNG) with Mega Bezel's blend modes (LED and
  device-LED layers are additive black plates, glass and top plates normal
  alpha), and in layered mode paints only the black edge, ring and picture
  so the plate art shows through the lens. Verified with headless RetroArch
  captures of the GBA, Game Boy and NES through the rebuilt bundle (`art=0`
  in the renderer's debug line, shells and TV scene composed from the
  upstream layers). The editor composites with the same blend modes,
  covers the canvas with viewport-following plates, draws canvas-sized
  plates at the canvas, exposes per-axis edge bulge handles (the opening's
  sides bow out like a tube; `shape.bulge` in px, the ring follows) and
  design-tool cursors. Editor pass (2026-09-20): the `cutouts` layer (was
  `screens`) fills ring, black edge and picture through the same rounded and
  bulged outline the handles edit, X-ray dims it while aligning, right-click
  menus edit a layer (visibility, order, canvas, blend, opacity, ring and
  surround colours, add picture or ring, reset), undo/redo covers every
  edit, the wheel pans (horizontal deltas sideways) and ctrl/alt+wheel zooms
  at the cursor, and the layout is toolbar, layers with thumbnails and
  badges, canvas, inspector. Later the same day: the sidebar became an
  inspector (Position/Size pills, chips for the four cutout rectangles, one
  Curve block, a Preview opacity slider next to the Ring and Edge colours,
  a `?` shortcuts popover), clicking a plate on the canvas selects it
  (alpha-tested, additive black ignored), orange bulge diamonds sit on all
  four edges of the opening and both ring rectangles, and packages carry
  `ring.reflection {strength, blur, fade}` from `HSM_REFLECT_GLOBAL_AMOUNT`,
  `HSM_REFLECT_BLUR_MAX` and `HSM_REFLECT_FADE_AMOUNT`: the renderer mirrors
  the picture across its edges onto the ring (`SEMU_RENDER_SCREEN_<n>_REFLECT`,
  screen blend, blur and fade growing with distance) and the editor previews
  it with the test card. Reflection then moved from the ring to the cutout
  (`screens[].reflection`) and only ever lands on a bezel band: the ring,
  which may be unfilled (`ring.color: none`) to mark a painted bezel on a
  plate such as gb-studio; the chrome pass reflects the part of the band
  outside the opening. A reach past the bezel and a softer direct-plus-diffuse
  model were tried on 2026-09-20 and rejected by the user; the band-only
  mirror (strength = the preset's global amount) stays, so the narrow-band
  Soqueroeu scenes (PSX, Wii, N64, Dreamcast) reflect faintly by design and
  the Strength slider and the bezel outer handles are the knobs. The
  compositor GLSL is data since 2026-09-20:
  `config/render/compositor.{vert,frag}` (the launch sets
  `SEMU_RENDER_COMPOSITOR_DIR`), read by the renderer at start and re-read
  once a second while a game runs, so a shader edit shows in the running
  game without rebuilding anything; a broken edit keeps the last good
  program and logs. Still to do: the emulators link libsemurenderer
  directly (PCSX2's and RetroArch's flakes take src/renderer as an input),
  so renderer code changes rebuild PCSX2; dlopen against the ABI header
  would end that. The crt-premium / crt-silver plates were deleted on
  2026-09-20 (the user did not want them; every TV system has its Soqueroeu
  scene, Switch and Wii U stay bezel-free by design). Cutout handles became zones: the knob at a corner or
  edge middle sizes (Shift keeps the ratio), and just beyond it the same
  corner rounds (yellow diamond, drag toward the centre) or the same edge
  bows (orange diamond, drag outward). Still open: bundling the upstream layer files into
  the asset tree (today they resolve through `build/bezel/shaders`, so only
  a launch from the repository checkout reaches them; the installed bundle
  falls back to the flat plate), the DS/3DS/PSP captures, the fast/real
  galleries, and the glass-asset crop.
- Active milestone: close the P0 gaps from the 2026-09-22 review first, in
  this order:
  1. G1: build and test on the Mac and in the checks from pinned inputs.
  2. G2: tests that actually run, and no silent skips.
  3. G3: the hot-reload sampler bug.
  4. G4: variants that differ, the TV night look, and the M9 alternates.
  5. G7: the art licence.
  Then return to the old order: M11 layer bundling into the asset tree
  and galleries, then M5/M6 hardware acceptance once a Deck is reachable
  (`DECK_HOST=deck tests/deck/deploy.sh install`), then the native-emulator
  render hook. Three rulings are needed from the owner:
  - macOS as a target or a development host only (G1)
  - how recolours and night lighting reach the layer stack (G4)
  - whether `semu bezel` may write `config/` (G5)
- Review 2026-09-22 (Mac, no production host reachable): see *Gap review*
  above. The contracts compiled in a Linux VM print `PASS: 371 checks` in
  63 s, but the placement and layer-stack contracts skip and count as passes,
  and none of the nine `tests/spec` files is compiled. The corrections to the
  lines above are:
  - M1 is only partly met: the spec tests are not ported, the run is over a
    minute, and `make build` fails on macOS.
  - M4: the emulators are now pinned-source flakes that `overrideAttrs` the
    nixpkgs recipes, not thin `package.nix` recipes.
  - M9 is reopened: 8 TV systems lost their bezel alternate with the
    crt-silver deletion, and the recolour alternates render as their
    defaults in layered mode.
  - M10 count: 26 packages on disk, not 29, and no Semu CRTs are left.
  - Build structure: 24 flakes (13 cores), 12 of them macOS-enabled cores.
    RetroArch, ES-DE, the renderer, PCSX2, Cemu and PPSSPP are Linux-only,
    and the root flake exposes only `x86_64-linux`.
  - Last observed: 371 checks, not 230.
  - The renderer's hot reload is broken after the first successful edit
    (G3).
- Build structure (2026-09-19 evening): 22 flakes, one per emulator (8),
  per core (11), RetroArch, ES-DE and the renderer, each with its own
  pinned source input and lock, composed by the root flake through path
  inputs. Every binary in the bundle is compiled here (checked against
  cache.nixos.org for each emulator and core), `nix flake check` passes
  including the `platform-matrix` check that evaluates the Linux and macOS
  derivations of all 22 (macOS: 5 emulators, 10 cores; Windows declared
  planned everywhere), and the host runs that bundle. Built-on-macOS is
  unverified: no Darwin builder here.
- Last observed result: 2026-09-19 session. Every one of the 17 systems
  launched a real ROM fullscreen on FRACTAL-NORTH through `semu launch` (11
  through RetroArch, 8 native), the RetroArch systems now render through the
  shared renderer with bezels and shaders switchable from settings, ES-DE
  runs with the existing gamelists and a working SEMU SETTINGS menu, save
  states work, Flycast, PPSSPP and Dolphin composite through the preload
  shim, Syncthing save sync and the Steam shortcut writer work from the CLI,
  `make test` passes 230 contract checks, `nix flake check` passes
  (contracts, headless RetroArch, installer), and a relocatable release runs
  through bubblewrap. Unverified: gamepad input, Start+Select chord, reboot
  survival, bezels on PCSX2, melonDS, Azahar, Cemu and Ryujinx, anything on
  a physical Deck. Azahar and Cemu stopped creating GL contexts late in the
  session even outside Semu (see M6); recheck after a reboot. Commits
  after `52d60cf` are not pushed yet: GitHub SSH is waiting on the 1Password
  authorization prompt on the desktop.
