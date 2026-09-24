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

## Gap review (2026-09-22) and its resolution (2026-09-23)

The review ran on the Mac (mbp21, aarch64-darwin, macOS 27). FRACTAL-NORTH did not resolve
and no Deck was reachable on either day, so nothing below was observed on a production target.
Each gap keeps its done criterion; the evidence is what was observed on the Mac. Commits
dd2d43b to aa75831.

Rulings taken as defaults because the owner was not available (reversible; say if wrong):
- macOS is a development host, not a product target (G1). Superseded the same day: the owner made the Mac a target (see "macOS product target").
- Recolours and the late-night light are drawn by the renderer over the unmodified upstream
  plates (layer `tint` and a multiplied, lifted `ambient` plate); the few flattened plates are
  baked by nix on the building machine, never committed (G4, G7).
- `semu bezel` may write `config/bezels/<id>/bezel.json` and nothing else under the checkout's
  `src/` or `config/` (`SemuPaths.writeSource`, G5).

### G1. Build and tests run on one host only — done on the Mac

- The root flake exposes aarch64-darwin for the development outputs (btrcpy, semu-program/cli,
  bezel-tree, bezel-layers, visual-assets, asset-root, the contracts check); the bundle and
  emulators stay x86_64-linux. The Makefile's `nix run .#btrcpy` works there (the libffi abort
  came from btrc's own lock; through semu's follows it is gone).
- Evdev sits behind `src/launch/semu_evdev.h` (kernel headers on Linux, the same constants and
  records elsewhere); the dead pad reader in `process.btrc` is deleted.
- `MegaBezelTree` reads the pinned `.#bezel-tree` (slang-shaders pinned by rev, packs named in
  `bezels.json`) via `SEMU_BEZEL_TREE`, the bundle, or `build/bezel-tree`; no
  `/run/current-system`, no `nix-build <nixpkgs>`.
- Observed: `make build test` passes natively (2601 checks); the contracts run in 2 s once built;
  `nix build .#checks.aarch64-darwin.contracts` passes 2601 with no skip from pinned inputs;
  `make -j2 build test` from a touched source takes 61 s, all of it the two BTRC transpiles.
- Remaining: the M1 "under a minute" wants a faster transpiler or one binary for CLI and
  contracts; the x86_64-linux checks were evaluated, not built (no Linux builder here).

### G2. Tests that report PASS without testing — done

- All nine keep-list specs are ported to `tests/contracts/spec/*.btrc` against the current code
  and run in `make test` (RetroArch 294 conditions, runtime 323, standalone+Cemu 774, Steam
  Input+ES-DE 397, Game Boy 18-case matrix and touch 167, plus new launch (68) and render-env
  (111) specs). A skip now fails unless `SEMU_ALLOW_SKIP=1`; the hardcoded package count is a
  per-package check. `make nix-check` builds the checks. Quit chord and process-group
  behaviour have real tests (a fake child group is started and killed).
- Python is gone from the integration check (socat), and it now checks RetroArch's exit status.
- Remaining: one integration check per M3 system through `semu launch` with a real core on
  Linux (needs a Linux builder; the check exists only for the synthetic core); `menu-e2e.sh`
  and the Xvfb captures still need FRACTAL-NORTH.

### G3. Renderer defects — done, follow-ups listed

- Hot reload rebinds sampler units and checks once a second; `tests/visual/hot-reload.sh`
  proves a reloaded program draws the identical frame (the unfixed code changed 98.8 % of it).
- A failed shader chain, layer, asset or game phase keeps the bezel and the Semu menu and logs
  once; `initialize` backs off; no texture or source leaks on context change; the GL state guard
  also keeps units 0-15, UBO bindings 0-3 and the clear colour; plates are mipmapped;
  `SEMU_RENDER_FPS`/`FRAME_DELTA_MS` are emitted and `RenderEnvSpecContract` checks every read
  name is emitted; the dual gap is a setting, the widescreen switch is the 4:3/16:9 midpoint, the
  menu scales by whole multiples with nearest filtering.
- Remaining: a compositor file broken at launch has no built-in fallback program; no
  `system.json` declares `display.refresh_hz`; the renderer is still linked into RetroArch and
  PCSX2 (dlopen against the ABI header would end the rebuilds); the preload shim still assumes a
  centred picture at `SEMU_RENDER_ASPECT`; standalone melonDS, Azahar, Cemu and Ryujinx get no
  bezel (their `doc` strings no longer claim one).

### G4. Bezel look and variants — done

- Recolours are layer tints (`recolor`), the TV scenes carry their night plate again
  (`ambient`); seen in the editor and in the real renderer: arctic white, berry magenta, red PSP,
  night-lit NES.
- M9 again: black-edition NES, purple-trim SNES and Soqueroeu's speaker TV for n64, genesis,
  psx, ps2, dreamcast, gc and wii are the alternates; `none` turns one system's bezel or shader
  off (it used to fall back to the default), offered on every system page. 60-cell matrix
  (default, alternate bezel, alternate shader, off) for 15 systems rendered and inspected.
- Layers are bundled (`.#bezel-layers`, verbatim, with each upstream's licence); a package is
  usable from its layers alone. Scenes whose canvas is the room cover-fit the screen when every
  tube stays visible, so 16:10 no longer letterboxes.
- The editor opens fitted (smaller zoom steps, refit on resize), answers only its own loopback
  origin, serves the night plate, reports failed saves and drops the rejected `reach`.
- M11.5/M11.6: `tests/visual/gallery.sh` renders every variant at Deck and 4K through the real
  renderer and lights a flat card per fixed screen: 68 of 68 within 2 px (full run 4 min 8 s,
  `--quick` 39 s), plus the dimensions table and plate overlays, all inspected.
- `placement.btrc:303` `resX *=` is Mega Bezel's own (`cache-info.inc:183`), ported faithfully.
- Placement modes (d1b9014): `visual.placement` or `visual.systems.<id>.placement` = `fit`
  (default), `game` (the picture at the largest whole multiple the screen holds, shell cropped:
  DMG, GBC and GBA reach 5x on the Deck) or `bezel` (largest whole multiple with the shell
  whole); inspected at 1280x800. **Ruling needed:** which default per system (today `fit`,
  so the DMG LCD stays about 1.15x on the Deck).

### G5. Launch, input, settings, owned paths — done, rulings pending

- The quit chord comes from config; RetroArch gets `QUIT` over its command port (port read from
  its profile) before SIGTERM and the bounded SIGKILL; the group is ended after a normal exit;
  device slots are reused; slot next/prev journal their own codes.
- Settings writes never replace a malformed `semu.json`; malformed overrides are diagnostics.
- One owned-write helper: normalized paths, no writes into a checkout's `src`/`config` except a
  bezel package, temporary-then-rename. Steam Input icons copy as bytes (they were cut to 8
  bytes); `sync stop` checks it is Syncthing; peers are XML-escaped; seeds refuse symlinks and
  stay inside the state root; unresolved `${...}` fails a launch; the Wii controller mode and
  the RetroArch port are data; the supervisor is split under 500 lines.
- `semu steam input [--steam-root]` (66cec43) derives every destination from one Steam root,
  writes each user's default FULL profile and removes only Semu's file from retired ids.
- Remaining: the supervisor ignores the gamepad
  identities in `steam_input.json`; seed copies are not atomic; ES-DE install follows a symlink
  above the ES-DE home (the owner's `~/ES-DE` → Drive layout depends on it: **ruling needed**).

### G6. Packaging and directive compliance — mostly done

- Dead fields (`gl_wrapper`, `sandbox`, `backend`, `launch_contract`, `managed_profile_files`,
  `settings_overrides`, `fallbacks`, `build_contract`), `install.json`, the unused PCSX2 patch,
  `retroarch.nix` and the Python gallery template are gone; RetroArch patches apply with
  `--fuzz=0`; the Deck's Steam Virtual Gamepad autoconfig ships in the bundle; PCSX2 follows the
  root renderer and btrc; the slang tree is pinned by rev; the platform matrix also checks
  `emulator.json` slices and `package.json` systems against each flake.
- Remaining: nixpkgs still tracks `nixos-unstable`; ES-DE's nixpkgs (2026-01-02, insecure
  FreeImage) and the pins M4 names (Cemu v2.6, Ryujinx 1.3.3, ES-DE, RetroArch 1.22.2, PCSX2
  v2.6.3) are not refreshed; `librashader`, `syncthing` and `retroarch-joypad-autoconfig` are
  nixpkgs packages used as-is; the two targets and input files are copies, not inheritance.

### G7. Art licence — done, history is the owner's call

- `config/assets/NOTICE.md` attributes Duimon (CC BY-NC-ND 4.0) and Soqueroeu (credit, no
  profit); the bundle ships each upstream's licence beside its plates; the derived PNGs left the
  repository and are baked by the nix stager on the building machine.
- Remaining: the derived PNGs are still in git history (rewriting published history is the
  owner's decision).

### G8. Python in the tree — done

`git ls-files '*.py'` is empty and no script, check or Makefile calls python. The virtual gamepad
is `tests/visual/virtual_pad.btrc` (uinput), the gallery is bash + the render host, M11.7's
cleanup is done and the README and memory notes describe the new tools.

### Linux builds from the Mac (2026-09-23, e1f1f91)

A stock `nixos/nix` x86_64 container on this Mac's podman VM builds x86_64-linux with the sandbox
on (only Nix's syscall filter is off, since seccomp cannot load under emulation). Built and run
there: `checks.contracts` (2611), `checks.installer`, `checks.retroarch-headless` (RetroArch built
with the renderer loader, driven through socat) and the new `checks.launch-systems`: all 12
RetroArch systems launched through `semu launch` with the synthetic core as their core file,
answered on the command port and ended through their own QUIT with nothing left over. The
renderer now reaches RetroArch and PCSX2 through `libsemurendererloader` (dlopen of
`SEMU_RENDERER_LIBRARY`), so renderer edits no longer rebuild them; the renderer's install check
proves the forwarding. After the owner freed the old cache volume: PCSX2 built with the loader (its binary needs no
`libsemurenderer.so`), and the new `checks.real-cores` passes: gb and gbc (gambatte), gba (mgba)
and nes (mesen) boot pinobatch's 240p Test Suite (GPL-2.0-or-later, `tests/integration/
test_roms.json`) through `semu launch`, draw a non-blank frame, save and load a state and quit
through QUIT. Not built there: `checks.platform-matrix` (it evaluates on the Mac) and the whole
bundle. n64 (mupen64plus_next, angrylion) now boots PeterLemon's public-domain `HelloWorldCPU32BPP320X240`
the same way (2026-09-23, in the VM's Xvfb); its RDP-drawn twin gave a blank frame under
angrylion, an open lead. Still open: test programs for PSX (Beetle PSX needs a Sony BIOS, which
is not freely licensed), PSP, DS, 3DS and Dreamcast (no pinned binary with a clear licence yet) and nixpkgs on a release branch with the M4 pin
refresh (a full rebuild of every emulator; FRACTAL-NORTH).

### macOS product target (2026-09-23)

The owner said semu may run on the Mac and pointed at the library in
`~/Drive/media/Games/Emulation`, which supersedes the G1 ruling. The `macos` target
(`config/targets/macos.json`, `aarch64-darwin`) resolves that library read-only and keeps
state under `~/Library/Application Support/Semu`. Done and observed on mbp21:
- RetroArch 1.22.2 builds from the pinned source for darwin (`config/emulators/retroarch/
  darwin.nix`): Metal plus glcore for the Semu renderer hook, the renderer reached through the
  loader, and a real `RetroArch.app`. Its profile uses Cocoa input, HID pads and CoreAudio.
- The renderer builds as `libsemurenderer.dylib`. macOS pads reach the supervisor through
  Apple's GameController framework (`src/launch/gamepad.btrc`), one slot per controller, with a
  departed pad's buttons released (spec-tested).
- Real ROMs from the library, through `semu launch --target macos` (a one-off on-screen run, script since removed), each with the bezel: gb, gbc and psp booted and quit through QUIT (psp
  also saved a state); gba (inspected capture: bezel and boot logo), nes, snes and genesis
  booted but did not exit within the 1.5 s QUIT grace and were stopped by signal. That run
  happened while the owner was using the screen, and the owner stopped it. **Never use the
  Mac's real display: emulator runs go to a virtual display (the Linux VM with Xvfb) or the
  offscreen render host.**
- psx multi-disc games in the library are ES-DE directories named `Game.m3u/` holding
  `Game.m3u`. `semu launch` now resolves the directory to that file (contract-tested).
- 3DS through RetroArch is Linux-only: Azahar and Citra both compile their OpenGL renderer
  out on Apple, and Azahar asked for Vulkan and shut down under glcore. Dreamcast's Flycast
  core has no darwin build either. On the Mac, 3DS runs on standalone Azahar: it builds for
  darwin with nixpkgs MoltenVK (`USE_SYSTEM_MOLTENVK`, QuartzCore, target 14.0), its macOS
  slice pins Vulkan (`graphics_api=2` from `platform.azahar.graphics_api`; Linux keeps OpenGL),
  and its XDG state root is honoured on macOS once those directories exist. Not yet observed
  running.
- Darwin cores built: gambatte, mgba, mesen, snes9x, genesis_plus_gx, mednafen_psx, ppsspp,
  melonds (the Linux-only `-z noexecstack` is now gated) and desmume (deployment target 11.0).
  mupen64plus_next builds against nixpkgs zlib and libpng on darwin (its bundled copies
  take Apple headers for classic Mac OS) and, like upstream's osx build, without the dynarec
  (its arm64 linkage is ELF assembly).
- The darwin bundle builds (`nix build .#packages.aarch64-darwin.semu`, 0e500d6): Semu.app,
  ES-DE.app (built against Nix libraries), RetroArch.app, Dolphin.app, Ryujinx (`bin/Ryujinx`;
  nixpkgs builds no .app, fixed in 04a64c2) and the ten darwin cores. `semu doctor --target
  macos` against it resolves every library path and all three emulators. `semu-es-de` now puts
  the bundle's `bin` on PATH, since Semu.app opened from Finder has only `/usr/bin:/bin` and
  ES-DE finds the `semu-*` shims there (85422fb).
- `semu <command> --help` used to run the command: `prepare --help` wrote an ES-DE home under
  `~/Games/Emulation` on the Mac (removed; nothing was there before). Help now wins over
  everything and is contract-tested (c56b0fb, 0d60d8c).

- The QUIT stall is fixed (2026-09-23, observed on screen after the owner unlocked the Mac).
  RetroArch took GPU screenshots at the whole viewport, 4800x3200 on the 6K display, and its
  PNG encoder took seconds. RetroArch's tasks run one at a time, so SAVE_STATE waited behind
  the screenshot, and QUIT waits for pending tasks. The profile now captures the core's frame
  (`video_gpu_screenshot = "false"`). RetroArch also wrote its content history to
  `~/Documents/RetroArch` on macOS; history is off now, and its playlist, log, cache,
  thumbnail, remap and menu-config directories live under the state root. With the bundle,
  every RetroArch system (gb, gbc, gba, nes, snes, genesis, n64, psx through the `.m3u`
  folder, nds, psp) booted a real library ROM with its bezel, saved and loaded a state and
  quit through QUIT in about 1 s. Nothing new appeared in `~/Documents/RetroArch`. Linux
  checks (contracts, launch-systems, real-cores) pass in the VM with the change.
- Dolphin ran a GameCube game fullscreen on the Mac (OpenGL, with Dolphin's "no
  ARB_buffer_storage, performance may be poor" notice). A Vulkan (MoltenVK) trial exited
  before capture while the screen was locking, so it is not adopted.

- Standalone emulators on the Mac, observed on screen 2026-09-23 through `semu launch` with
  scratch state: Dolphin runs GameCube (an intro video) and Wii (Animal Crossing loading) on
  OpenGL; Ryujinx boots Animal Crossing fullscreen to its controller applet (no pad
  connected) after two fixes, firmware seeded on macOS and `use_hypervisor` off (the unsigned
  Nix build stalls after loading the game with Apple's hypervisor); Azahar runs Ace Combat on
  Vulkan with both screens, after a source patch skips its "run it through `open`" dialog.
- RetroArch cores wrote into the read-only library's system folder (melonDS
  `firmware.bin.bak`, Mupen64Plus `mupen64plus.ini`). RetroArch's `system_directory` is now a
  state-root copy of the library's, seeded by a new `missing_files` mode: copy the whole tree
  once, then add only files the copy lacks, never replace (contract-tested; first copy of
  545 MB, 3346 files, is about a second once Drive has the files locally). Re-run: N64 and
  DS write into the copy and the library is untouched.
- Semu.app is the responsible process for everything it starts, so macOS TCC killed ES-DE for
  Bluetooth without a usage string; Semu.app now declares Bluetooth, microphone and camera,
  and `semu-es-de` stops macOS holding ES-DE at a reopen-windows prompt after a crash.
  `tests/visual/mac-locked.sh` checks every RetroArch system while the screen is locked.

Still open on the Mac:
- ES-DE through Semu.app, seen only through its log and stacks (the owner declined computer-use
  control): SDL3's CoreAudio open waits about six minutes on the default device, a Neural DSP
  Quad Cortex, where RetroArch's CoreAudio plays at once; listing the Drive-hosted ROM folders
  then blocks, most likely on macOS's one-time "access files in Google Drive" consent for
  Semu.app, which only the owner can grant. ES-DE's own launch command for a gb game (the `semu-retroarch` shim with an absolute `%ROM%`) plays, saves and quits via QUIT, after the bundle started defaulting and exporting `SEMU_TARGET` (the shims had run as linux-desktop and refused every macOS ROM, 3609a69). Linux checks pass in the VM with the system-folder copy (contracts 2764, launch-systems, real-cores).
- Dolphin on Vulkan (MoltenVK ships in the build) exits at once in batch mode; OpenGL stays.
- Signing Ryujinx with the hypervisor entitlement would bring the hypervisor back.

### G9. Observation still missing (needs hardware)

- On FRACTAL-NORTH: Xbox pad input, Start+Select on hardware, save and load by pad, reboot
  survival (M2-M4); `menu-e2e.sh` with the BTRC pad; captures of wii, ps2 and n3ds through real
  emulators; a 16:9 title for the widescreen switch; Azahar and Cemu GL after a reboot; Steam
  launching the shortcut; `nix flake check` built on x86_64-linux.
- On the Deck: all of M5 and the Deck halves of M6 and M8.
- Inspected Mac captures are retained under `build/verification/mbp21/2026-09-23/` (M9 matrix,
  gallery with verification table and overlays).

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

- M1 reset: met on the Mac 2026-09-23 except the one-minute bound (G1, G2: spec tests ported, 2601 checks, no skips; `make -j2 build test` from a touched source takes 61 s, all of it BTRC transpiling; the contracts alone take 2 s). Was: done 2026-09-19 (`make build && make test` pass in about ten
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
- M9 bezel and shader fidelity: done again 2026-09-23 through the real renderer on the Mac (G4: 60-cell matrix inspected, build/verification/mbp21/2026-09-23); real-emulator captures still pending on FRACTAL-NORTH. Was done on the desktop 2026-09-19 (late) for
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
  galleries, and the glass-asset crop. (2026-09-23: bundling and both galleries done, see G4.)
- Active milestone (2026-09-23): the P0 gaps from the 2026-09-22 review are closed on the
  Mac (see *Gap review ... and its resolution*). What is left needs hardware or a ruling:
  1. FRACTAL-NORTH: `nix flake check` built on x86_64-linux (contracts with the bezel tree,
     retroarch-headless with socat, installer, platform-matrix), `tests/visual/menu-e2e.sh`
     with the BTRC virtual pad, the pad/Start+Select/reboot items of M2-M4, real-emulator
     captures of wii, ps2 and n3ds.
  2. The Deck: M5 acceptance (`DECK_HOST=deck tests/deck/deploy.sh install`), then the Deck
     halves of M6 and M8.
  3. Engineering: per-system checks through `semu launch` and renderer dlopen are done and
     built on x86_64 Linux from the Mac (see *Linux builds from the Mac*). Left: real cores with
     real test ROMs per system, nixpkgs on a release branch and the M4 pin refresh, PCSX2 built
     with the loader (all need a full emulator rebuild: FRACTAL-NORTH or a bigger VM disk), and a
     design choice for a built-in fallback compositor (G3).
  Owner rulings (2026-09-23): handhelds default to `game` placement (gb, gbc, gba, psp); ES-DE
  may sit under a link and the ROM and BIOS folders are chosen from SEMU SETTINGS > LIBRARY;
  git history is left as it is; the old VM cache was freed for the Linux builds. The three
  defaults at the top of the gap section stand unless the owner says otherwise.
- Implementation 2026-09-23 (Mac, commits dd2d43b..aa75831): 2601 contract checks pass
  natively and in the darwin flake check with no skips; the real renderer runs offscreen on
  the Mac (`tests/visual/render_host.btrc`); the full gallery verifies 68 of 68 fixed screens
  within 2 px at Deck and 4K; hot reload proven; no Python in the tree. Corrections to older
  lines: 29 packages on disk (26 plus 3 TV alternates, no Semu CRTs); build structure is 24 flakes and the
  root flake exposes aarch64-darwin for development outputs.
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
