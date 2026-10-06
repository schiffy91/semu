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
- `config/input/steam/`: Steam Input templates for the Deck and the Steam
  Controllers (shared with linux-desktop) with the binding table from
  `steam_input_contract_test.btrc`, published into Steam's directories with
  the existing atomic publish logic trimmed to what is needed, plus the
  configset entry that makes Steam load the Semu profile.
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
  nothing double-fires. On Linux RetroArch's own keyboard rows are "nul"
  too (the keymap Semu compiles, and every 69a4f0e default whose key a Semu
  chord uses): the supervisor sends keyboard chords over the command port,
  because RetroArch polls its keymap once a frame and can miss a short tap.
  macOS keeps RetroArch's keymap, since its supervisor has no keyboard source.
  Semu-owned actions (`ui.menu*`, `visual.*`, and `app.quit` wherever the
  supervisor reads keyboards) are never compiled into an emulator's own
  keymap, and every native default that shares a Semu chord is written empty
  or provably never applies (`profile.json` `native_shortcuts`, from each
  pinned source). Save slots are the emulator's own (`emulator.json`
  `save_states`): Semu's counter wraps both ways as the emulator does, save
  and load hit the selected slot, and the menu shows the slot as the emulator
  numbers it.
- Plain controllers: hold Select and press a button (`gamepad_chords`):
  Y opens the Semu menu, R1 saves, L1 loads, B screenshots, D-pad left and
  right change the slot. Start+Select stays the quit chord. Button names are
  positions: pads listed in `gamepad_chords.xpad_layout_ids` (Steam's virtual
  pad 28de:11ff, Microsoft pads) report the top button as BTN_WEST, and the
  supervisor swaps it back. On the Deck the trackpad radial emits the
  keyboard chords, but in Game Mode Steam types them only as XTest into the
  game's Xwayland, which evdev never sees. The supervisor therefore reads
  keys from evdev and, where an X display is present, from XInput2 raw key
  events on the emulator's own display (an optional adapter, see the
  rulings). Both paths produce the same actions: a chord fires on its own
  key with exactly its modifiers held, one action from two sources within
  250 ms runs once, and Semu's own uinput typing is ignored when it comes
  back through X.
- The right trackpad is the pointer on DS and 3DS: Steam moves the X pointer,
  a soft press taps the touch screen under it and press-and-drag drags, on
  every route (RetroArch's melonDS, DeSmuME, Azahar and Citra cores, and
  standalone Azahar), wherever the bezel or layout put the touch screen. A
  cursor shows while it moves and hides when idle (Semu draws it on
  RetroArch routes; standalone Azahar draws its own), and no core can swap
  the screens out from under the bezel.
- The native menu (`menu.items`): RESUME, SAVE STATE, LOAD STATE,
  SCREENSHOT, BEZEL, SHADER, QUIT GAME. Opening it pauses the
  emulator through its own pause action, and closing it resumes, only where
  the emulator was seen to keep presenting while paused (`emulator.json`
  `menu.pause`); elsewhere the menu draws over the running game, and an
  emulator with no compositor gets no menu, never an invisible one. On every
  emulator the open menu is modal for pad input: on Linux the supervisor
  grabs the pads it reads (EVIOCGRAB, Steam's virtual pad in Game Mode) once
  every button, stick, trigger and hat on them is at rest, so the emulator
  never keeps a held button it saw go down but not come up, and lets them go
  after the menu closes, once the closing press is released (2 s at most), and
  at once on quit, a stop, the emulator's exit and the supervisor's end. The
  renderer draws it from `SEMU_MENU_ITEMS` and the supervisor mirrors the
  same list, so the drawn selection and the executed action never diverge.
  BEZEL and SHADER (and the radial's Next Bezel, Next Shader) step this
  system's own variants, then off, live, and save the choice to `semu.json`
  as `visual.systems.<id>.bezel_variant` or `shader_variant`; the launch
  precomputes every combination into `semu-render-variants.env` and the
  journal carries the chosen index (codes 79 and 80). The renderer applies
  that section's keys two game frames later and reloads only the bezel
  images; a toast at the top of the screen names the new choice (and the
  slot after a save, load or slot change), and the menu's BEZEL and SHADER
  rows show the current value.
- The Deck's radials (`config/input/steam/steam_input.json`, one template per Steam controller, the
  same for all 17 systems): the left pad is the quick ring (Save, Load, Previous and Next Slot, Next
  Bezel, Next Shader, Settings, Menu, Screenshot, Quit; centre empty). Settings is a Steam preset
  switch to the settings radial (Next Bezel, Bezel On/Off, Next Shader, Shader On/Off, Fit, Aspect,
  Controller Layout, Players, Reset to Default; Close Settings in the centre), which stays until
  Close while the rest of the controller stays the gamepad. Holding View or an upper grip gives the
  menu ring (Up, Down, Confirm, Back, Bezel On/Off, Shader On/Off, Reset, Players; Open Menu in the
  centre); a lower grip the Wii layer. Every slot types a modified chord of the one vocabulary,
  except the two preset switches, and every icon in one radial differs.
- **Done when:** on the desktop, a controller opens the menu with Select+Y,
  navigates it with the D-pad, saves a state from it (state file observed),
  toggles the bezel live, and Start+Select still quits, all captured
  headlessly through a truthfully named virtual gamepad; on the Deck the
  same through the trackpad radial, in two parts. Off-screen, on the release
  that carries the radial (`tests/deck/input-check.sh PAD
  tests/deck/radial-check.cases OUT`): each radial chord, typed as XTest from
  the second Xwayland of a private gamescope, runs its action once (`semu:
  action <id> (keyboard)` and its journal record), Next Bezel and Next Shader
  switch live with their toast and are saved for that system, Select+Y on the
  replica of Steam's pad opens the menu, the menu over running Azahar keeps
  its d-pad and A from the game, and right-trackpad taps reach every
  DS and 3DS route where the touch screen is drawn, before and after a layout
  switch. Then the owner, once in Game Mode: Steam loads Semu's profile
  (controller_ui.txt names config/semu/controller_neptune.vdf for App ID
  2162992320), the radial's ring, icons and click-to-fire, Steam's own key
  timing, the trackpad's feel and cursor, and Semu's uinput typing into
  standalone emulators, none of which an off-screen run can reach.

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
core's flat-card and control-file modes (removed 2026-10-03 with the
RetroArch calibration path, G8); the gallery's verify mode.

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

### M12. Bezels and shaders for standalone emulators (proposed 2026-09-25)

No standalone emulator has a bezel on macOS, and on Linux Azahar, Ryujinx,
Cemu and standalone melonDS have none. Prior art (researched 2026-09-25):
EmuDeck has unimplemented stubs for every standalone bezel on Linux and
Windows; ES-DE and Pegasus have none; Batocera draws a separate overlay
process that swallowed Azahar's touch input until it got an empty input
region; RetroBat injects ReShade (rescales the picture, so pointer and touch
drift; no Vulkan) or stacks a topmost click-through window (exclusive
fullscreen, cursor and focus bugs); vkBasalt is Linux-only, unmaintained,
submits on the wrong queue and is reported not to work under gamescope's
Wayland backend. So Semu composes in-process at present time, with no
windows of its own:

- One compositor: an API-free core (layout, largest-integer placement, bezel
  art, parameters, the Semu menu) with an OpenGL backend (today's) and a
  Vulkan backend (`compositor.frag` compiled to SPIR-V; librashader's Vulkan
  runtime for the CRT and LCD presets).
- Entry points: RetroArch keeps its in-process tap. Standalones on Linux,
  the Deck and later Windows go through Semu's own Vulkan layer (vkBasalt's
  zlib boilerplate as a start; submit on the present queue; handle several
  swapchains and present ids; work either side of gamescope's WSI layer; be
  visible to Flatpak builds). On macOS the emulators load MoltenVK directly
  and never see a layer, so the same code ships as a MoltenVK proxy library
  (`LIBVULKAN_PATH` for Dolphin and PCSX2, bundle placement or a source
  patch for the others).
- Decorate, never move: the emulator's picture stays where it drew it, so
  mouse and touch keep working. Semu pins each emulator's scaling and screen
  layout so the picture sits in the bezel's hole; dual-screen emulators
  report their screen rectangles through a small source patch (the tap
  contract). Curvature stays on non-pointer systems.
- Vulkan is pinned in every managed profile (Dolphin, PCSX2 and Cemu have
  Metal backends on macOS; Windows builds default to Direct3D). melonDS has
  no Vulkan; DS stays on the RetroArch core.
- Order: renderer split and Vulkan backend against the reference renders;
  the layer with Dolphin on Linux (headless through lavapipe in the VM,
  compared with today's preload); Ryujinx and Cemu; Azahar with its screen
  rectangles; the macOS proxy; Windows later.

Done when: Dolphin, Azahar and Ryujinx draw inside their bezels with the
system shader on Linux and macOS, observed in inspected screenshots, and a
mouse click on the 3DS bottom screen lands where it is drawn.

Progress (2026-09-25):
- Design change, recorded here: no Vulkan port of the compositor. The Vulkan
  entry points share one image with the existing OpenGL compositor through
  external memory (Linux: `GL_EXT_memory_object_fd` in a surfaceless EGL
  context; macOS: an IOSurface exported by MoltenVK through
  `VK_EXT_metal_objects`, bound by an offscreen CGL context). The whole
  compositor and librashader's OpenGL path carry over unchanged; CPU fences
  order the two APIs (llvmpipe has no `GL_EXT_semaphore_fd`).
- `src/renderer/vulkan/`: `semu_vulkan_core.btrc` (tracking, blits, the
  present round trip), `semu_vulkan_layer.btrc` (Linux layer
  `VK_LAYER_SEMU_compositor`), `semu_vulkan_metal.btrc` (the macOS stand-in,
  `lib/semu-vulkan/libvulkan.dylib`, re-exporting MoltenVK). The shared
  composition (single screen letterboxed, or Azahar's stacked two screens)
  and the pointer map live in `preload/semu_compose.btrc`, used by the Linux
  preload, the layer, the stand-in and the macOS window shim.
- `render_vulkan` in an emulator's platform entry emits the render
  environment and loads the layer (Linux) or puts the stand-in first on
  `DYLD_LIBRARY_PATH` (macOS). Azahar, Cemu and Ryujinx on Linux and Azahar
  on macOS now run Vulkan with it. Dolphin on macOS (OpenGL) is composed by
  the window shim at `-[NSOpenGLContext flushBuffer]` (`render_preload`).
- Touch: Azahar's `semu-touch.patch` maps a click on the composed picture
  back through `semu_touch_unmap` (the renderer's pointer map) to where its
  own layout drew the bottom screen; unchanged when Semu is absent.
- Ryujinx on macOS loads the MoltenVK it bundles (.NET resolves it in its own
  folder first): `packaging/nix/ryujinx_semu.nix` puts a second link of the
  stand-in there as `libMoltenVK.dylib`, re-exporting the original renamed
  beside it. Observed offscreen with Ryujinx's MoltenVK 1.2.0 (no headless
  surfaces there, so the harness presents to a CAMetalLayer no window shows).
- Observed offscreen: vkcube on lavapipe under Xvfb in the Linux VM, and
  `tests/visual/vulkan-present.c` (a headless swapchain with a test card) on
  the Mac through the stand-in, both inside the GameCube TV bezel with the
  CRT shader, upright and in the right colors.
- Observed with a real game (2026-09-25, `tests/visual/vm-azahar-layer.sh`):
  Pushmo in Azahar on lavapipe in the Linux VM, composed by the layer into
  the 3DS shell with the LCD grid on both screens; a click held on the OK
  button of the composed bottom screen (1093,560, where Azahar never drew
  it) mapped to 638,653 in Azahar's own layout and dismissed the dialog.
  The Linux half of the touch criterion is met.
- Observed on the Mac with real games (2026-09-25, screen locked, frames
  captured by the renderer): Ace Combat in Azahar on both screens of the 3DS
  shell at 4112x2582, and Aggressive Inline in Dolphin in the GameCube TV
  with the CRT shader. Two fixes came out of it: the stand-in resolved
  `vkCreateDevice` against Azahar's throwaway instance (MoltenVK marks every
  object alike), and a `libMoltenVK.dylib` on the library path shadowed
  Ryujinx's own, stalling its start (`vulkan_stand_in: beside` now keeps
  Ryujinx off `DYLD_LIBRARY_PATH`).
- Ryujinx on the Mac: Animal Crossing composed through the stand-in at
  4112x2658 (captured, screen locked, a keyboard mapped as player one so the
  controller applet passes). .NET found each assembly's folder from its real
  path, so `ryujinx_semu.nix` now copies the Ryujinx folder instead of
  linking it; linked, Ryujinx loaded its original MoltenVK.
- Linux through the layer, in the Rosetta-backed VM on lavapipe (2026-09-25):
  Ryujinx (Animal Crossing, keys and firmware from the library) composed 960+
  frames with no layer error, but the game had not left its black boot frames
  before it exited at about 2:40 at emulated speed; Cemu (Kirby and the
  Rainbow Curse) composed its first frame, then segfaulted inside its own PPC
  interpreter. Both need a native Linux machine for an inspected picture.
- Observed live on the owner's unlocked Mac (2026-09-25): Pushmo launched through semu's plan
  opened at the full screen size and faded in (alpha 0 to 1 in about 260 ms, no small window);
  a real click on OK in the composed bottom screen of the 3DS shell (screen point 1756,984;
  framebuffer 3512,1892) mapped to Azahar's layout at 2052,2269 and dismissed "Save data
  created", and the game went on to its intro. The touch criterion is met on both platforms.
- Open: those two inspected pictures on real Linux hardware, and the Deck
  (gamescope's WSI layer alongside ours).

### M13. Owner feedback from the Deck (2026-10-04)

The owner tested the 70a5d9a release on the Deck (restarted it, played) and asked
for the following; tackled with subagents (investigation, design, critique, then
sequential implementation, review and fixes), each item done only when observed.

0. **Mods, done properly.** The owner's mods sit in an older per-emulator tree on the
   SD card (`Emulation/{Azahar,Cemu,Dolphin,PCSX2,RetroArch,Ryujinx}`): OoT 3D and
   Majora's Mask 3D LayeredFS patches, 7.8 GB of 3DS HD texture packs (4 titles), a
   Tears of the Kingdom mod, RetroArch cheats; Semu never installed any of them. Build a
   modular, declarative system: one centralized mods library under the emulation root
   where the user places mods for every emulator in its native format; each
   emulator.json declares its mod kinds, where they install in its state root and the
   settings that make the emulator use them (safe defaults: async texture loading, no
   multi-GB preload); Semu installs them without copying large data or writing into
   the library. Migrate the existing mods into the central library with `mv` (same
   filesystem, a rename; the owner authorised moving mods only, nothing deleted),
   running in the background on the Deck. Done when OoT 3D shows its patch and HD
   textures and TotK loads its mod on the Deck, installed from the central library.
1. **N64 TV edge glow** is missing (other TV systems have it). Done when the N64 TV
   bezel draws the same edge glow as the others, inspected in render-host pictures and
   on the Deck.
2. **Right trackpad is the pointer by default** on 3DS, DS and Wii, in addition to
   touch (Wii: the Wii remote's IR pointer follows the trackpad cursor). Done when a
   trackpad move and click point and tap on each, on the Deck.
3. **Radial, version 2.** For every system the radial changes and turns off bezels,
   changes and turns off shaders, resets to defaults, and switches between bezel fit
   and screen fit (always integer scale); systems with two outputs (Wii) switch 4:3 and
   16:9. Systems with several controller layouts (Wii) change layout from the radial,
   and a multiplayer part of the radial assigns pads to players (data-driven per
   emulator, never hard-coded to one player). Done when each slot works live in Game
   Mode and persists per system.
4. **Wii and Wii U were laggy.** Make sure every emulator is a release (optimised)
   build, Semu's forced settings and renderer backends are the fast ones, and measure
   the SD card. Done when the cause is shown with numbers and Wii/Wii U run at full
   speed in the matrix games, or the remaining limit is named.
5. **PSP background** is off-centre and a black carbon-fibre plate: use the same
   centred background as the other handhelds. Done when render-host pictures and the
   Deck show it.

Status (2026-10-04): every item built and observed in the podman VM or the render host; on the Deck
(release b1bf425) item 0's mods load and parts of items 2 and 3 work (below); the review of the series is
fixed and its Deck reruns (cases 4, 14, 16 on release 5f55703) pass; the owner's mods were moved into the
central library on the Deck SD and on the Mac's Drive library; what remains needs the owner in Game Mode:
the Wii IR, the players page by eye, Steam's own radials and icons.

- Item 0 (mods), the core: built 2026-10-04 and observed in the podman VM; the migration (`semu mods
  migrate`, below) is built; the Deck is open. Rulings taken as reversible defaults:
  - One library per machine, `paths.mods` = `<emulation_root>/mods` on every target (an override in semu.json
    moves it; empty turns mods off), organised by emulator then native kind:
    `azahar/{mods,textures}/<title id>`, `ryujinx/contents/<title id>/<mod>` (or romfs.bin/exefs.nsp
    beside the mods), `dolphin/textures/<game id>`, `pcsx2/textures/<serial>`, `ppsspp/textures/<disc id>`,
    `flycast/textures/<game id>`, `cemu/graphicpacks/<pack>`. Every emulator.json declares
    `platforms.<os>.state.mods`; Dolphin, PCSX2, PPSSPP, Flycast and Cemu are dormant until a pack is placed;
    RetroArch and melonDS declare none and say why (libretro packs live in RetroArch's Syncthing-carried save
    folder behind per-core options, and RetroArch rewrites a loaded .cht). The checker refuses an unknown
    field, a target outside `${state_root}/`, two kinds sharing a library folder or a target (or nesting), a
    target over a seed or profile file, a profile `${mods.<group>}` with no group, and a library inside
    content_root, state_root, roms or bios.
  - A launch links before it writes the profiles: one symlink per entry at the declared depth inside real
    state folders, never a copy and never a write into the library; real ancestors are required before any
    link, unlink or rmdir; the emulator's empty title folder is replaced, its own folders and other links
    win (reported as shadowed); case twins, `.semu-off` entries and headers Azahar refuses (IPS without PATCH,
    BPS with metadata, an exheader not 2048 bytes) are reported and not linked. `${state_root}/semu-mods.tsv`
    lists what Semu made, and only that is removed: an entry gone from the library loses its link, an empty
    `paths.mods` removes them all, an unmounted library changes nothing.
  - Settings follow what is installed. A settings group is on while a kind of it is installed: Dolphin
    HiresTextures (CacheHiresTextures False), PCSX2 LoadTextureReplacements (async, no precache), PPSSPP
    ReplaceTextures, Flycast rend.CustomTextures (no preload); with no pack these emulators check one folder
    name, so the switch is global. Azahar is per title, since its switch costs every game a hash per texture:
    qt-config.ini keeps custom_textures off (async on, preload off, F7 unbound, each with `\default=false`),
    and a title with a pack gets `config/azahar-emu/custom/<TID>.ini` with custom textures on and
    resolution_factor at the composed top screen's whole step (2 at 1280x800, 3 at 1920x1080,
    `SemuComposedScale`, the compositor's arithmetic without GL). Ryujinx needs no switch (an unlisted mod is
    on) and gets `games/<tid>/updates.json` choosing the highest `[<id>800][vN]` in ROMs/switch/updates.
  - Observed in the podman VM (`tests/integration/mods.sh`, 028b2a4, the Mac's Azahar load/{mods,textures},
    Ryujinx mods, updates, keys and bis mounted read-only as the library): OoT 3D (Europe) logged "Loading per
    application config file for title 0004000000033600", Utility_CustomTextures true and "load/mods/
    0004000000033600/code.ips patching code.bin", and its title screen draws the pack's "OCARINA OF TIME 4K"
    logo and HD grass at resolution_factor 2; Tears of the Kingdom logged "Found enabled mod '!!!TOTK
    Optimizer'", "NSO 'subsdk3' replaced", "main.npdm replaced", "Using modded RomFS" and "Application
    Loaded: ... v1.2.1", the update the owner's save and mod are on (lavapipe stops at its boot frames).
    The library listing was identical before and after.
  - Open: the GPU cost of resolution_factor 2 and the texture streaming time from the SD (measure on the Deck);
    Continue in TotK and the Deck pictures wait for the migration and a release.
  - The owner's view and Cemu's packs: built 2026-10-04 and observed in the podman VM.
    `semu mods list [--json] [--roms]` shows each entry per emulator and kind with its status (installed,
    pending until the next launch, shadowed by something in the emulator's state, invalid with the reason,
    parked, duplicate), the names the emulator never reads (MM3D's code_faster_aim.bps) and its game: a
    Switch dump by its bracketed id, a 3DS ROM by the NCSD media id at 0x108, declared in system.json
    `titles`; ROM headers are read only with `--roms` (one short read per ROM; on the Deck's SD about 50 ms
    each, so 172 3DS ROMs would cost seconds). It warns when ES-DE starts the system in another emulator,
    which would not see the mods: Semu's first command, unless the gamelist pins one by label (on Linux the
    3DS default is the libretro core; the Deck's gamelist pins Azahar (Standalone), which silences it).
    Listing writes nothing; `semu doctor` sums it per emulator. `semu mods layout` prints the folders per
    kind; `semu mods init` creates them with a README.txt each (an edited one is kept), only below a mounted
    emulation folder and never in state. Ruling (reversible): the libretro routes stay documented exceptions
    with no kind (RetroArch's emulator.json says why), so 3DS mods apply in standalone Azahar only.
    Cemu: a kind may declare `entry_lines`, one settings line per installed entry, written with
    `@lines:mods.<kind>`; settings.xml lists each installed pack by its absolute rules.txt path under
    graphicPacks, since Cemu matches the path it walked through the link and its relative form canonicalises
    into the library (GraphicPack2.cpp:90-97, helpers.cpp:313-321). Observed in the podman VM
    (`tests/integration/mods.sh`, the owner's downloaded CaptainToad_Resolution mounted read-only as
    `cemu/graphicpacks`): Captain Toad (US) logged "Activate graphic pack: Captain Toad: Treasure
    Tracker/Graphics/Resolution [Presets: 1280x720 (Default)]"; with the relative path the first try logged
    no activation. The library was unchanged. The owner's downloaded packs (154 game folders under
    Cemu/data/graphicPacks/downloadedGraphicPacks) are not moved or enabled: that is the migration's call
    (they are folders of packs, which Cemu does not walk through a link, so each pack would go in on its own).
  - The migration: built 2026-10-04 and observed on the Mac and in the podman VM; it has not run on the Deck
    or on the owner's Mac library yet. `semu mods migrate [--dry-run] [--allow-copy] [--detach] [--reverse]`
    moves each entry a kind's `migrate_from` declares (Azahar and Lime3DS `data/load/{mods,textures}`,
    Ryujinx `config/mods/contents`, all under `${emulation_root}`) to `<mods>/<emulator>/<library>/<entry>`
    at the kind's own levels (a Switch mod moves alone; its title folder stays, empty). On one filesystem it
    is a rename the kernel refuses over an existing destination (renameat2 RENAME_NOREPLACE,
    renamex_np RENAME_EXCL). Another filesystem is refused unless `--allow-copy`, which copies through
    `.semu-partial`, counts files and bytes against the source, renames the copy in and keeps the source.
    A symlinked root or entry is reported and never followed (only the last component is checked, so the
    Mac's linked ~/Drive works). Empty folders, folders that are not mods and entries already in the library
    stay where they are; the first declared source wins (Azahar before Lime3DS). One run at a time
    (`semu-mods-migrate.lock`, a dead holder's lock is taken over); every move is appended to
    `semu-mods-migrate.tsv` and `--detach` writes `semu-mods-migrate.log`, all in the global state root. A
    second run moves nothing; `--reverse` renames recorded moves back when their old place is free. Nothing
    is deleted. Cemu's downloaded packs are not declared: they are folders of packs.
    Observed: the dry run over the owner's Drive library (read-only) plans 6 renames (OoT 3D and MM3D mods;
    OoT 3D, ALBW and MM3D textures; TotK Optimizer), 5 Lime3DS duplicates and the two empty 00040000000AEB00
    folders. A scratch tree on the same Drive APFS volume: both entries kept their inodes, a second run moved
    nothing, reverse restored them. Podman VM (Linux aarch64, kernel 7.1, overlay): renameat2 kept the inodes
    and `--detach` logged "done: 3 moved, 1 duplicate, 1 empty". With the library on tmpfs, the run was
    refused without `--allow-copy` and copied with the sources kept.
    Owner workflow. Deck: `~/Applications/Semu/bin/semu-deck-cli mods migrate --target steam-deck --dry-run`,
    then the same command with `--detach`, then `tail ~/.local/share/semu/semu-mods-migrate.log` and
    `mods list`. Mac: `semu mods migrate --target macos`. Open: both runs. The Deck needs a release with
    this command, and the owner's libraries are not changed by the agents.
- Item 1 (N64 glow): built 2026-10-04 and checked in the render host; the Deck is open. Two causes:
  angrylion handed over its black overscan columns, so the lip mirrored black, and the package had the
  weakest reflection of the TVs (0.2, from upstream's HSM_REFLECT_GLOBAL_AMOUNT 20). RetroArch now sets
  `mupen64plus-angrylion-overscan = "enabled"` in the linux and macos slices (both checked against the
  core-options fixture), and tv-soqueroeu-n64 mirrors at 0.4 like genesis, gc and ps2
  (`provenance.edited`, so `semu bezel emit` keeps it). PSX and Wii share the thin lip and 0.2; they stay
  as they are unless the owner wants every TV to match. Render host at 1280x800 and 1920x1080: the N64
  lip carries a brighter blue/green mirror; snes and genesis are unchanged (AE 0).
  The cropped frame stays on whole steps: angrylion hands RetroArch maxhpass-minhpass by vres, 625x237
  for the usual 474-line VI span (mupen64plus-libretro-nx f275caf4 vi.c:456-462), and the RetroArch
  bridge reports the frame's own size as the surface's native size (surface_contract.btrc:193, positive
  geometry policy), so game and bezel placement step in 237s: 711 (3x) at 1280x800, 948 (4x) at 1080p,
  474 (2x) in bezel placement (render host with SEMU_RENDER_SURFACE_0_NATIVE=625x237, and the placement
  contract). Observed with the real core in the podman VM on a3d0dc6 (`tests/integration/live-switch.sh`,
  mupen64plus_next booting PeterLemon's HelloWorldCPU32BPP320X240.N64 at 1280x800): the renderer read
  native 625x237 (Hide overscan in force; without it vi.c:464-468 sends 640x240) and drew 948x711 in
  game placement and 686x515 in fit, and both live switches still applied. The test program draws on
  black, so the lip glow itself waits for a real game on the Deck. Logged, as only a RetroArch patch
  would avoid it: RetroArch's own integer scale works from the core's declared geometry, 640x480,
  which the core forces for angrylion (libretro.c:1449-1456), so
  inside RetroArch's viewport the 237 lines are point-stretched 480/237 = 2.03x before Semu reads them
  back at native size (a linear blit to 625x237, `RendererShaderPasses.extract`). Fit placement, the
  default, is fractional by design either way.
- Item 5 (PSP background): built 2026-10-04 and checked in the render host; the Deck is open. The carbon
  was Duimon's Canvas_Background.jpg layer: 3840x2160 on the PSP's 5335x2160 canvas, so it was emitted as
  cover and stretched over the desk-night wood. It is hidden (`visible: false`, the editor's eye) in
  psp-e1000 and psp-red, and in gb-dmg-shell, gbc-shell, gbc-berry, gba-shell and gba-arctic, where bezel
  or fit placement shows it (nds-shell keeps its canvas-sized one under the opaque face); `semu bezel emit`
  now keeps a layer the package hides. The off-centre look: game placement centred the picture, and the
  E1000's screen sits above the device's middle, so the device sat 31 px low with a 53 px band on top
  only. Ruling taken as a default (reversible): game placement centres the shell on an axis where the
  whole shell fits, by whole pixels (`RendererPlacement` in the new GL-free renderer_placement.btrc,
  mirrored in the editor). At 1280x800 the PSP shows the whole device on wood bands of about 21 and
  22 px, the picture still 960x544 at x=160 (rows 97..640, was 128..671). gb, gbc, gba, nds and n3ds
  render identically (AE 0 at both sizes): their shells do not fit, so their pictures stay centred.
  editor-sync matches production for psp, gb, gba and n64 at both sizes. Contracts: placement.btrc (the
  strengths, no carbon layer on any psp/gb/gbc/gba variant, the PSP's bands, the Game Boys' centred
  whole steps, the 237-line steps) and bezel_emit's hidden layers, each failing under its mutation.
  Open, the owner's call: at 1920x1080 the PSP's 3x shell (1178 px) is taller than the screen, so the
  picture stays centred, a thin wood strip shows at the top and the button bar is half cut; centring the
  shell there needs a per-system rule, and applied to all it pushes the gb, gbc and gba pictures to the top.
- Item 4 (Wii and Wii U lag): cause named 2026-10-04, Semu's settings built and checked in the podman VM;
  the Deck's speed numbers are open (task 14's matrix). The main limit is the SD card, which only the owner
  can change. Measured read-only on the Deck: 2.9 MB/s sequential (64 MiB of Kirby's Epic Yarn's .wbfs in
  4 MiB direct reads took 23.5 s), 48-105 ms per random 128 KiB read (Skyward Sword's .wbfs), and 53.7 ms
  per read since boot; on 2026-10-04, 4 h after a boot, /sys/block/mmcblk0/stat again said 57 ms per read
  (27565 reads, 1584 s of read time, 2.5 MB/s while busy) against 0.29 ms on the internal NVMe, which reads
  2.0 GB/s. The bus negotiated UHS-I SDR104 correctly; the card is a SanDisk SD1T5 1.4 TiB from 05/2024
  (A1, V10), and a healthy A1/A2 card does about 85-90 MB/s and 1-2 ms in a Deck. So a game streams its
  disc 30 to 50 times slower than the slot allows: Cemu spent 17.5 s between its recompiler and Vulkan
  start reading Smash's .wua, and Dolphin's LoadGameIntoMemory would need about 23 minutes for a 4 GB Wii
  disc. Every cache (Dolphin's shaders, Cemu's shader cache, Ryujinx's PTC, Mesa's) already lives on the
  NVMe under ~/.local/share/semu. In the matrix Mario Kart 8 presented 46-48 fps against 59.94; a second
  limit, Semu's own, is named here and not fixed: the Vulkan layer (Cemu, Ryujinx, Azahar) waits on the
  GPU at every present, fences in `submit` and glFinish in `compose` (semu_vulkan_core.btrc:279-327),
  even when Wii U draws no bezel and no shader. Passing such frames straight to present is the design's
  passthrough task, open because it must keep the 3DS's whole-step layout with the bezel off.
  The owner's options: test the card read-only in another reader and replace it (the biggest win); or,
  on the owner's word, keep the Wii and Wii U games being played on /home (201 GB free). That crosses
  filesystems, so it is a copy, never a rename (the mods migration's `--allow-copy` rule), and Semu does
  not move ROMs unasked.
  Builds: no emulator is a debug build. Every Linux emulator compiles from its pinned source as Release
  (nixpkgs cmake setup-hook :88; dotnet Release for Ryujinx; RetroArch and its cores without DEBUG, at
  their release optimisation), with no assertions linked (0 `__assert_fail` imports in Dolphin and Cemu).
  - Cemu is GCC 15 at -O2 with LTO: nixpkgs swaps CMake's Release flags for `-DNDEBUG`
    (pkgs/by-name/ce/cemu/package.nix:113-114), leaving the cc-wrapper's -O2, and Cemu turns LTO on
    (CMakeLists.txt:74-75). Flathub's build, the one most Decks run, is the same class: the freedesktop
    SDK's GCC at RelWithDebInfo (-O2, LTO on; it installs `Cemu_relwithdebinfo`). Upstream's AppImage is
    clang-15 at -O3 with LTO (.github/workflows/build.yml:42, 69). -O3 is logged, not taken: GCC at -O3
    is a combination no upstream channel ships or tests, the gain is a few percent, and nothing here can
    play a Wii U game to catch a miscompile (the VM's Cemu segfaulted in its PPC interpreter on lavapipe on
    2026-09-25; the Deck is read-only for this work). Matching upstream exactly means clang with lld,
    verified on the Deck.
  - PCSX2 is clang 21, Release, Multi-ISA, without LTO (nixpkgs pcsx2/package.nix:50, 70-74); upstream
    links with `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` and lld (linux_build_qt.yml:137-142). Logged,
    not taken: it needs lld wired into the recipe and the render hook's static loader archive linked under
    LTO, a long rebuild for a few percent on PS2, which the owner did not report as slow.
  Settings Semu owns (reversible defaults, the owner being unattended):
  - Dolphin runs dual core: Dolphin.ini [Core] `CPUThread = True`. Dolphin 2606a enables it only on
    Android (MainSettings.cpp:59-65), so every Wii and GameCube game ran CPU and GPU emulation on one
    thread; the 98 GameSettings INIs that need one core still win (GlobalGame before Base, Enums.h:39-47).
  - Dolphin compiles shaders behind hybrid ubershaders: GFX.ini `ShaderCompilationMode = 2`
    (AsynchronousUberShaders), so a new shader no longer stalls a frame; OpenGL on radeonsi compiles in the
    background (OGLConfig.cpp:688-690). `WaitForShadersBeforeStarting` stays at Dolphin's False: with
    ubershaders on it would queue every ubershader pipeline before the boot (ShaderCache.cpp:69-75).
  - `visual.performance_overlay` (VISUALS: SPEED OVERLAY AND FRAME LOG, off by default) turns on Dolphin's
    ShowFPS, ShowSpeed and LogRenderTimeToFile, which writes Logs/render_times.txt per presented frame and
    vblank_times.txt per VI in Dolphin's user directory. The overlay sits at the window's top-right, outside
    the 4:3 picture the compositor cuts on the Deck, so there the log is the measure.
    `tests/deck/system-matrix.sh` takes `SEMU_MATRIX_SETTINGS` as every launch's `--settings-json` (the
    owner's semu.json is never written), copies a Dolphin case's two logs and notes their rates (VIs per
    second against 59.94 is the speed; presents undercount, as Dolphin skips duplicate frames), and notes
    when the first frame was composed, so a run after a release and a second run give the first and second
    boot times. A heavier case joins: Mario Kart Wii's attract race at 60, 120 and 180 s.
  - The Deck plays the Switch handheld: `input.systems.switch.play_mode` is docked in defaults.json and
    handheld in the steam-deck target (`semu settings put input.systems.switch.play_mode docked` docks it
    back). Handheld writes Ryujinx `docked_mode: false`, so games read the handheld operation mode
    (ICommonStateGetter.cs:92-94) and render at their 720p-class handheld resolution for the 800-line panel
    instead of 1080p (the TOTK Optimizer's own [Handheld] block asks for 1280x720), and the pad becomes the
    Handheld controller. Checked against Ryujinx 1.3.3's rules (e2143d43): a Handheld controller always sits
    at the Handheld index (NpadDevices.cs:142-145) and is dropped while docked (:103 in Validate, :186 in
    Remap), so docked keeps the Pro Controller at Player1; undocked, Validate counts the Handheld pad for
    every title that accepts the Handheld style and id, which every handheld-playable title does, so the
    controller applet (ControllerApplet.cs:88) returns without a dialog. Both of the owner's Switch titles
    (Animal Crossing, Tears of the Kingdom) play handheld. A TV-only title would show the applet, as a real
    Switch asks for detached Joy-Cons.
  Contracts: performance.btrc (dual core, hybrid ubershaders and no boot wait on linux-desktop, steam-deck
  and macos; the three overlay keys False by default and True with the setting; the VISUALS switch; the
  Switch modes per target and under overrides, each a pairing Ryujinx accepts; an unknown mode is a
  diagnostic) and deck_harness's matrix lines; 13 mutations each failed their checks.
  Observed in the podman VM (Rosetta, llvmpipe and lavapipe, Xvfb at 1280x800; GameCube and Wii Animal
  Crossing, New Horizons, the keys and firmware mounted read-only), this tree against HEAD 27862dc:
  - Dolphin with this profile ran a "CPU thread" and a "Video thread" where HEAD's ran one "CPU-GPU thread"
    (the names Core.cpp:327-329 give them); its ubershader pipeline cache filled to 14.2 MB in the
    background, HEAD's stayed at 48 bytes; the first frame was composed as fast on both, 6.6 s on a first
    boot and 4.4 s on the second (GameCube), 7.2 s on the Wii disc's first boot, and the title screens draw
    the same. With the overlay on, render_times.txt and vblank_times.txt filled (1316 VI lines in 120 s of
    City Folk, about 10.6 per second: software rendering under Rosetta, not a speed figure), and the overlay
    showed nowhere inside the cut.
  - Ryujinx through the steam-deck target, with a replica of Steam's virtual pad
    (tests/visual/virtual_pad.btrc): handheld logged "Configured Controller Handheld to Handheld" and
    "Connected Controller Handheld to Handheld"; New Horizons asked for ProController, Handheld and
    JoyconPair on Player1 and Handheld, and in 3 minutes no controller applet and no "No matching
    controllers" line followed. Docked logged "EnableDockedMode set to: True" and the Pro Controller at
    Player1. Lavapipe does not take the game past its boot frames, so its picture waits for the Deck.
- Item 2 (right trackpad pointer), the cursor (radial follow-up F3): built 2026-10-04 and observed in the
  podman VM; the Deck rerun of radial-check cases 3, 4 and 6-8 is open. Standalone Azahar had no Semu arrow
  (semu_compose zeroed the frame's cursor; only Azahar's Qt cursor showed). Now semu-touch.patch hands every
  GRenderWindow mouse move, press, release and leave to `semu_pointer_sample`, which the Vulkan layer and
  the macOS stand-in export (found as semu_touch_unmap is, retried until the layer has loaded); SemuCompose
  keeps the last sample under its own mutex and fills each frame's cursor through `SemuCursorPolicy`
  (src/renderer/retroarch/cursor_policy.btrc: header-free, the one RetroArch's bridge uses, gated on
  SEMU_RENDER_TOUCH_SURFACE_INDEX, kept beside the bridge because only that directory reaches RetroArch's
  build). While Semu draws, the render widget's own cursor is blank (on the child widget, so GMainWindow's
  show and hide of render_window's cursor never bring it back): one arrow. Without Semu the patch changes
  nothing. A held press is now activity (a drag that pauses keeps the arrow; it hides 3 s after the
  release), and SEMU_RENDER_DEBUG logs `semu-renderer: cursor shown|hidden X,Y ms=<epoch ms>` once per show
  and hide. Harness: the Deck's cursor-N was shot at +0.5 s while inject.sh's move landed 0.9-1.16 s after
  its time (touch_rect forked per process over /proc); inject.sh now finds the emulator with one
  `grep -l -z` over /proc/*/environ, reads the touch rectangle a second before the move is due and notes
  each step's move, press and release times; input-check.sh shoots cursor-N at +1.5 s and idle-N at +5.0 s
  and writes `arrow-N: ok` when tests/deck/cursor-arrow.sh finds the whole arrow (at 800 rows 276 white
  fill and 196 black outline pixels) with its tip where inject.sh put the pointer in cursor-N and not in
  idle-N; without ImageMagick on the Deck the verdict reads unchecked and the script runs on the fetched
  shots. On the F3 Deck captures it reads 276/276 and 196/196 in nds idle-2 at 954,369 and in idle-3 at
  701,537, and nothing in the early cursor-1 and cursor-2.
  Observed in the podman VM (Rosetta, lavapipe and llvmpipe, Xvfb at 1280x800), this tree on 034d62d:
  `tests/visual/vm-azahar-layer.sh` with OoT 3D (a scratch copy of the ROM) and Azahar rebuilt with the
  patch: 1.5 s after a relative move to 40,44 xwd held the whole arrow there (276/276, 196/196), maim with
  and without the X cursor were identical (Azahar's own cursor blank), and 5 s after the move the arrow was
  gone; the renderer logged shown at the move and hidden 3.2 s later, and a 2 s held click kept it shown
  until 3 s after its release. `tests/integration/touch-x11.sh`, its moves now relative from the top left as
  on the Deck: PASS on both RetroArch routes (the taps within 1 percent, the bezel tap pressing nothing, the
  arrow whole at 1.5 s and gone at 5 s). macOS builds the stand-in with `_semu_pointer_sample` exported; its
  picture is not checked (no window on the Mac display). Contracts: right_trackpad (held press, the debug
  transitions and line, the shared policy), standalone_cursor (the index gate, compose's cursor under the
  sample lock before the renderer reads the frame, both exports and the version script, the patch's
  forwarders and blank cursor, cursor-arrow.sh's rows equal renderer_cursor's arrow) and deck_harness (the
  capture times, the one-grep lookup ahead of the due time, the step notes, the arrow verdict); 11 mutations
  each failed their checks.
- Item 2 (right trackpad pointer), the Wii Remote (decision F2): built 2026-10-04 and observed in the podman
  VM; the Deck check (the four picture corners in a pointer menu, a soft press as A, the Deck's gyro through
  the SteamDeck backend) waits for the owner's next release. Dolphin aims the IR by the X pointer inside its
  window, scaled so its own 4:3 letterbox spans -1 to +1 (XInput2.cpp:268-313), while Semu shows that
  picture smaller inside the TV bezel, so the IR drifted away from the finger. Now wii declares
  `display.surface_mapping.pointer_screen: "main"`, the launcher emits SEMU_RENDER_POINTER_SURFACE_INDEX=0
  (no other system names one; a name that is no declared screen is refused), and libsemupreload interposes
  XIQueryPointer (exported in preload.map): it asks libXi, then hands Dolphin the point under the finger on
  the composed picture as the point Dolphin drew there (SemuCompose.unmapOn on that screen, clamped, through
  the frame's pointer map and SemuFrameLayout.drawnPoint, the arithmetic the touch path now shares), so the
  IR reaches exactly the picture's edges whatever the bezel, and a finger on the bezel holds it at the
  nearest edge. SEMU_RENDER_DEBUG logs `semu-preload: pointer X,Y -> X',Y'` once per new window point. The
  IR keeps Dolphin's defaults (Vertical Offset 10, Total Yaw 25, Total Pitch 20) with Auto-Hide off (it hid
  the pointer 2.5 s after the last move, and no button brought it back); Dolphin.ini [Interface]
  `CursorVisibility = 0` blanks the X arrow, and Semu draws no arrow on Wii (its cursor is gated on a touch
  screen), so the one pointer seen is the game's own (reading of F2's "one visible pointer", reversible).
  inject.sh aims a Wii case at the pointer screen. macOS is not covered: Dolphin there reads the pointer
  through its Quartz backend, which the window shim does not interpose.
  Observed in the podman VM (Rosetta, llvmpipe, Xvfb and openbox at 1280x800), this tree on a1ff828:
  `tests/integration/wii-pointer.sh` with Wii Sports + Resort (read-only mount) at its disc menu, the
  picture composed at 297,97 686x515 in the TV bezel: all 16 relative moves (the four corners, 5 percent in
  from each, 3 percent in from each edge, a quarter, three quarters, the centre and a bezel point) logged the
  expected Dolphin point within 1 px (corners 107,0 / 1171.4,0 / 107,798.4 / 1171.4,798.4, centre 640,399.2,
  the bezel point 107,0). By eye (OUT/sweep.png, a crop with a cross per target): the game's hand follows
  the finger and is at the centre at the centre; near the bottom edge its body reaches the picture's bottom.
  This title's own IR mapping is not 1:1 at Dolphin's defaults: the hand moves about 1.34 times the finger
  across (a quarter in put it at 0.165 of the width, three quarters at 0.826), so it reaches the side edges
  with the finger about 17 percent in and is off screen (hidden) beyond, and it sits about 48 px above the
  fingertip (26 px near the bottom), hidden in the top 3 percent. The old Semu tuning (Total Yaw 19) would
  bring this title near 1:1 across; F2 keeps Dolphin's defaults, so retuning per title is the owner's call.
  The keyboard Return is both Wii A and HOME in the profile (a press in the VM opened the HOME menu), and the
  pad has no HOME: flagged as its own task. Contracts: wii_pointer (the routing for wii and none for gc, nds,
  n3ds; the remap arithmetic over a published map, clamped and unclamped; the XIQueryPointer hook order, the
  clamp and the declared screen; the export; the IR values and no Auto-Hide in WiimoteNew.ini and the three
  Semu profiles on both targets; CursorVisibility = 0; the injector), right_trackpad (unmapOn reads one
  frame's map under the lock); 10 mutations each failed their checks.
- Item 3 (radial v2), part 1 (Fit, Aspect, Reset, the menu rows; controller layouts and players follow):
  built 2026-10-04, checked in the render host and the podman VM; the Deck is open (it needs a release and
  `semu-deck-cli steam input` with Steam stopped, which installs the new slots). Rulings, as reversible
  defaults (the owner being unattended):
  - The default placement does not change (fit on TVs, game on gb/gbc/gba/psp, bezel on the DS and 3DS,
    whose shell looks the same in fit and bezel). Fit toggles this system between bezel (the whole shell at
    its largest whole step) and screen (game: the largest whole step the screen holds, the shell cropped),
    live through journal code 81 (the renderer's config.placement after the toast's two frames, nothing
    reloads, kept across a context reset) and saved as `visual.systems.<id>.placement`. A default outside
    the toggle (fit) sits after it in the variants file's placements, so the toggle never returns to it;
    Reset does. Bezel placement is always whole. Screen placement goes fractional only where a 3D-era
    system opts in through `display.scaling.game_fractional_below: 2` (wii, gc, ps2, dreamcast): their
    480 lines hold only 1x on the 800-line screen, so screen fills it (800 lines, 1.67x) and bezel stays 1x.
    On the DS and 3DS, screen drops the shell (`RendererPlacement.shellHidden`) and draws the computed
    whole-step layout (stacked, 2x at 1280x800) over the package's wood background, screens plain.
  - Aspect: a system declares `display.outputs` (Wii: standard 4:3, widescreen 16:9) and each bound
    emulator its arguments per output (Dolphin `platforms.<os>.outputs.wii`: SYSCONF.IPL.AR=False, or True
    with the CustomStretch pin moved to 16:9), placed before args, which end on `-e` and the game. Bezel
    variants say which output they are drawn for (`output`) and their partner for another (`outputs`): the
    Wii's tv_wide is drawn for 16:9 and leaves the 4:3 cycle, and stays the 4:3 TV's _B partner. The output
    is chosen per game (`visual.games.<id>.<game file stem>.output`), so a 4:3 title is never stretched by
    another game's choice. Dolphin pins it at boot, so the first press only prompts ("16:9: AGAIN TO REBOOT
    GAME", code 82 reserved 1) and saves nothing; a second press of the same choice within 3 s saves it,
    says "ASPECT 16:9: REBOOTING" and restarts the game in place: the supervisor stops the emulator as on
    quit, waits for its group, and `semu launch` plans the same game again from semu.json (at most 8 times;
    ES-DE keeps waiting on the same process). An outside stop never restarts. The command-line layer sits
    below game INIs (Enums.h SEARCH_ORDER), so a GameSettings INI setting the aspect would win; none does.
    The checker refuses an output an emulator gives no arguments for, a variant drawn for an undeclared
    output, and a default bezel with no bezel for an output.
  - Reset to Default asks for a second press within 3 s (code 85 slot 0; reserved 2 when the running output
    is not the default: "RESET: AGAIN TO REBOOT GAME"), then removes `visual.systems.<id>`,
    `visual.games.<id>` and `input.systems.<id>.players` from semu.json (SemuSettingsStore.remove, which leaves a
    malformed file untouched), journals the default bezel, shader and placement as absolute selects (live)
    and 85 slot 1 ("RESET TO DEFAULTS"), and restarts only to change the output back.
  - Nothing to choose says so: a select with slot -1 shows the action's unavailable toast (BEZEL: NONE
    HERE, SHADER: NONE HERE, FIT: NO BEZEL, ASPECT: ONE ONLY). Toasts stay data: any `NAME_toast` of
    an input.json action reaches the renderer as `action:NAME` (loading, unavailable, confirm, restart, done).
  - Chords: Fit Ctrl+Shift+F, Aspect Ctrl+Shift+G, Reset Ctrl+Shift+R. Aspect is not Ctrl+Shift+A: Dolphin
    (ExpressionParser.cpp:565-590) and PCSX2 (InputManager.cpp:1080-1170) fire a binding when extra modifiers
    are held, so it would also load a state (Ctrl+A). Their profiles declare `native_shortcuts.
    superset_modifiers` with the source, and a generic contract refuses any chord Steam sends whose key a
    superset matcher binds with fewer modifiers.
  - The radials: the quick ring is Save, Load, Previous and Next Slot, Next Bezel, Next Shader, Fit,
    Aspect, Menu, Screenshot and Quit (11, centre empty); the held ring (View, L4, R4) is Up, Down,
    Confirm, Back, Bezel On/Off, Shader On/Off and Reset to Default, Open Menu in the centre. Icons from
    Lucide (scaling, ratio, rotate-ccw) through render-icons.sh. The Semu menu shows FIT where a bezel can
    be placed and ASPECT where a system has two outputs (a row's `when`), RESET TO DEFAULT everywhere; it
    holds up to twelve rows and draws only as tall as its rows (never below the eight-row size, so short
    menus and the toasts keep their scale); left and right step every value row. On macOS, whose supervisor
    reads no keyboard, they are reached from the pad's menu (Select+Y).
  Contracts: radial_choices.btrc (codes and toasts; remove; Fit on gba, wii, wiiu and nds; Aspect's
  prompt, window, per-game save and restart; Reset's prompt, removal, defaults and Wii restart; the menu
  rows), radial_render.btrc (an 81 record through the post-UI state, applyPlacement and reapply; the
  prompts and unavailable toasts; the rows' values and height; whole steps for bezel placement and the
  opt-in fill; the DS shell; the Wii's 16:9 environment, bezel choices and Dolphin arguments per game; the
  outputs checker; the superset-modifier collisions), and the updated variants, Steam ring, Dolphin argv
  and menu checks. 16 mutations each failed their checks: remove doing nothing, applyPlacement or its
  reapply dropped, the opt-in dropped or applied to bezel placement, a silent nothing-to-choose, Aspect
  saving on the first press or per system, the output arguments ignored, Reset without its window or its
  defaults, the menu filter dropped, Aspect on Ctrl+Shift+A, the DS shell kept, the outputs check
  unhooked, the prompt toast dropped.
  Render host (1280x800, Read by eye): snes and wii toast FIT: BEZEL then show the whole TV at a whole
  step, and FIT: SCREEN then crop it (snes 3x; wii filling the 800 lines); nds FIT: SCREEN shows both
  screens stacked at 2x on the wood without the shell; gba FIT: BEZEL the whole shell; the wii menu reads
  FIT BEZEL and ASPECT 4:3 over ten rows; the prompts and the unavailable toast read as written; a Wii game
  on 16:9 fills the 16:9 TV's hole.
  Observed in the podman VM (Xvfb 1280x800, llvmpipe; tests/integration/live-switch.sh with
  CHORDS="ctrl+shift+f ctrl+shift+r,ctrl+shift+r ctrl+shift+g,ctrl+shift+g", typed as XTest the way Steam
  does): RetroArch gba (240p Test Suite) drew the menu with FIT SCREEN and RESET TO DEFAULT; Fit journaled
  81/0, the renderer logged the placement and drew the whole arctic shell at a whole step with FIT: BEZEL;
  Reset journaled 85/0 (RESET: PRESS AGAIN) and, a second later, 79/0, 80/0, 81/1 and 85/1: the shell, the
  GBA LCD and the screen placement came back live (RESET TO DEFAULTS) and semu.json lost its gba entry;
  Aspect journaled 82/-1 (ASPECT: ONE ONLY). Real Dolphin 2606a with City Folk (mounted read-only): the menu
  showed FIT FIT and ASPECT 4:3; after Fit and Reset, Aspect twice saved
  visual.games.wii."Animal Crossing - City Folk (USA, Asia) (En,Fr,Es) (Rev 1)".output = widescreen,
  stopped Dolphin and started it again in the same `semu launch` (restarts=1), whose argv held
  `--config SYSCONF.IPL.AR=True --config Graphics.Settings.CustomAspectRatioWidth=16 --config
  Graphics.Settings.CustomAspectRatioHeight=9`; a minute later the game's 16:9 strap screen filled the 16:9
  TV, its variants file offering tv_wide and none.
- Item 3 (radial v2), part 2 (controller layouts and players): built 2026-10-04, checked in the render host
  and the podman VM; the Deck is open (a release, then `semu-deck-cli steam input` with Steam stopped for the
  two new slots). Rulings, as reversible defaults (the owner being unattended):
  - A system declares `controllers` (Wii: Wii Remote, Nunchuk, Classic, GameCube; default Nunchuk) and
    `players.max` (wii, gc, n64, dreamcast, switch, wiiu 4; snes, nes, genesis, psx, ps2 2; the rest 1); an
    emulator `players.max` (RetroArch 8, Dolphin 4, Ryujinx 4, Cemu 4, PCSX2 2; the others 1). A game takes
    the smaller. Choices are saved per system: `input.systems.<id>.players.<n>.layout` and `.pad`.
  - Pads are dealt at launch from a roster (Linux: the evdev pads SDL is not told to ignore; Steam's virtual
    pads first in slot order, named from Steam's `SteamVirtualGamepadInfo` slot file as SDL names them).
    Each pad carries what emulators match by: SDL 2.26's GUID with the CRC of the evdev name (checked
    against the Deck's own 030079f6de28..., Cemu), Ryujinx's CRC-less spelling and its own duplicate count,
    and Dolphin's per-name SDL/<n>/<name>. Player 1 on the first pad keeps the target's device_identities
    exactly (so a one-pad launch renders as before); player 1 always plays. Without a roster (macOS)
    player 1 holds the default pad and the others none.
  - The profile compiler has one new construct, `for_each` over `players` (or `player_layouts`, each
    connected player with each layout the emulator switches live), on a line, a JSON array element or a
    whole file, filtered by `when` (row fields). RetroArch writes input_playerN_joypad_index per player (a
    player without a pad gets an index no pad has, so RetroArch's own default never doubles a pad; without
    a roster only player 1); Dolphin SIDevice/WiimoteSource per player, [WiimoteN] and [GCPadN] on each
    player's pad; PCSX2 [Pad2] on player 2's SDL pad, no keyboard; Ryujinx a Pro Controller per further
    player (Player2-4); Cemu controller1-3.xml as Wii U Pro Controllers (ProController::ButtonId 1-25 on the
    GamePad's SDL buttons). Player 1 aims the Wii Remote with the pointer and reads the Deck's motion; the
    others aim with their right stick and have no motion: no file of one player names another's device
    (a contract over every stored profile and [WiimoteN]).
  - Wii layouts switch live through Dolphin's own profile cycle (no Dolphin patch): each connected
    player's live layouts are rendered to Config/Semu Layouts/P<n> <Layout>.ini; a switch copies the
    player's file over Semu's one file in Config/Profiles/Wiimote and types that player's Next Profile key
    (Alt+F5..F8, profile.json layout_hotkeys). Dolphin lists that folder sorted at every press and keeps one
    index for all remotes, so with Semu's file alone there every switch is one press whatever the index
    was (the resync: a lost press changes nothing and choosing again redoes it); other files are counted
    from the index Semu tracks since boot. The files older releases generated there (Semu Wiimote,
    Nunchuk, Classic) move once to Config/Semu Retired Profiles, never over anything. Presses are held
    40 ms and 40 ms apart (emulator.json controllers; the checker refuses under 30: Dolphin polls every
    5 ms and needs the release), typed through XTest on the game's display when the supervisor reads keys
    there (Game Mode's Xwayland), else Semu's uinput keyboard; with neither (macOS) a layout applies by
    restarting. GameCube (Wii Remote disconnected, read at boot) and leaving it ask for a second press
    (journal 83 reserved 16 + player) and restart the game (32 + player); Reset returns every player to
    the default, live where it can. The Undo Load/Save State hotkeys are unbound: Dolphin matches superset
    modifiers, so the bare F12 fired on Steam's Ctrl+Shift+F12 (the Wii layer's GameCube).
  - Radial: Controller Layout (Ctrl+Shift+C, Lucide gamepad-2) joins the quick ring after Aspect and steps
    player 1; Players (Ctrl+Shift+U, Lucide users; not P: Ctrl+P pauses under superset matchers) joins the
    held ring; the Wii layer's four slots pick that layout for player 1 through the same code. Menu:
    CONTROLLER (player 1's layout, a value row) where layouts apply, PLAYERS where the game takes more than
    one. The players page lists P1..Pn with pad and layout, then RESTART GAME and BACK: confirm moves a
    player to the next pad no one holds (or none; player 1 always holds one), left and right step its
    layout, RESTART GAME restarts only after a change ("PLAYERS: NO CHANGES"). The supervisor writes the
    rows to semu-render-players.txt before each record (86 page, 84 pad), so the renderer draws exactly
    what it mirrors.
  Contracts: players.btrc (roster order, names, GUIDs; rows, saved pads and the lowest free pad; each
  emulator's per-player output; the identity contract; the checker) and controller_layouts.btrc (the
  paced timeline with a fake clock, the press count, XTest's keycode, live switches with a stray profile,
  GameCube's confirm and restart, Reset, the page's pads, RESTART and BACK, the renderer's page and
  toasts, a real launch plan dealing players and the menu rows); 21 mutations each failed their checks.
  Render host (1280x800, by eye): the players page draws the four player rows, RESTART GAME and BACK under
  the toast P1: CLASSIC, without the slot line.
  Observed in the podman VM (Xvfb 1280x800, llvmpipe, a rootful run with /dev/input bound;
  `PADS=2 BASICS=0 tests/integration/live-switch.sh` with two replicas of Steam's pad on slots 0 and 1 and a
  slot file; real Dolphin 2606a with City Folk mounted read-only): the launch dealt P1 PAD 1 and P2 PAD 2
  (Dolphin.ini SIDevice1 = 6, WiimoteSource1 = 1); the menu showed CONTROLLER NUNCHUK and PLAYERS among its
  twelve rows; Ctrl+Shift+C (XTest, as Steam types) journaled 83 slot 2, logged "P1 layout classic: 1
  press(es) of Alt+F5 via X", inotify saw Dolphin open Semu.ini once, and Dolphin's own message "...profile
  'Semu' for device 'Wiimote1'" showed on the picture under the toast P1: CLASSIC; semu.json saved
  players.1.layout classic. Ctrl+Shift+U opened the page (P1 PAD 1 CLASSIC, P2 PAD 2 NUNCHUK, P3 and P4 NO
  PAD), confirm on P2 made it NO PAD, RESTART GAME restarted City Folk in the same launch: the new plan
  dealt P1 PAD 1 CLASSIC, P2 NO PAD and P3 PAD 2 (an unsaved player takes the lowest free pad: a player set
  to none leaves its pad to the next one), Dolphin.ini WiimoteSource0 = 1, WiimoteSource1 = 0, WiimoteSource2
  = 1. The first VM run found a launch that never dealt its players (the supervisor stopped on an empty
  roster vector); fixed, and a real-launch contract guards it.
- Item 3 (radial v2), part 3 (Steam Input): built 2026-10-04, checked offline; the Deck is open (a release,
  then `semu-deck-cli steam input` with Steam stopped). Rulings, as reversible defaults (the owner being
  unattended):
  - Steam's radial stays one template for all 17 systems, so the choices sit on a second radial reached from
    the quick ring. Its Settings slot (Lucide settings) is a Steam preset switch, `controller_action
    CHANGE_PRESET` with the label and icon in the binding: the form community radials on the Deck use for
    their Back centres (workshop item 3372611329 binds `CHANGE_PRESET 32765 0 0, Back, RD-user-red-home.png`
    on a radial_menu's touch_menu_button_0, read-only). It enters a settings page whose left pad is the
    settings radial: Next Bezel, Bezel On/Off, Next Shader, Shader On/Off, Fit, Aspect, Controller Layout,
    Players and Reset to Default, with Close Settings (Lucide circle-x, CHANGE_PRESET 1) in the centre.
  - The page stays until Close, so Next Bezel steps on and the second press of Aspect, GameCube or Reset
    lands inside its 3 s window. The rest of the controller is the gamepad there, and View's long press,
    the upper grips (hotkeys) and the lower grips (Wii layer) work from it as from the gamepad. The quick
    ring is Save, Load, the two slots, Next Bezel, Next Shader, Settings, Menu, Screenshot and Quit (10,
    centre empty): Fit, Aspect and Controller Layout moved to Settings. The held menu ring is unchanged
    (it keeps Bezel and Shader On/Off, Reset and Players next to the menu steering).
  - Data: `radial.pages` (id: label, icon, preset id), `radial.settings_slots` and `settings_center`, a
    `settings` preset with no `held_by` in each layout (the Deck's preset 2, so the Wii layer is now preset
    3; gordon's preset 4, after its grip presets), group kind `settings_radial` (group 40) and each
    template's `settings_radial_name`. A page finds its preset by id on each controller. The checker
    refuses a page that is also an action, a page without a preset, a preset a controller lacks, and a
    page preset that nothing holds and no page switches to.
  - Off-screen the preset switch cannot be exercised (it is Steam's), so `radial-check.cases` covers every
    slot that types a chord: case 14 (snes: Controller Layout and Aspect unavailable, the players page
    steered with Ctrl+Down, Ctrl+Space, Ctrl+Up and Ctrl+Backspace), 15 (gba: Bezel On/Off, Fit, Reset
    twice) and 16 (wii: Controller Layout live, Fit, Players, Aspect twice and its restart). A token
    `key:A,B` types B a second after A; the result's journal line now shows the reserved field.
  Contracts: steam_controllers.btrc (the ten-slot quick ring and its Settings switch to preset 2; the
  settings radial's nine chords and Close; the page equal to the gamepad but its left pad; View, the upper
  and the lower grips from it; gordon's switch found by id to preset 4; the four new refusals),
  steam_input.btrc (page icons declared and distinct, the Wii layer at preset 3), gameboy.btrc and
  emulator_hotkeys.btrc (every ring and centre, the settings radial's included, from the document's bound
  actions), deck_harness.btrc (codes 81-86 named, every chord-typing slot of every radial in the cases with
  its input.json chord, the double-press timeline, a dangling comma refused). 7 mutations each failed their
  checks: pages typed as chords, a page's preset found by position, page presets losing the holds, the
  missing-preset refusal dropped, gordon without its settings preset, the Players token removed from the
  cases, and inject.sh without the comma.
  Checked: `semu steam input --steam-root <scratch>` wrote the four templates, 32 icons and the Deck
  profile; in it quick button 7 is `controller_action CHANGE_PRESET 3 0 0, Settings, semu-settings.png`,
  group 40 "Semu Settings" holds the nine slots and Close on button 0, the presets are Gamepad, Hotkeys,
  Settings and Wii Controller Mode, and its braces balance. `render-icons.sh --check`: all 28 Lucide icons
  re-render at RMSE 0 (the new two read as a gear and a circled x, by eye). `input-check.sh --plan
  tests/deck/radial-check.cases`: 16 timelines, identical under bash 5.3 and 3.2.
  Open, on the Deck: Steam honouring the preset switch and Close in Game Mode (only the binding's form is
  evidenced), the 10 + 10 + 9 + 4 icons and their look, and cases 14 to 16.
- The Deck, release b1bf425 (2026-10-04, read-only checks over ssh; the owner ran `semu mods migrate`
  there, so the mods sit in the SD card's Emulation/mods). Observed:
  - Item 0: OoT 3D and Majora's Mask 3D load their 4K packs and patches from the central library (Azahar's
    log: the per-application config for the title, Utility_CustomTextures true, code.ips patching code.bin);
    Tears of the Kingdom logged "Found enabled mod '!!!TOTK Optimizer'", its exefs and RomFS replaced, on
    v1.2.1. Item 0's criterion is met on the Deck for the patch and textures loading; the pictures by eye
    and Continue in TotK stay for the owner's next play.
  - Item 2, 3DS: Semu's arrow on standalone Azahar after a tap (the whole arrow, 276 px of fill, at the
    tap's point plus 2,4) and none 5 s later. The very first move right after launch drew none ("cursor
    shown 0,0" was logged after the shot, at the next token's park): fixed below. Not observed: the Wii
    IR pointer (Dolphin's Wii Remotes read the Deck's controls through its SteamDeck backend, which the
    off-screen harness cannot drive).
  - Item 3: the radial v2 chords reach the supervisor (system.reset twice, visual.placement.next,
    visual.output.next, controller.layout.next, ui.players); Fit on the Wii shrank the picture into the bezel;
    the Wii's Aspect double press rebooted the game; GBA Fit pressed while the bezel was off showed no
    change (the test order; fixed below). Not observed: the players page and per-player layouts (the
    harness typed `ctrl+down`, `ctrl+up` and `ctrl+backspace`, which xdotool ignores in that case, so only
    Ctrl arrived; fixed below), Steam's own radial UI and the settings page (the owner's eyes only).
- Review of the M13 series (2026-10-04), fixed in 5f55703 and ff6e9df, each with contracts that
  fail under their mutations (11 and 29):
  - Azahar's first trackpad move: its patch reports a sample only on a mouse event, so the renderer's rule
    for RetroArch's polled pointer (a first sample is only a position) swallowed it. The standalone
    compositor now counts each event (a serial under the sample lock) and marks the first frame carrying
    it visible 2, which the renderer counts as motion; RetroArch's bridge passes fresh=false.
  - The Deck harness: inject.sh maps Semu's key names to X keysyms (down Down, backspace BackSpace, f5
    F5) and prints them in --plan; input-check.sh greps run.log with -a (a restarted Dolphin leaves NUL
    bytes there, which hid every action line of the Wii case), reads any cursor-arrow.sh status but 0 or 1
    as unchecked, and refuses to start without inject.sh and cursor-arrow.sh beside it (the Deck's copy
    lacked cursor-arrow.sh, so every arrow read FAIL; on the fetched shots arrow-2 and arrow-4 pass).
    Rerun on the Deck with release 5f55703 (2026-10-04, input-check.sh + inject.sh + cursor-arrow.sh
    from that commit): case 4 draws the 276-pixel arrow at the first move's target (cursor-1 box
    18x32+1096+532 for the move to 1094,528) and none 5 s later; case 14's players page is steered by
    Ctrl+Down, Ctrl+Up, Ctrl+Space and Ctrl+Backspace (ui.menu.down/up/confirm/back each from the
    keyboard); case 16 runs controller.layout.next, ui.players, visual.output.next x2 and
    visual.placement.next, saving wii placement "bezel".
  - Reset to Default removes `input.systems.<id>.players`, not the whole subtree, so the Switch's
    play_mode (docked on a TV) survives it.
  - Fit while the bezel is switched off journals 81/-1 with the unavailable toast, now FIT: NO BEZEL (it
    covers no bezel here and the bezel off), and saves nothing; the menu's FIT row stays (rows are fixed
    per launch) and says the same.
  - Dolphin on linux-desktop: a pad Steam does not own is named as SDL names it (input.json
    `players.sdl_names` by vendor:product, read from SDL 3 in the podman VM with uinput pads: 045e:028e
    Xbox 360 Controller, 045e:02d1 Xbox One Controller, 045e:02ea Xbox One S Controller, 045e:0b12 Xbox
    Series X Controller, 054c:05c4 and 054c:09cc PS4 Controller, 054c:0ce6 PS5 Controller; Nintendo's Pro
    Controller, Logitech's F310, 8BitDo's Pro 2 and the DualSense Edge keep their kernel names), so players
    2-4 get `SDL/<n>/Xbox 360 Controller` instead of a kernel name Dolphin never lists. The GUID keeps
    the kernel name's CRC (SDL's own). Logged, not changed: linux-desktop's default device identity names
    `SDL/0/Xbox Controller` for 045e:02ea, while SDL 3 names that id Xbox One S Controller over evdev; the
    owner's xone pads on FRACTAL-NORTH decide which is right, and nothing here can check them.
  - Style: paced_keys' `at` and `x` are offsetMs and xTestReady, and the style scan covers src/mods, the
    new launch, emit, checker and renderer sources and the M13 specs.
  - Contracts the review found missing, now in place: the case of updates.json's title folder and of the
    per-title settings file (listDir, exact: APFS ignores case, the Deck's card does not), a per-title file
    rewritten when its content changes, migrate refusing a linked library, a short copy never renamed in
    (`SemuModMover.adopt`), a leftover `.semu-partial` never an entry, reverse's "left" outcome, the
    pointer and touch screen diagnostics, the late XIQueryPointer bind, Left from the default fit, BPS
    beat numbers over two bytes, an exheader over 2048 bytes, a .PNG pack, a title's first character, a
    `$` in an entry name, a 17-character bracket token, two pads of one model (same_guid_index 0 and 1),
    a saved player 1 without a pad, player 1 never moved to none, Dolphin's recursive any-case profile
    listing, a padless player on GameCube at Reset, and the checkers' layout keys, single layout,
    undeclared output and partner.
  - Rulings (reversible, the owner being unattended): RESTART GAME after a layout already switched live
    still restarts from the players page (the page saw a change; every file then comes from the saved choice); the Restart Game action restarts only for what waits (review round, M14). Fit stays a
    two-way integer toggle (bezel and screen, the owner's "always integer scale"), so on TVs the default
    fit (on a TV room the nearest whole step since the M14 review round) is reached only by Reset, which also returns the
    players' layouts and pads and, on a 16:9 Wii game, reboots it.

### M14. Owner feedback from the Deck, round 2 (2026-10-04)

The owner played the e1fcb01 release in Game Mode and reported (verbatim where quoted):

1. **N64** "rendering is off centered on the bezel. loaded mario 64. also very laggy". Done when Super
   Mario 64 sits centred in the TV bezel at whole steps and runs at full speed on the Deck.
2. **GBC** "start select buttons are missing from" the shell. Done when the GBC shell shows them.
3. **Bezel fit** "on some systems, bezel fit has black bars because the bezel ends and doesn't repeat".
   Done when no system shows black bars in bezel fit (the scene continues past the art).
4. **Wii controller layouts** "changing wii controllers causes you to reboot even when it's not
   supported - we shouldn't press again to reboot, there should be a reboot button in the radial maybe,
   that way you can oscillate through and it works". Done when layouts cycle without rebooting and a
   separate Restart slot reboots once when a change needs it.
5. **Wii U** "hitting bezel in wii u caused it to hang? maybe it was just slow". Done when the cause is
   known and Bezel on Wii U never hangs.
6. **Dreamcast** "flickers, something looks wrong with the shader". Done when it no longer flickers.
7. **Reflections** "take the shader lines into account in a way that's kind of low resolution ... the
   reflections of Aero the Acro-Bat on Sega Genesis had a weird pixelated look". Done when reflections
   are smooth (mirror the clean picture, softened outward), checked by eye.
8. **CRT shader** "looks fake on the Sony PS1 white loading screen? looks like bands"; port Retro Crisis's
   "Realistic PlayStation RGB CRT Shader" (GDV-NTSC presets on crt-guest-advanced-ntsc,
   https://www.youtube.com/watch?v=cyktna9FF08) for the CRT systems if the Deck can run it. Done when the
   PS1 white screen shows no banding and the new preset runs at full speed on the Deck (measured).
9. **Speed** "make sure we are compiling our emulators with the recommended settings for speed".
   Done when every emulator is built with its upstream-recommended release flags (and the Deck's CPU
   level where safe), with the evidence listed.
(The owner's PlayStation quit report was holding Start+Select, which is by design.)

Status: tackled in three parallel tracks (performance, picture, controls), then review and the Deck.
- Item 4 (controls): built and contract-proven on the Mac; the Deck rerun is open. No choice restarts
  the game by itself any more. Controller Layout, the Wii layer and the players page step layouts
  freely, live where Dolphin switches them; GameCube (or leaving the GameCube a game booted on) is
  saved and toasts P1 GAMECUBE: RESTART TO APPLY. Aspect saves at one press (ASPECT 16:9: RESTART TO
  APPLY). Reset keeps its safety confirm but never reboots (RESET: RESTART TO APPLY). The new
  Restart Game action (Ctrl+Shift+D, Lucide refresh-cw icon, journal 87) sits in the settings radial
  after Controller Layout, in the centre of the held Wii layer, as the menu's RESTART GAME row (the
  Wii menu is 13 rows; the texture grew to 683 px, still scale 1 at 1280x800, seen on the render
  host) and as the players page's RESTART GAME. It restarts once when something waits (the players page: when anything differs from the
  launch), else toasts RESTART: NOTHING TO APPLY. radial-check.cases case 16 now cycles Classic, GameCube
  and Wii Remote without a restart, then applies 16:9 with one Restart Game.
- Item 5 (controls, Wii U Bezel): cause found and fixed, measured in the podman VM; the Deck rerun is
  open. The Deck's journal from that session (Mario Kart 8, 15:00) holds exactly four records within
  6 s, each Next Bezel as 79/-1 (Wii U has no bezel). Nothing reloads for those records. Through
  the real Vulkan layer in the VM, with the Wii U environment at 1280x800 (lavapipe and llvmpipe),
  `tests/visual/vulkan-present.c` now prints each present's time and journals a record at a chosen
  frame. The first press's frame took 0.78 s against a steady 15 ms, and a second press 18 ms: the
  driver built the blended menu program and the toast texture at the first toast. The renderer now
  draws the menu pass once at a context's first composed frame, transparent and one pixel, at the
  toast's texture size (RendererPostUiCompositor.warm). The first press then takes 21 ms, and the
  BEZEL: NONE HERE toast is pixel-identical (AE 0) and the picture clean. radeonsi on the Deck may stall
  less than llvmpipe, so the Deck rerun measures it. Bezel On/Off and Shader On/Off on a system with
  nothing to choose used to flip the global switch that every other system defaults to, flip the live
  config, and show no toast. They now journal 79/-1 and 80/-1 (NONE HERE) and change nothing (the
  legacy global switch stays only for launches with no variants file). Cemu's log also shows Mario
  Kart 8 starting its H264 video in the same second as the first press (Cemu's ih264d rejected the
  first slices), which may have slowed the game then too. Contracts: the Wii U presses (visual_cycle)
  and the warm-up wiring (live_variants), each failing under its mutation.
- Item 1 (performance, N64): built and checked in the VM with Super Mario 64; the Deck run is open
  (speed and the picture). The lag: Semu forced angrylion, a software RDP, with cxd4, an interpreted
  LLE RSP. The geometry: angrylion's Hide overscan handed over a frame whose size depended on the game.
  The Deck log shows 582x222 for SM64, which RetroArch stretched 1.53x across into an 888x666 viewport
  before Semu read it back. Every placement's rect was on the TV's opening (fit 686x515 at 297,187,
  bezel 592x444), so the off-centre look came from that crop, not the placement. RetroArch's
  mupen64plus_next now runs upstream's performance defaults, GLideN64 and the HLE RSP, held at native:
  EnableNativeResFactor 1 and 43screensize 320x240, in both slices, with no angrylion option. The N64
  ignores visual.render_resolution, so the frame is always the core's declared 320x240, and RetroArch's
  integer viewport is an exact 3x that Semu reads back pixel for pixel. In the VM (Xvfb, llvmpipe,
  dynarec) the renderer read native 320x240 in every placement: game 960x720 at 160,40 (screen-centred),
  bezel 640x480 at 320,202 (on the opening, 2x), fit 686x515 at 297,187. All three were judged by eye.
  Under Rosetta, 4 of 8 dynarec runs with the HLE RSP segfaulted at boot, angrylion+HLE among them. With
  cxd4 or the cached interpreter 0 of 3 did. This is logged as a VM artifact, since it is upstream's
  default x86 configuration. Bezel placement now steps in 240s, not 222s: 2x (480) at 1280x800 in the
  covered TV. Contracts: the core-options fixture (n64-core-options.cfg), no angrylion or cxd4 in either
  slice, the N64 at 320x240 whatever render_resolution says, and the TV at whole steps of 240, centred,
  at 1280x800 and 1920x1080. Each fails under its mutation.
- Item 9 (performance, build flags, d08ddbb): every flake compared with its upstream's own release
  build at the pinned source. The three that differed were fixed and built in the podman VM; the Deck
  measurement is open. Upstream release, then Semu's build:
  - PCSX2 2.6.3: Release, CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON, clang linked by lld, Multi-ISA
    (linux_build_qt.yml:135-148). Was clang 21 Multi-ISA with no LTO (0 ThinLTO symbols). Now IPO on, with
    LLVM bintools (lld, llvm-ar). Log: CXX_FLAGS "-O3 -DNDEBUG -std=gnu++20 -flto=thin -msse -msse2
    -msse4.1"; binary "Linker: LLD 21.1.8", 6722 ThinLTO symbols, -help runs. Multi-ISA picks the AVX2
    GS on the Deck at run time, so no fixed CPU level.
  - Cemu 2.6: CMake Release (-O3 -DNDEBUG) with IPO (CMakeLists.txt:73-75; build.yml:69 builds with
    clang-15, BUILD.md:44 recommends clang and documents GCC). The nixpkgs recipe set the Release flags
    to -DNDEBUG alone, leaving the fortify wrapper's -O2 (add-hardening.sh:95). Now CMake's own flags:
    "-O3 -DNDEBUG -flto=auto -fno-fat-lto-objects", 3234 LTO symbols. GCC 15 stays.
  - Azahar 2126.0 (standalone, and the libretro core that is the Deck's default 3DS): Release with LTO on
    by default (CMakeLists.txt:93-97, 157), SSE4.2, and a CPU-level switch (ENABLE_NATIVE_OPTIMIZATION,
    CMakeLists.txt:158, 252-266). It already matched ("LTO enabled", 10314/3923 LTO symbols). It now also
    compiles with -march=x86-64-v3 -mtune=znver2 on x86_64 Linux only: the Deck's level, held at v3 so
    FRACTAL-NORTH runs the same build. Log: "compiler CPU level: AVX2 1, tuned for znver2 1". AVX2
    instructions went from 72 to 29752 (standalone) and from 73 to 26868 (core). real-cores passes with it.
  - Matched already, unchanged: Dolphin 2606a (Flatpak Release, ENABLE_LTO off: Flatpak yml:50-55,
    CMakeLists.txt:87; 0 LTO symbols); melonDS (Release, ENABLE_LTO_RELEASE on whenever the IPO check
    passes, CMakeLists.txt:70-76; the binary is fully stripped, so this is not observed directly);
    Flycast 2.7 (upstream's Linux build is RelWithDebInfo -O2, c-cpp.yml:25; Semu builds Release -O3);
    PPSSPP 1.20.4 (log "Build type: Release"); Ryujinx 1.3.3 (dotnet publish -c Release, release.yml:58;
    log bin/Release/net9.0, runtimeconfig System.Runtime.TieredPGO true from Ryujinx.csproj:10; upstream
    ships no ReadyToRun); RetroArch 1.22.2 (Makefile:40-48 -O3); mupen64plus_next (libretro CI
    linux-x64 flags, .gitlab-ci.yml:93-100, identical to nixpkgs', -O3 -ffast-math, Makefile:653-660,
    x86_64 dynarec); Beetle PSX (HAVE_LIGHTREC, -O3); the other cores build with their own Makefile or
    CMake Release defaults, the same as libretro's buildbot.
  - The macOS N64 stays slow because upstream's osx build has no dynarec (WITH_DYNAREC is empty,
    Makefile:399-414 at f275caf). Semu matches upstream there. nixpkgs' default hardening (fortify3, stack
    protector, stack clash, zero-call-used-regs) is kept in every build. Cemu and Azahar keep GCC, though
    upstream's releases use clang.
  Contract build_flags.btrc fails under each mutation: no IPO, no ThinLTO check, -DNDEBUG restored, an
  explicit -O2, no CPU level, the level outside the x86_64-Linux guard, and a level on Dolphin. Every
  build also fails by itself if its flags never reach the compiler.
- Items 2 and 3 (picture): built and checked on the Mac render host at 1280x800 and 1920x1080, every
  system and bezel variant in bezel fit, judged by eye; the Deck look is open. GBC: a3d0dc6's hidden
  carbon plate never carried the buttons. Duimon draws Start and Select in GBC_Decal.png and no Duimon
  preset names it (res/layers/Nintendo_Game_Boy_Color/*.params at d03dabf), so the shell never had
  them. gbc-shell now declares the plate in `upstream.textures` (DecalImage, following the device:
  `upstream.parameters` HSM_DECAL_FOLLOW_LAYER 4), `semu bezel emit` stacks it at Mega Bezel's decal
  order 7, and gbc-berry inherits it. Bezel fit: the black bars were the Soqueroeu TV rooms, shrunk to
  the whole step, ending inside the screen (138 px top and bottom, 282 px each side at 1920x1080 on the
  PSX). Folding the room back put a second TV beside the first, so a scene's room (its viewport-
  following canvas layer, and the night plate over it) now carries its edge rows and columns outward:
  the flat wall up, the wall and the table's level grain to either side (layer line field `|1`,
  compositor `roomPlate`, mirrored in the editor). Its table never extends down, because the plate's
  last rows are the table's dark front edge: a room shorter than the screen in bezel fit stands on the
  screen's bottom edge, as fit already shows it. The TV stays centred across and at whole steps (the
  N64 contract now expects the table on the bottom edge). The gb studio render's opaque black backdrop
  read as bars on the desk, so its surround is now transparent (config/assets/bezels/gb/classic.png,
  digest updated). Contracts (scene_fill.btrc): both GBC shells draw the decal; the NES room and night
  lines carry `|1` and the renderer parses it; the layer pass draws roomPlate; and every system and
  variant in fit, game and bezel at both sizes is covered by its desk, its canvas or a standing room
  (194 pictures, 28 rooms). Each fails under its mutation: no anchor, no field, no parse, no roomPlate,
  no supplement (the emit fixed point) and no decal layer.
- Items 6 and 7 (picture, c2bd63a): built and checked on the Mac render host, the Dreamcast also with
  Flycast 2.7 in the VM; the Deck look is open. Flicker: both Dreamcast presets split every 480-line
  frame into two fields and drew one per frame. crt-easymode-halation does it from 400 lines
  (INTERLACING_TOGGLE, default 1, crt-easymode-halation.slang:143-155) and crt-royale from 289 to 576
  lines (interlace_detect_toggle, default 1, src/bind-shader-params.h:233 and scanline-functions.h
  is_interlaced), both at slang-shaders 4812a82. The Deck's evidence log shows the owner cycling sharp,
  royale and none on Flycast. The picture moved a field line every frame. Two consecutive render-host
  frames of a still Dreamcast screenshot differed in 34% of their pixels with either preset (GameCube
  35%, Wii 33%, PS2 35%). Every emulator hands Semu whole progressive frames, so all 15 CRT wrappers of
  those families now set the switch to 0: every system's sharp variant, the royale screens and the
  Genesis royale-fast. Consecutive frames are now identical (0 pixels) on the Dreamcast (both
  variants), GameCube, Wii and PS2. In the VM, Flycast 2.7 (Semu's emu.cfg, libsemupreload, Xvfb,
  llvmpipe) ran ChuChu Rocket!'s title screen in the Dreamcast TV with the sharp preset, and 8 captures
  were taken 0.37 s apart. With b86dada's presets, consecutive captures differed over the whole picture
  (17% to 44% of the window). With these presets, only the animated logo and the blinking PRESS START
  differ (1.6% to 5.3%). Reflections: the mirror read the shaded
  picture at the mip whose texel spans one source pixel. Mips step in powers of two, which match no
  integer scale except 2x and 4x, so at the Genesis's 3x it blended a 2-pixel and a 4-pixel mip: a
  coarse grid with the scanlines half averaged. Each mirrored point now averages a box at least one
  source pixel (and one screen pixel) wide, with five taps a fifth of the box apart. That covers whole
  periods of the shader's scanlines and mask wherever the point lands. The filtering is linear,
  including the raw frame, which magnifies point-sampled, and the box widens outward as before. Aero
  the Acro-Bat (its screenshot as the card, new `RENDER_HOST_CARD=/path.png`) was checked in fit,
  bezel and game with both Genesis presets, judged by eye at 6-8x. The mirrored score digits and green
  pipe are now sharp at the edge and smooth outward, with no blocks and no mask stripes; before, they
  were blocky columns. The editor compiles the same file, validated as GLSL ES 3.0 with glslang.
  Contracts (fields_reflections.btrc) check that every CRT variant of an interlacing family (15
  variants, including the 480-line Dreamcast, GameCube, Wii and PS2) emits its switch at 0, and that
  reflectAt keeps the one-source-pixel box, the fifths and the linear lod. Each fails under its
  mutation: the Dreamcast sharp switch removed, the PS2 royale switch at 1, the old reflectAt, a stride
  of half the box. For the CRT track (item 8): crt-guest-advanced's Interlace Mode (`interm`, on from
  375 lines) bobs the same way, and the contract already expects `interm` 0 on any guest wrapper.
- Item 8 (picture, CRT shader, dfaff71 and 515daf3): built and checked on the Mac render host; the Deck
  look and its frame time are open. The bands: the PS1 defaults to fit, at 2.14x on the Deck (686x515
  for 320x240). crt-royale's fixed-width scanlines beat against that fractional step, so a white field
  shows irregular dark rows on the render host at 1280x800. At 1080p (3x) they are gone. The owner's
  reference is Retro Crisis's "RC GDV-NTSC - PlayStation RGB 100", guest.r's crt-guest-advanced-ntsc
  with Retro Crisis's values. The presets live only in GitHub release zips (GPL-3.0). The values were read from
  ShaderGlass's import of them (mausimus/ShaderGlass, retro-crisis/720p Steam Deck), and every
  parameter exists in the pinned slang-shaders 4812a82. psx/default is now that wrapper with the 720p
  Steam Deck values. The Deck preset turns the mask off (shadowMask -1), and the 1080p and 4K presets
  differ from it only in the mask, so one preset serves every screen. intres 1 (the one line that
  separates the 240p and 480i presets) draws a 480-line frame as 240 scanlines and changes nothing at
  240. interm 0 keeps whole frames. GDV's beam widens with brightness, and the white is even, with
  only the preset's fine luma grain (addnoised 0.2: drop it if the owner finds it noisy). Crash Bandicoot is
  softer and brighter, with NTSC chroma bleed and no royale dot grid. The N64 (the RC N64 preset), SNES (SNES RGB
  100) and Genesis (Mega Drive RGB 100) follow, with the presets' hum bar and any shader overscan crop left out.
  The NES was tried with the RC NES Composite 100 and left on its NTSC composite: brighter, yellower and
  red-fringed, not more accurate. Each previous default stays one step away (psx/royale, n64/hyllian,
  snes/svideo, genesis/rainbow). The 480-line systems keep Sharp CRT, because at 1280x800 their 480
  lines hold 1x (bezel) or 1.67x (screen). Cost: render-host gained RENDER_HOST_GPU_TIME=1, a
  GL_TIME_ELAPSED query around the renderer. Renderer GPU time on the M1 Max, shader off / crt-royale /
  GDV: PS1 fit 1280x800 0.77/1.41/1.50 ms, game 3x 0.37/1.53/1.79 ms, 1920x1080 fit 0.48/1.62/1.90 ms,
  3840x2160 game 2.11/4.55/5.03 ms. GDV costs 1.1 to 1.4x the royale it replaces. The Deck was
  unreachable today (ssh timed out) and is read-only for this track. Its GPU has about 1/4.5 the
  bandwidth and 1/6.5 the FP32 of the M1 Max (88 vs 400 GB/s, 1.6 vs 10.4 TFLOPS). Scaled by those
  ratios, the GDV passes come to roughly 3 to 5 ms per frame in fit and 6 to 9 ms in 3x screen
  placement, against 3 to 4 ms and 5 to 7.5 ms for the royale that ran there. That is inside the
  16.7 ms frame for the software-rendered PS1, SNES and Genesis. Retro Crisis ships Steam Deck presets
  of this same 19-pass chain. Contract (crt_gdv.btrc): the four defaults with every value emitted and
  pinned, the previous defaults one step away, and no guest preset rolling a hum bar or cropping
  overscan. Each check fails under its mutation (12 run).
- Review round (2026-10-05, ec25cb8, e363bca, 9227e7e): six findings on the three tracks, all fixed and
  checked on the Mac render host at 1280x800 and 1920x1080; the Deck look is open.
  - Rooms (items 3, 8): b86dada's room test also caught the Duimon DS and 3DS device plates (canvas-sized,
    viewport-following), which smeared their edge rows over the wood desk at 1280x800. Only a scene's
    own room plate, the Mega Bezel BackgroundImage, carries on now; the DS desk is back, judged by eye.
  - Fit on a TV room (items 1, 6, 8): every TV system defaults to fit, which drew 240 lines at 2.14x and
    480 at 1.07x on the Deck, so scanlines beat into bands on any field that is not pure white (a grey
    card banded every ~7 lines under GDV, the Dreamcast every ~14 px). Fit on a room now snaps to the
    nearest whole multiple of the native lines whose opening stays on screen (2x for 240 lines and 1x
    for 480 at 1280x800, 3x for 240 at 1920x1080), the room carrying on and its table on the bottom
    edge. A 50% grey card in the PS1's fit is now grain only, the Dreamcast's even. The editor preview
    carries the same arithmetic. This supersedes the 2026-10-04 ruling that left the default fit
    fractional on TVs.
  - Bezel placement on a room lets 1% of the plate leave the screen (the owner's bezel rule of
    2026-07-07), so the NES TV draws at 2x and fills the Deck's height instead of 1x. A body rectangle
    per Soqueroeu TV would allow more (the NES at 3x on 1080p); not done. SemuComposedScale (the
    texture-pack scale for mods) still floors fit and bezel; it matters only for packs on a TV system.
  - GDV fields (item 6): the PS1 and SNES presets set ntsc_fields 0, so ntsc-pass1 flipped the chroma
    phase every frame. Merged (ntsc_fields 1, as the RC Mega Drive preset), consecutive frames of a still
    Alundra screenshot differ in 116 pixels instead of 4233, Donkey Kong Country 2768 instead of 8745
    (the rest is the preset's grain).
  - Restart Game (item 4): the action (settings radial, the Wii layer's centre, Ctrl+Shift+D, the menu
    row) restarts only when something waits, as its Steam doc says, so a stray centre click after a
    live layout switch does nothing. The players page's RESTART GAME keeps the ruling below. With
    nothing to apply the toast reads RESTART: NOTHING TO APPLY.
  Contracts: scene_fill.btrc (the DS and 3DS device lines carry no room field; every TV room's default
  placement is a whole multiple of its native lines at both sizes; the NES in bezel is 480 lines at
  1280x800; the editor's copy of the room flag, uLayer.w, scissor, tolerance, fit snap and bottom-edge
  anchor), fields_reflections.btrc (every GDV-NTSC variant merges its fields) and controller_layouts.btrc
  (the action after a live switch journals 87/0). Each fails under its mutation (10 run).

### M15. Owner feedback from the Deck, round 3 (2026-10-05)

The owner played the 15cc2ec release in Game Mode and reported (verbatim where quoted):

1. **Background** "i don't like the wood background, can we just make it black?" Done when every
   package that drew the wood desk (desk-night.png: the handheld shells and the dual-screen layouts)
   draws pure black around its art instead, the editor preview included. The Soqueroeu TV rooms are a
   scene, not a background, and keep their wall and table (reversible default; the owner can ask).
2. **Dual screens** "i don't like the weird rounded corner screens bezels for nds and 3ds ... 'Large
   main, second right', 'large main, second left', 'bezel side-by-side'. i have noticed there is weird
   behavior on some of the dual screen settings and 'Fit (Bezel/Screen)'. For example, on DS vertical
   shell, if i hit fit screen, the bezel disappears." Done when the drawn rounded frames are gone (plain
   square screens on black), and every DS and 3DS variant behaves consistently under Fit: a shell is
   never dropped by Fit, every state is at whole steps, checked by eye at 1280x800 and 1920x1080.
3. **N64** "doesn't have reflections on the borders". Done when the N64 TV mirrors the picture on its
   borders like the other TVs, on the Deck.
4. **Bezel fit scale** "are we confident fit bezel works? ... can't make the fit bezel on gameboy twice
   the current size on the steam deck, there seems to be a ton of vertical padding ... can you check for
   gbc too?" Done when the GB and GBC bezel-fit scale on the Deck is shown to be the largest whole step
   at which the shell fits (numbers), or fixed.
5. **GB pixel accuracy** "are we sure we're not drawing off by 1-2 pixels? ... the text start
   immediately at the bezel without any spacing". Done when a native test pattern shows every GB and GBC
   pixel drawn whole (k by k), none covered by the shell, the picture centred in the glass.
6. **Wii U sound** "sound doesn't work wiiu smash bros". Done when Wii U games play sound on the Deck.
7. **PS2** "screen isn't centered - it's very low on the table compared to ps1". Done when the PS2
   picture sits on its TV and table the way the PS1's does, on the Deck.
8. **Reflections** "the reflections on the edge of the screen don't seem to be working? or very
   inconsistently, please run an audit". Done when an audit of every system, bezel variant and placement
   shows the mirror on every bezel that should have one (render host and Deck), with each gap fixed.

Status: observed on the Deck 2026-10-05 (release bdabd01), off-screen, every item but Wii U sound by ear.
- Deck run (2026-10-05, 1280x800, `tests/deck/m15-check.cases`, 13 cases with the owner's games, each judged by
  eye from its shots, journal, receipts and evidence by one reviewer per case): 13 of 13 hold, every launch quit
  clean. GB (Aerostar) 5x at 240,40 with the game's own column 0 and row 0 mirrored on the 21 px LCD frame, Fit:
  Bezel the whole DMG at 1x centred on #000000 (164/165 px above and below); studio 2x whole (41/39 px), square
  LCD at 5x with whole corner pixels and no darker outer row; GBC 1x whole (161/162 px), berry carried over; GBA
  shell and arctic centred on black, the lip mirroring row by row. N64 (Super Mario 64) the picture mirrored on
  the lip with no black column; PS1, PS2 (Def Jam), Genesis (Aero), Dreamcast (Crazy Taxi) centred on their
  tables with the wood carried level, no streaks, no black; PSP mirrored on both variants; DS (Dual Strike) every
  layout plain and square on black, the shell kept in Fit: Screen, taps landing in every layout; 3DS (Ocarina of
  Time 3D) the touch screen's lip about 7 px wide; Wii U (Smash) playing on, the compiled settings.xml carrying
  api 3, TVDevice default, TVVolume 100. PCSX2's frame-60 capture shows no unsafe-settings or texture notice, but
  it did show "Controller SDL-0 connected." over the TV: OsdMessagesPos = 0 now drops PCSX2's OSD messages
  (Semu's toasts announce saves, loads and choices; reversible default). Open: Wii U sound heard in Game Mode.
- Item 1 (background): done on the Mac render host (db92462); seen on the Deck (above). The wood desk
  (desk-night.png: Duimon's dark_wood1 under the night plate) was the `background` of the gb, gbc,
  gba, psp, nds and n3ds shells and the four dual layouts, and the DS and 3DS shells also drew
  Duimon's Canvas_Background (shell) or Canvas_Vignette (vertical) layer over it. All 16 packages now
  name no background, those four backdrop layers are hidden (the editor's eye, which `semu bezel
  emit` keeps), and the compositor's first pass is black, so nothing is drawn behind the art. Seen on
  the render host at 1280x800 and 1920x1080 for every handheld and dual variant, in its default
  placement and switched live to Fit: Bezel and Fit: Screen (120 pictures): every pixel outside the
  art is #000000 (corner and edge samples; gb:studio went from 0 to 203,200 black pixels at
  1280x800), the shells and screens unchanged. The editor's capture matches the renderer to 0.000%
  of pixels, MAE 0, for 16 cells at both sizes (`tests/visual/editor-sync.sh`, headless Chrome).
  Reversible defaults:
  - The Soqueroeu TV rooms keep their wall and table: a scene, not a background.
  - Black also replaces Duimon's dark canvas and vignette behind the DS and 3DS shells, not only the
    wood, from the rule that outside the art is pure black where there is no scene.
  - The desk recipe leaves config/assets/bezels.json since nothing draws it (git history has it).
  - A background plate and a drawn frame stay package options that no shipped package names; the
    render_env contract proves the launch still passes them to the renderer.
- Item 2, the rounded frames (the Fit half is the placement track's): done on the render host
  (db92462); the Deck check is open. dual-main-left, dual-main-right, dual-side-by-side and
  dual-stacked name no frame (the rounded #121212 ring at 3% of the height is gone), so the screens
  stand square on black. With nothing drawn round them, the beside layouts reserve only the gap
  between the screens (visual.dual_screen_gap: 20 px on the Deck, 30 at 1080p), no longer one at
  each end too. Still the largest whole steps, centred as a pair or with the main centred alone.
  Re-derived (contract rows; the render host pictures match to the pixel):
  - NDS 1280x800: beside main 3x with the second 1x (1044 px pair, 118 px margins); side by side 2x
    and 2x; stacked 2x and 2x (6 px top and bottom).
  - NDS 1920x1080: beside 5x with 2x (49 px margins); side by side 3x; stacked 2x.
  - 3DS 1280x800: beside 2x with the touch screen 1x (70 px margins); side by side 1x; stacked 1x.
  - 3DS 1920x1080: beside 3x with the touch screen 2x (25 px margins; was 1x behind 30 px end
    margins); side by side 2x; stacked 2x.
  Touch and pointer maps are published from these lanes, so the touch contracts hold (8992 checks
  pass). Reversible default: no end margin beside the screens, which changes only the 3DS touch
  screen at 1080p.
- Item 2, Fit on the DS and 3DS (the placement half): done on the Mac render host (ceb3354); the Deck check is
  open. Audit of the release before it (render host, test card, shader off, every variant in bezel and screen
  placement at 1280x800 and 1920x1080, each screen measured from the picture):
  - nds shell: bezel 2.195x (562 px wide) at 1280x800 and 3.296x at 1080p; screen dropped the shell and
    stacked the screens at 2x, a different layout from the side-by-side shell.
  - nds vertical shell: bezel 1.48x (380x285) at 1280x800, 2x at 1080p only by chance; screen dropped the
    shell (the owner's report) and stacked the screens at 2x.
  - n3ds shell: bezel top 2.005x with the touch screen 0.943x at 1280x800, 3.005x with 1.415x at 1080p;
    screen dropped the shell and stacked them at 1x (2x at 1080p).
  - n3ds vertical shell: bezel 1.48x at 1280x800, 2x with the touch screen 1.996x (639x479) at 1080p; screen
    dropped the shell.
  - The four computed layouts were whole steps in both placements, so Fit changed nothing there, yet it
    toasted FIT: SCREEN or BEZEL and saved the choice.
  Causes, at 016377d: renderer_placement.btrc:15 took a whole step only for one screen (`surface_count == 1`),
  so a two-screen shell stayed contained (line 12) or, for the vertical shells whose device layer follows the
  viewport, covered (line 14); renderer_placement.btrc:89 with renderer_compositor.btrc:320 dropped the shell
  in screen placement (the M13 F3 ruling), :358-359 then stacked the screens, and :341 fitted each screen to
  its calibrated rectangle at whatever scale the canvas had. Duimon's 3DS face also draws the touch window at
  0.47 of the top screen's density (908x681 canvas px for 320x240 against 2404x1443 for 400x240), so no scale
  makes both whole inside their windows, and the 3DS vertical shell's touch rectangle is one canvas pixel short.
  Now every two-screen fixed bezel is placed by the new GL-free renderer_dual_shell.btrc (the compositor, the
  editor and the texture-pack scale use it). Fit never drops a shell, and every state is at whole steps, centred:
  - Bezel (and fit, which no DS offers): the largest step of the top screen at which the whole shell stays on
    the display, up to 1% past an edge, the shell centred. The whole shell is the package's declared
    silhouette (`shell`, item 4's field), else its canvas.
  - Screen: the largest step at which both screens, at the shell's own spacing, stay on the display, the
    shell drawn round them and cropped by the edges, the pair centred. When that step is no larger than the
    bezel's, it is exactly the bezel's picture, so the toggle never jumps.
  - The second screen takes the largest whole step its own rectangle holds (1% grace); below 1x, the
    nearest step. Its opening and lip are carried out with a picture that runs over, each side by its
    overrun (RendererLayout.carry since e379c10; the 1-px RendererLayout.hold here left the Deck's 3DS touch
    screen a 1-px mirror, see the review fixes below).
  - A Fit that moves nothing toasts FIT: SCREEN SAME AS BEZEL (`same_toast` in input.json). The renderer
    compares the geometry the placement produced with the same frame placed as Fit: Bezel
    (RendererPlacementNotice; it compared with the frame before until e379c10, see the review fixes below).
  - On the four computed layouts, which draw no art, Fit journals 81/-1 and toasts FIT: NO BEZEL. It saves
    nothing (SemuRenderVariantSet.placeable).
  Seen on the render host at 1280x800 and 1920x1080 (shader off and DS LCD; set directly and switched live
  with `render.sh 'nds:vertical>@game'`; the computed layouts with RENDER_HOST_SELECT=81,-1), judged by eye:
  - nds shell: 2x and 2x (shell 1165x656 centred), 3x and 3x at 1080p.
  - nds vertical shell: 1x and 1x (shell 960x540), 2x and 2x at 1080p (shell exactly 1920x1080).
  - n3ds shell: top 2x with the touch screen 1x, 320x240 over the 302x227 window (9 px past it each side,
    2 px past the opening, every pixel shown). At 1080p, 3x with 1x inside a 453x340 window, the glass gap
    mirroring the picture.
  - n3ds vertical shell: 1x and 1x, 2x and 2x at 1080p.
  - At both sizes the screen step equals the bezel's on all four shells, so Fit: Screen draws the same picture
    and says so. At 4K the DS shell's screen placement is 7x against 6x for bezel.
  - The computed layouts are unchanged (3x+1x, 2x+2x and so on, as listed above), and Fit leaves them as they
    are.
  The editor matches the renderer to MAE 0 for all four shells in bezel and screen placement at 1280x800,
  1920x1080 and 4K (`tests/visual/editor-sync.sh --card test`). Touch and pointer maps are published from the
  lanes, so they follow; `tests/deck/radial-check.cases` case 17 covers Fit on the large-main layout and on the
  vertical shell. `tests/visual/reflection-audit.sh` loses its none-no-shell verdict: the eight DS and 3DS
  screen-placement cells now mirror like the rest.
  Contract `tests/contracts/spec/dual_fit.btrc` covers both shells of both systems in all three placements at
  1280x800, 1080p, 4K and 4:3 1024x768: the shell drawn round both screens, whole steps, on screen, nothing
  masked, bezel the largest whole-shell step, screen the bezel's picture or a centred larger pair. It also pins
  the steps, the compositor's wiring and the lip, the SAME AS BEZEL toast, 81/-1 on the computed layouts and
  the texture-pack scale. Each check fails under its mutation: the compositor bypassing it, screen dropping the
  shell, a fractional or a spilling second screen, the shell's width unchecked, screen never larger, the lip not
  grown, no SAME toast, Fit stepping on a computed layout, and the old scale estimate.
  Reversible defaults:
  - Fit never removes a shell. The owner rejected the M13 F3 screen placement without it.
  - Fit on a DS or 3DS shell is the bezel placement, so every DS state is at whole steps.
  - Below 1x the second screen takes the nearest whole step. On the Deck the 3DS touch screen is 1x over its
    window's edge, never 0.94x. Under half of 1x (only the 3DS face on a 4:3 display) it fills its window
    instead, the fractional fallback, so it never spills off the shell.
  - The toast reads FIT: SCREEN for the two frames before the placement applies, then SAME AS BEZEL.
  - On the Deck both vertical shells are 1x (they were 1.48x). 2x needs 1080 lines for the whole shell and 896
    for both screens at the shell's spacing. The stacked layout gives 2x on the Deck.
- Item 6 (audio, Wii U sound): cause found in the pinned source and fixed (3ba382e), proven in the
  podman VM; the Deck check is open. Read-only on the Deck: the compiled settings.xml had no <Audio>
  block, and log.txt for Smash (0005000010144f00 v304, 01:33) stopped at "Cubeb: available" with no
  device line. Cemu a6fb0a4 then reads api 0 (DirectSound, Windows only) and an empty TV device
  (CemuConfig.cpp:271-290), and an empty device opens no TV stream at all (IAudioAPI.cpp:116-117), so
  every Wii U title was silent. WirePlumber's stream history on the Deck holds an output stream for
  every other emulator Semu launches (no Flatpak emulator is installed there, so those streams came
  from Semu's builds) and never one for Cemu ("Cemu Cubeb"). settings.xml now carries
  the whole block: api 3 (Cubeb), TVDevice default (Cubeb's default output, which on the Deck is
  PipeWire's pulse server), TVChannels 1 (stereo) and TVVolume 100, with the GamePad speaker and the
  mic off. In the VM, `tests/integration/audio.sh` runs a PulseAudio null sink found through
  $XDG_RUNTIME_DIR as on the Deck. Super Smash Bros. (US) (v304) through `semu launch` opened "Cemu
  Cubeb output" within 10 s (s16le, 2 ch, 48 kHz, uncorked, 100 %) and held it for all 210 s. The
  same argv and environment with the block taken out reached the same screen and opened no stream
  in 210 s, the Deck's symptom. MARIO KART 8 (US) opened the stream at 10 s, and the null sink's
  monitor measured the boot jingle (peak 0.15 of full scale), then the title screen's music from
  150 s to the end at 300 s (peaks 0.17 to 0.26). The capture shows "Press A to start". Audit of every emulator Semu launches on the Deck (backend and
  device; evidence: compiled config, log, WirePlumber stream):
  - RetroArch: pulse (audio_driver = "pulse"), default sink; stream RetroArch.
  - Cemu: Cubeb, default device; was none, fixed above.
  - PCSX2: Cubeb (no [SPU2/Output], upstream default), default device; emulog "Creating Cubeb audio
    stream ... driver = , device =" and stream PCSX2.
  - Dolphin: Cubeb (no [DSP]; AudioCommon.cpp:95-109 at 2606a), default; stream Dolphin Emulator.
  - Ryujinx: SDL2 (audio_backend SDL2, volume 1), default; stream Ryujinx.
  - Azahar: output_type 0 (auto, Cubeb), device Auto; log "Cubeb Audio Stream Started" and stream
    Azahar Output.
  - melonDS: SDL audio, Volume 256; stream melonDS.
  - Flycast: auto, which prefers SDL2 (audiostream.cpp:22-40 at v2.7); stream .flycast-wrapped.
  - PPSSPP: SDL, AutoAudioDevice, GameVolume 100; stream PPSSPPSDL.
  The release launcher's bubblewrap binds /run (pulse/native and pipewire-0 under /run/user/1000) and
  /home, and passes the environment through: no emulator sets PULSE_*, PIPEWIRE_*, SDL_AUDIODRIVER,
  ALSA_CONFIG_PATH or XDG_RUNTIME_DIR. A relocated XDG_CONFIG_HOME only gives libpulse a fresh cookie,
  which pipewire-pulse does not check. Contracts: `tests/contracts/spec/audio.btrc` (each emulator's
  path to the session's server, the defaults left alone, no audio variable exported, the launcher's
  binds, the headless harnesses) and the exact block in cemu.btrc. Each fails under its mutation:
  block removed, a Dolphin [DSP] section, PULSE_SERVER exported, --clearenv in the launcher,
  RetroArch on alsa. Reversible defaults:
  - The TV plays at 100 (unity, like every other emulator here). Cemu's own default is 50, or 20
    when the key is missing.
  - The GamePad speaker stays off as in Cemu, so a game that mirrors its mix to the pad is not heard
    twice; a pad-only sound is lost. No microphone.
  - Cemu has no macOS slice, so macOS has nothing to fix (RetroArch there is coreaudio, contracted).
  - With no sound server and no ALSA device, Cemu now stops at the game's audio init. It logs
    "can't initialize tv audio" and calls exit (ax_out.cpp:403-413), which is upstream's behaviour
    for a named device; seen in the VM. A null ALSA default keeps it running, also seen in the VM.
    So mods.sh, like live-switch.sh, menu-pause.sh and the Deck scripts, gives cubeb a null ALSA
    device. The Deck always runs PipeWire, and a Linux desktop almost always runs PipeWire or
    PulseAudio.
- Items 4 and 5 (GB and GBC bezel fit and pixels): fixed and measured on the Mac render host at
  1280x800 and 1920x1080 (2807952); the Deck check is open.
  - Item 4, Fit: Bezel. It measured the plate's whole canvas, transparent margin included (the DMG
    plate is 3440x6115 around a 2610x4252 device), so at 1920x1080 the DMG and GBC sat at 1x and the
    studio at 2x when the next step fits. Packages now carry `shell`, the alpha bounds of their
    visible plates measured from the pinned files with ImageMagick (DMG 2610x4252+415+936, GBC and
    berry 2624x4306+408+910, studio 766x1337+149+103, PSP 4999x2133+168+22), emitted as
    SEMU_RENDER_CANVAS_SHELL. Bezel takes the largest step whose shell fits with the 1% leeway and
    centres the shell. Shell sizes per step (with the soft drop shadow; body alone in brackets):
    - DMG: 1x 295x480, 2x 590x961 (569x940), 3x 885x1441. Deck: 1x, 160 px above and below, 492 at
      the sides. 2x would need 961 px of height (940 without the shadow, 1382 with the plate's
      margin); the Deck has 800, 808 with the leeway, so no reading of the rule allows it. 1080p: 2x
      (was 1x), 60 px above and below.
    - GBC (shell and berry): 1x 296x487, 2x 593x973 (573x952). Deck: 1x, 157 px above and below; 2x
      needs 973 (952). 1080p: 2x (was 1x), 54 px.
    - Studio: 2x 413x720, 3x 619x1080. Deck: 2x as before, 40 px; 3x needs 1080. 1080p: 3x (was
      2x), the height filled, 0.4 px over, inside the leeway.
    - PSP E1000 and red, the same defect: Deck 1x as before (2x needs 1818 px of width); 1080p 2x
      (was 1x).
    Seen by eye: whole shells, centred, on black. The editor's capture matches the renderer to 0.000%
    of pixels for gb dmg and studio, gbc shell and berry, and psp, in bezel and game at both sizes.
  - Item 5, pixels. `tests/visual/pixel-grid.sh` sends a 160x144 card through the renderer: a
    one-pixel border in four colours, a one-pixel checker and a marker in each corner. It compares
    the 160k x 144k rectangle with the card scaled k by k. DMG, GBC and berry were already exact: AE
    0, 5x at 800x720+240+40 on the Deck, 7x at 1120x1008+400+36 at 1080p, margins equal to the pixel.
    The studio was not. Its rounded LCD (radius 0.0336) cut every corner pixel, and its antialiased
    edge darkened the outermost device row of each edge pixel by 6% (198 for 211). Its LCD is now
    square like the real one, AE 0. Each device's picture corner now lands on a whole pixel in every
    integer placement, so the picture is exactly the step on any screen. Odd sizes such as 1281x801
    drew 801x721 before. LCD look on, by sampling: the DMG grid is symmetric in each k by k cell,
    the outermost row profiled like the interior. The GBC subpixel triad is locked to the native
    grid at 5x and 2x. (With a look on, the GBC and GBA looks, the DS and 3DS grid looks and the DMG at 7x darkened
    the edge pixels' outermost line: fixed in e40a944, see the review fixes below.)
  - "Text at the bezel": the game draws there. Aerostar, the owner's GB game (ES-DE log, 01:18),
    puts LICENSED BY NINTENDO in native column 0 (8 dark pixels in that column). Its SCORE line sits
    in row 0 (64 dark pixels). As in Duimon's art, the picture ends at the DMG's dark LCD frame, about 4
    native pixels wide (21-22 px at 5x on the Deck, 30 at 7x, 4 at 1x bezel, 9 at 2x), and the
    mirror is on it. The frame is centred on the picture within a canvas pixel (DMG 38/38 and
    39/39, GBC 45/45 and 46/46). The studio plate's lens is off centre: 60 canvas px left and 14
    right, beside its BATTERY label. Centring it would shrink the studio screen; left as drawn.
  Reversible defaults: the shell is alpha above 0 (shadow included, the strictest reading). The 1%
  leeway now applies to every bezel placement, not only rooms; no GBA step changes at either size.
  GBA keeps its canvas, because its LED layer's faint glow spans the plate. The PSP took the same
  fix. The studio LCD is square. Contract `tests/contracts/spec/gameboy_fit.btrc` covers the
  silhouettes, the steps and centring at both sizes, and whole pixels at ten sizes. It also covers
  no shape mask under an outermost pixel, the texture-pack scale and the editor's copy. Each check
  fails under its mutation (9 run): shell ignored, no leeway, not centred, no snap, studio rounded,
  berry's own shell, not emitted, composed scale floored, editor not centred.
- Item 3 (N64 reflections): cause found and fixed (cc323cf); seen on the render host and with the real
  core in the podman VM; the Deck check is open. Since M14, GLideN64 hands RetroArch a 320x240 frame with
  the VI pass region drawn black: 4 columns on the left and 3 on the right (vi_minhpass 8 and
  vi_maxhpass 7 half-dots, FrameBuffer.cpp:1249-1250 and 1557-1564 at mupen64plus-libretro-nx f275caf4),
  and the 3 lines under the 237 active lines of the standard NTSC modes. The lip mirrors the picture's
  outermost pixels, so it mirrored those black bands, 8 to 12 px at 2x and 3x, about half the lip. The
  fade left the game only on the lip's faint outer half. angrylion's Hide overscan had cropped the same
  border until M14. Measured with Super Mario 64 in the VM (real core, bezel and shader off, 3x): columns
  0-3 and 317-319 and lines 237-239 black. Its own 8-line border is the game's and stays. Ruled out: the
  tap's texture or a bound FBO (the picture and the lip draw from the same extracted frame), the
  strength (0.4 since M13), and the GDV-NTSC preset (the same black band with the shader off). Fix:
  `display.screens[0].crop` 4,0,3,3 in n64/system.json, emitted as SEMU_RENDER_SURFACE_0_CROP. The
  renderer takes the crop off every producer's reported surface before any geometry
  (renderer_surface_crop.btrc). Placement, the extracted picture and the mirror all see the 313x237
  picture, with square pixels and whole steps of 237: game 939x711 at 1280x800 and 1252x948 at 1080p,
  fit and bezel 626x474 at 1280x800. The editor previews the same picture. Seen:
  - Render host with SM64's real frame as the card (RENDER_HOST_CARD), the N64 TV in fit, bezel and
    game at both sizes. Before: a black column between the picture and a plain grey lip on both
    sides. After: the scene's green and blue, and a Bob-omb, mirrored across the edge onto the lip,
    sharp at the edge and fading outward.
  - VM, real core through the RetroArch GL tap (cc323cf): the renderer read native 313x237 and drew
    626x474 at 327,172 in fit and bezel. The left lip shades from #3C3D3E to #3E4250 toward the edge
    (the title's navy, mirrored) and meets the picture directly. The M14 capture there had 7 columns
    of #0B0E16 and a neutral #343433 lip.
  - Editor (`tests/visual/editor-sync.sh`, headless Chrome) against the renderer for n64:tv and
    n64:speakers in fit, bezel and game at 1280x800 and 1920x1080: framing equal, 0.000% of pixels
    differ, MAE 0.
  Reversible defaults: the crop is the standard NTSC VI's. A PAL game, or one with its own VI start,
  keeps a thin black edge of its own picture. SM64's own 8-line border still mirrors black at the top
  and bottom: it is the game.
- Item 8 (reflection audit): done on the render host; the Deck check is open. Every system (17) x
  bezel variant x placement (fit, bezel, game) at 1280x800 and 1920x1080 is 234 cells, plus switch and
  wiiu, which declare no bezel. `tests/visual/reflection-audit.sh` renders each cell twice, as declared
  and with every SEMU_RENDER_SCREEN_<n>_REFLECT zeroed, and the difference is the mirror. The full table
  is `tests/visual/reflection-audit.tsv`: declared strength, pixels changed, brightest step, live path
  and verdict. Compact (expected / render host / live path):
  - gb, gbc, gba (all variants): mirror / drawn in all 36 cells / RetroArch GL tap. Was 0.2-0.25,
    now 0.4; brightest step 29-46, now 54-93.
  - nes, snes, genesis, n64, psx (all variants): mirror / drawn in all 60 / RetroArch GL tap. nes was
    0.5, snes 0.3, psx 0.2; n64 drew only on black until item 3.
  - nds, n3ds shell and vertical shell: mirror in fit and bezel, drawn in all 16 / RetroArch tap
    (nds) and the Azahar Vulkan layer (n3ds). Since item 2's Fit fix (ceb3354) the shell stays in game (Fit:
    Screen) too, and those 8 cells mirror as well (re-measured: 24 of 24 drawn, but counted over both screens
    together: the Deck's 3DS touch screen mirrored 1 px until e379c10, see the review fixes below). The vertical
    shells were 0.2.
  - nds, n3ds main_right, main_left, side_by_side, stacked: none (computed layouts, nothing to
    mirror on) / none in all 48.
  - gc, wii (tv, tv_wide, speakers): mirror / drawn in all 30 / Dolphin GL preload. A Deck capture of
    2026-10-04 shows the white Wii safety screen mirrored bright on the TV lip. wii was 0.2.
  - dreamcast, ps2: mirror / drawn in all 24 / Flycast and PCSX2 GL preload. PCSX2's dark memory-card
    screen mirrors dark on the Deck, as it should.
  - psp (e1000, red): mirror / drawn in all 12 / PPSSPP GL preload. The Deck capture shows the
    picture tinting the lip.
  - switch, wiiu: none (no bezel) / none / Ryujinx and Cemu Vulkan layers.
  Verdict: no package that declares a mirror failed to draw it where its shell is shown (178 of 178; the
  re-run per screen at e379c10, below, gave 186 of 186, but its six wii tv_wide rows were the 4:3 TV's; re-run at
  9529a9c with the 16:9 TV drawn as a launch reaches it, through tv on a 16:9 picture: 186 of 186, see the audit
  tooling review fix below).
  The inconsistency was the strengths, which came from each preset's HSM_REFLECT_GLOBAL_AMOUNT: 0.2 to
  0.5. On the same card the brightest mirror step ranged from 29 (GB) and 40 (PS1) to 110 (NES). Every
  mirror is now one strength, 0.4, marked edited so `semu bezel emit` keeps it, and the range is 54 to
  93. The GB's green LCD is the darkest picture, so it stays the lowest. Live paths: the RetroArch
  bridge, the GL preload and the Vulkan layer all end in semu_render_game_gl, which crops and extracts
  the reported content rectangle. The mirror samples that frame's outermost pixels, so black inside the
  reported rectangle mirrors black. The N64 was the only such frame found. Contract:
  `tests/contracts/spec/reflection_audit.btrc` checks every cell (RendererMirror's lane strength, which
  the compositor sends to reflectAt, equals the package's wherever its shell is shown, and its ring band
  stands at least 1 px on screen on every side with screen beyond the picture), one strength for every
  mirror, the fragment shader's three mirrored bands per screen, and the compositor using RendererMirror.
  Each fails under its mutation: the lane strength sent as 0, the ring never shown, one chrome-pass
  reflectAt removed, psx back at 0.2, psx's ring outer pulled onto its picture. Reversible default: one
  strength, 0.4, for every bezel: the NES drops from 0.5, and gb, gbc, gba, snes, psx, wii and the
  vertical DS and 3DS shells rise to it.
- Item 7 (PS2 low on the table): cause found and fixed (38ed18a); seen on the render host and in the
  podman VM with the real PCSX2 and Beetle PSX at 1280x800 and 1920x1080; the Deck check is open.
  Read-only on the Deck: the owner's PS2 launch at 01:38 was Def Jam - Fight for NY (SLUS-21004, NTSC,
  emulog: OpenGL, integer scaling on) in fit (`current=0,0,2` in semu-render-variants.env), the PS1 at
  01:39 also in fit. No render evidence of that release is on the Deck (only switch receipts are
  logged), so the VM reproduced it: `tests/integration/live-switch.sh` (now with SEMU_BIOS, the PS1
  BIOS folder read-only, and SIZE) ran Def Jam with the owner's PS2 BIOS and CTR through `semu launch`
  under Xvfb on llvmpipe. PCSX2 is not the cause: with AspectRatio Auto 4:3, IntegerScaling and
  upscale 1 it draws its picture centred in its own fullscreen window (a Deck capture of 2026-10-04,
  Devil May Cry: 1024x768 at 128,16 in 1280x800, 16 px above and below), and the render hook
  publishes that rectangle with the GS texture's 1x size, 639x448 for Def Jam (448 NTSC lines,
  deinterlaced by PCSX2). The renderer placed it: fit on a TV room takes the nearest whole step whose
  opening stays on screen (renderer_placement.btrc:54-58 at 78ed857), 514.8/448 rounds to 1x where the PS1's
  514.8/240 rounds to 2x, so the PS2's TV was drawn at 87% of the covered room (1238x696) and the
  PS1's at 93% (1326x746), and both rooms then stood their table on the screen's bottom edge (the
  M14 anchor, :83). The smaller PS2 TV's picture sat at rows 199-647, its middle 23 px below the
  screen's; the PS1's 2x picture at rows 145-625, 15 px above. VM before (renderer debug lines): PS2
  out 341,153 597x448 in a canvas 21,0 1238x696; PS1 (256x240) out 320,175 640x480 in -23,0 1326x746.
  Fix: fit and bezel on a TV room centre the picture down the screen at the same whole step (game
  always did); fit's opening test follows the centred picture; the wall carries on up and the table
  under the TV, where the compositor draws the plate's last 32 rows averaged (a 32-texel mip
  footprint) instead of one row pulled into vertical streaks; every picture, rooms included, lands on
  whole pixels. VM after at 1280x800: PS2 out 341,176 597x448 (rows 176-624, middle 400), canvas
  21,23 1238x696, so 23 px of table front under the TV; PS1 out 320,160 640x480 (middle 400), and
  CTR's 640x472 logo screen out 326,164 629x472 (middle 400). VM after at 1920x1080: PS2 out 364,92
  1193x896, 2x, middle 540 (on the render host it was 59 px above the middle, its opening 6 px from
  the top); PS1 (256x240) out 480,180 960x720, 3x, middle 540. One 1080p PS1 run lost RetroArch to a
  SIGSEGV before the renderer's first frame; a rerun (the same renderer on 0abf372) played. The render
  host shows the same for every TV room (19 variants x fit and bezel x both sizes, judged by eye: no
  black, the band level). The editor's capture matches the renderer to 0.000% of pixels, MAE 0, for
  ps2, psx, dreamcast, n64 and nes in fit and bezel at both sizes. Sizes stay whole steps: at
  1280x800 the PS2 stays 1x (448 lines, its TV 93% the height of a 240-line PS1's TV; 2x needs 896
  rows); at 1920x1080 the PS2 is 2x (896) and the PS1 3x (720). Not changed, noted: PCSX2's own
  integer scaling widens 512 to 1024 by a whole step but takes the height to 4:3 (768 for 448 lines),
  so the renderer reads a 1.71x picture back down to native with a linear blit
  (renderer_compositor.btrc:808-813); PCSX2's notice "Texture filtering is not set to Bilinear (PS2)"
  is filter = 0 in the profile. Both are fixed in the review round ("Review, PCSX2's notice and
  picture" below). Contract `tests/contracts/spec/tv_room.btrc`: PCSX2's 639x448 and
  Beetle's 320x240 in fit at both sizes (steps 1/2 and 2/3, centred, whole pixels, the two middles
  within a pixel), fit's opening test (1.4x the covered PS2 room stays at 1280x800, 1.5x leaves),
  every TV room in fit, game and bezel at both sizes centred on whole pixels at whole steps (168
  pictures), the shader's band and the editor's copy. scene_fill.btrc and placement.btrc (the N64)
  now expect the centred picture. Each fails under its mutation (7 run): the renderer's anchor, its
  opening test and its snap, the shader band, the editor's anchor, opening test and snap.
  Reversible defaults:
  - A TV room's picture is centred down the screen in fit and in bezel. This supersedes the M14
    ruling that a room shorter than the screen stands its table on the bottom edge.
  - Under the table the room carries on as a level band of its last 32 rows. At 1920x1080 a
    480-line TV at 1x (Dreamcast, GameCube, Wii) shows about 140 px of it. (Superseded by 567077b: the band
    showed vertical streaks; the wood now carries on as mirrored rows falling into the dark, see the review fixes below.)
  - Rooms take the whole-pixel snap in every placement, so a picture may sit up to half a pixel off
    the exact middle (the N64's 711 lines at 1280x800).

- Review fixes, renderer (2026-10-05, the M15 review's findings on items 2, 3, 4 and 8): done on the Mac render
  host and in headless-Chrome editor-sync; the Deck checks below are open.
  - The 3DS touch screen's lip (item 8; the review's high finding). Since ceb3354 the default 3DS shell drew its touch
    screen at 1x (320x240) over a 302x227 picture window on the Deck, and RendererLayout.hold grew the lip's edges only
    a pixel past it, so the touch screen's mirror was 1 px while the top screen's was about 14 (render host, 1280x800:
    reach 1 px on every side, profile 17 then 0). Now the opening and both lip edges are carried out with a whole-step
    picture that runs past the package's picture rectangle, each side by its overrun, then held a pixel past it
    (RendererLayout.carry through RendererMirror.ring and RendererDualShell.opening, and the editor's Geometry.carry), so
    the band keeps its declared width round the picture drawn. Measured on the render host through each side's middle
    (the screen's own mirror, its REFLECT zeroed alone): n3ds:shell at 1280x800 touch screen 6-7 px (52 46 29 18 10 5),
    its declared lip 22 canvas px = 7.3 px; the top screen 13-15 px of a 46-47 canvas px = 15.5 px lip (Duimon draws the
    touch window at 0.47 of the top's density, so its lip is half as wide). The lip covers the plate's window frame
    round the 1x picture; by eye it mirrors the picture sharp at the edge and fading outward like the top screen. Same in
    fit, bezel and game. At 1920x1080 nothing changes (touch 1x inside its 453x340 window, the glass gap mirrored, reach
    55-71 px). Every other cell (every system, variant and placement at both sizes, 234 cells against 0f535bb's build) is
    unchanged to the pixel except n3ds:vertical at 1080p, whose touch rectangle is a canvas pixel short: its bottom lip
    moves out by that pixel (reach 9 to 10 px, like the top screen's 10; 7397 px differ by at most 15 of 255).
  - The audit measures the drawn lanes. tests/contracts/spec/reflection_audit.btrc takes each band from the lip edges
    RendererMirror.ring sends round the picture the compositor draws (RendererDualShell's lanes for a two-screen shell)
    and needs at least half the package's own lip width on every side with screen beyond it; the compositor must use
    RendererMirror.ring. Under the old 1-px hold it fails: "n3ds:shell ... screen1 band 1px of a 7px lip".
    tests/visual/reflection-audit.sh measures each screen of a two-screen cell on its own (only that screen's REFLECT
    zeroed) and reports its reach (its mirror's pixels over its picture's perimeter, lanes from the renderer's debug
    lines, which now print lane1): THIN when one screen's reach is under a quarter of its sibling's. A variant for
    widescreen output (wii tv_wide) was fed a 16:9 card with bezel_variant tv_wide and its _REFLECT_B zeroed in the
    mirror-off pass; but render-env does not honour tv_wide (it is not in the radial's bezel list), so the renderer drew
    the 4:3 tv-soqueroeu-wii round a letterboxed 16:9 picture (corrected in the audit tooling review fix below). A
    system with no bezel gets its row unrendered. The table states the revision it
    was measured at. Re-run at e379c10 (tests/visual/reflection-audit.tsv, 238 rows): 186 of 186 declared mirrors
    drawn on every screen, none thin. Reach (px): n3ds shell 12.5 top and 5.7 touch at 1280x800 (the old hold gives 12.5 and 1.0:
    THIN, exit 1), 18.5 and 62.7 at 1080p (the touch screen's glass gap); nds shell 14.0 and 14.3, 21.0 and 21.6;
    the vertical shells 4.7 to 4.9, 9.3 to 9.8. The six wii tv_wide rows (88440 to 230922 px) were the 4:3 TV's mirror
    round a letterboxed 16:9 picture, not the 16:9 TV's (re-measured below: 21464 to 88322 px). The TV rooms' rows moved with 38ed18a's centring (psx tv fit 1280x800
    19428 to 18918, ps2 tv fit 1080p 129223 to 129886, n64 tv fit 1080p 53922 to 52570; ps2 speakers fit 1080p
    39462 to 157074, the 2x it has drawn since 38ed18a).
  - SAME AS BEZEL (item 2's toast). It compared a Fit choice with the frame before, so a PS2 at 1080p, whose Fit and Fit:
    Screen are both 2x while Fit: Bezel is 1x, toasted FIT: SCREEN SAME AS BEZEL and FIT: FIT SAME AS BEZEL. Now
    semu_render_game_gl resolves the same frame as Fit: Bezel (RendererGeometry.resolve with placement 2) and compares
    with that. Fit itself says FIT: SAME AS BEZEL (`same_fit_toast`). Render host: ps2:tv and ps2:speakers at 1920x1080,
    Fit to Fit: Screen, toast FIT: SCREEN; ps2:tv at 1280x800, Fit: Screen to Fit, FIT: SAME AS BEZEL with the 1x TV;
    nds:vertical at 1280x800, Bezel to Screen, FIT: SCREEN SAME AS BEZEL as before. Contract dual_fit.btrc pins all
    three and that the frame path calls the notice after the crop and the drawn geometry.
  - Editor parity. tests/contracts/spec/editor_dual.btrc pins the editor's port of RendererDualShell (canvasOn and lanes
    through dualShell, the whole-shell and screen steps, the second screen's step, the carried opening and lip, and
    carry itself on both sides). tests/visual/editor-sync.sh now exits non-zero on any framing more than 1 px apart or any
    pixel over 10 %, and finds the widescreen variant by its "output" (wii:tv_wide no longer reports 9 px; but it named
    tv_wide to render-env, which draws the 4:3 TV for it, so its six wii:tv_wide cells failed on a state no launch
    reaches; corrected in the audit tooling review fix below). The render
    host's editor card (RENDER_HOST_CARD=editor) is drawn into the picture a declared crop keeps, black round it as
    GLideN64 hands it over, so with --card test the N64 matches the editor too. editor-sync --card test, bezel and game,
    1280x800 and 1920x1080, for nds:shell, nds:vertical, n3ds:shell, n3ds:vertical, gba (both), n64 (both): 0.000 %,
    MAE 0, framing equal, 0 failures. With the editor's canvasOn bypassed it exits 1 (FRAMING DIFFERS by 462 px).
    bezel_emit.btrc proves a recolour inherits its base's `shell` (gbc-berry, psp-red, gba-arctic).
  - GBA silhouette (item 4's review). gba-shell and gba-arctic declare `shell` 4216x2411+92+43: the alpha bounds of the
    device, decal, glass and top plates. The led plate is a black matte (alpha over black at most 1 % luma), invisible on
    the black round the art, so it is not part of the silhouette. Render host, Fit: Bezel: the device was 68 px from the
    top and 80 from the bottom at 1280x800, 43 and 59 at 1080p; now 74 and 74, 52 and 51. Steps unchanged (2x 480x320,
    3x 720x480: 4x needs 2277 px of width); fit and game unchanged to the pixel. gameboy_fit.btrc covers both.
  Each new check fails under its mutation (run, restored with cp, checked with cmp): the lip held instead of carried
  (reflection_audit), the opening held (dual_fit), the notice not called (dual_fit and placement), same() always true
  (dual_fit), Fit's label back to same (dual_fit), the editor's canvasOn bypassed (editor_dual), shell dropped from
  inherit (bezel_emit), gba-shell without its shell (gameboy_fit). The visual tools too: the old hold makes
  reflection-audit.sh flag n3ds:shell THIN and exit 1, the old render-host card makes editor-sync --card test n64
  differ by 2.668 % and exit 1.
  Reversible defaults:
  - A whole-step screen past its picture window carries its opening and lip out with it, covering the plate's window
    frame, rather than stepping down to a fractional picture that fits inside (15cc2ec's 0.94x).
  - Fit's own toast reads FIT: SAME AS BEZEL; Fit: Screen keeps FIT: SCREEN SAME AS BEZEL. The plain FIT: FIT toast is
    unchanged.
  - The GBA's silhouette leaves out its black led matte.
  Deck checks (open):
  - 3DS, the Duimon shell (default), any game, Fit: Bezel and Fit: Screen: the touch screen's dark lip shows the touch
    picture mirrored about 7 px out, sharp at the picture's edge and fading, on all four sides, like the top screen's.
  - PS2, any game, docked at 1920x1080: switching between Fit and Fit: Screen toasts FIT: FIT or FIT: SCREEN, never SAME
    AS BEZEL. On the Deck's screen, from Fit: Screen back to Fit: FIT: SAME AS BEZEL with the 1x TV.
  - GBA, any game, shell and arctic, Fit: Bezel: the device centred, about 74 px of black above and below at 2x.
- Review, PCSX2's notice and picture (the M15 review's finding on item 7): fixed; seen in the podman VM
  with the owner's PS2 BIOS, Def Jam - Fight for NY (USA) and Devil May Cry (USA) through `semu launch`
  (`tests/integration/live-switch.sh`, Xvfb, llvmpipe), A/B against 0f535bb; the Deck check is open
  (case 7 of `tests/deck/m15-check.cases`).
  Before: every boot PCSX2 logged and drew "Integer scaling is enabled. This may shrink the image." and
  "Texture filtering is not set to Bilinear (PS2). This will break rendering in some games." over the
  composed TV for 10 s (VMManager::WarnAboutUnsafeSettings at v2.6.3; the profile's filter = 0, Nearest,
  forced since a8fffb1 with no reason given; the forced OpenGL renderer adds a third line). The Deck's
  frame-60 capture of 2026-10-04 shows the first two; the VM's emulog at 0f535bb holds 5 Unsafe Settings
  lines. And the render hook published the rectangle PCSX2 had drawn the picture into: Def Jam's 639x448
  GS picture drawn as 640x480 (receipt surface0_source=320,160,640,480 for native 639x448; the Deck's
  Devil May Cry 1024x768 for 512x448), which the renderer read back down to native with a linear blit,
  so the picture was resampled twice before the CRT shader. Now:
  - The profile sets [EmuCore/GS] filter = 2, Bilinear (PS2), upstream's default (textures filter as
    the game asks the GS to), and [EmuCore] WarnAboutUnsafeSettings = false.
  - The hook (`semu_render_hook.patch`) draws the GS output once at its own size, point-sampled, one
    texel to one pixel, centred in the window, and publishes that rectangle as the picture. PCSX2's own
    rectangle, on whole pixels as before, gives only the shape (4:3, 3:2 progressive, or a widescreen
    patch's), so placement is unchanged. When Semu composes nothing the hook clears the window and draws
    PCSX2's own picture as before. A picture larger than the window (an internal resolution above
    native, which Semu never sets) is published where PCSX2 drew it.
  VM after, 1280x800: no Unsafe Settings line in the emulog (5 before) and no notice in the frame-15
  capture; the receipt reads surface0_source=320,176,639,448 for native 639x448, so the renderer's
  extraction is a 1:1 nearest copy; the TV room unchanged (out 341,176 597x448, tube 311,162 657x476,
  identical to 0f535bb) with the shader on and off, in Fit, Fit: Bezel and Fit: Screen, judged by eye
  on Def Jam's Autosave Warning screen. 1920x1080: out 364,92 1193x896 (2x) as before, the receipt's
  source 640,316,639,448 for native 639x448 (PCSX2 would have drawn 1278x958). Devil May Cry, the 512x448
  game the Deck's capture showed as 1024x768: source 384,176,512,448 against 128,16,1024,768 at 0f535bb,
  the same TV (out 341,176 597x448). On its memory card text, shader off, the new picture is visibly
  crisper and evener (zoomed side by side); its neighbouring rows differ by 7.79 on average against
  7.37, its neighbouring columns by 10.90 against 9.80 (0-255, the same static screen). Semu composed
  every frame of these runs, so the fallback to PCSX2's own picture was not exercised live. Contract
  `tests/contracts/spec/ps2_picture.btrc`: the compiled PCSX2.ini for the Deck and the desktop (no
  notice, Bilinear (PS2), native resolution) and
  the hook's native draw, publish, fallback and order. Each fails under its mutation (5 run: filter 0,
  the notice switch removed, the fallback's clear removed, the native draw stretched back to PCSX2's
  rectangle, the published shape replaced). `tests/integration/live-switch.sh` now takes CAPTURE_FRAME
  and keeps each launch's capture, receipts and PCSX2 emulog. Reversible defaults:
  - filter = 2, the PS2's own texture filtering. Nearest gave no benefit Semu needs: Semu's renderer
    scales and shades the output, not the textures.
  - WarnAboutUnsafeSettings = false hides every PCSX2 unsafe-settings notice. Semu writes PCSX2's
    settings, so a notice could only repeat Semu's own choices over the game.
  - IntegerScaling and AspectRatio stay: they shape PCSX2's own picture when Semu composes nothing,
    and they are where the hook reads the picture's shape.
- Review, the DS and 3DS live paths after items 1 and 2: checked in the podman VM (Xvfb, llvmpipe and
  lavapipe); taps land where the picture is drawn in every layout and Fit state at both sizes; the Deck
  check is open (cases 11 and 12). Nothing needed fixing.
  - RetroArch's DS and 3DS routes: `tests/integration/touch-x11.sh` (synthetic core as melonDS and
    Azahar, now with SIZE, CASES and a SHADER=none check of the drawn picture) ran the Duimon and
    vertical shells in fit, bezel and game and the four computed layouts, 18 sessions per size. At each
    receipt's rectangle the outermost ring of the drawn screen is the card's white border (254-255) and
    the ring outside it is not (0 on black, 117-124 on a lip), for both screens, so a tap on the receipt
    is a tap on the picture (a rectangle a pixel off fails it: 164 to 215 on the 3DS touch screen's
    capture). Taps at 5, 50 and 95% across the touch screen reach the core at 0.047-0.050,
    0.500 and 0.949 (DS) and 0.140, 0.500 and 0.860 (3DS, which reads 0.1 + 0.8 f), 0.75 down; a tap on
    the bezel presses nothing; Semu's arrow shows and hides. 1920x1080: 270 checks pass. 1280x800: 264
    pass; the first tap of two 3DS sessions (shell in bezel, stacked) never reached RetroArch (no bridge
    line) while the VM was also compiling PCSX2, the same rectangles passing in the shell's fit and
    game. Rerun at e379c10 with this change (the 3DS touch screen's lip carried out): the shell in bezel
    and game, stacked and the vertical shell at 1280x800 (60 checks) and the shell and vertical shell at
    1920x1080 (30) all pass, every first tap included, the rectangles unchanged; outside the Deck's 1x
    touch screen the ring now reads 122 (73 before e379c10), its lip mirroring the white border.
  - Rectangles (left,top from the top left): DS 1280x800: shell 98,180 and 669,180 at 512x384 (2x);
    vertical 512,175 and 512,431 at 256x192 (1x); main right 118,112 768x576 with 906,304 256x192, main
    left mirrored; side by side 118,208 and 650,208 (2x); stacked 384,6 and 384,410. 3DS 1280x800: shell
    50,198 800x480 with the touch screen 933,409 320x240; vertical 440,139 and 480,421 (1x); main right
    70,160 800x480 with 890,280 320x240; side by side 270,280 and 690,280; stacked 440,150 and 480,410.
    DS 1920x1080: shell 147,210 and 1004,210 at 768x576 (3x); vertical 704,91 and 704,603 (2x); main
    right 49,60 1280x960 with 1359,348 512x384; side by side 177,252 and 975,252 (3x); stacked 704,141
    and 704,555 (a 30 px gap). 3DS 1920x1080: shell 75,237 1200x720 with 1479,613 320x240; vertical
    560,19 800x480 and 640,583 640x480; main right 25,180 1200x720 with 1255,300 640x480; side by side
    225,300 800x480 and 1055,300 640x480; stacked 560,45 and 640,555. All as item 2 lists them.
  - Standalone Azahar through VK_LAYER_SEMU_compositor: `tests/visual/vm-azahar-layer.sh` (now with
    SETTINGS and TAPS), Pushmo (U), clicked at 5, 50 and 95% across the touch screen as drawn, half way
    down. The layer mapped every click into Azahar's own touch screen, which spans 480-800 by 400-640
    of its 1280x800 frame: 496,520, 640,520 and 784,520, exactly 5, 50 and 95% across and half way down.
    1280x800: the Duimon shell (bezel and game), the vertical shell, main right, main left, side by side
    and stacked. 1920x1080, where Azahar's touch screen is 640x480 at 640,540: 672,780, 960,780 and
    1248,780 for the shell (its 1x touch screen at 1479,613), main right, the vertical shell and stacked.
    Semu's arrow showed after a move and hid 5 s later in every run. Azahar's own X cursor showed at the
    arrow (maim with and without the X cursor differs by 276-472 px) in 3 of 12 completed runs (the
    shell in game at 1280x800, the shell and the vertical shell at 1920x1080), each started beside other
    llvmpipe jobs; the shell's rerun at 1920x1080 was blank. That check is M13's cursor rule, not
    geometry, and is left open for the Deck (radial-check case 4). A stacked run at 1080p lost Azahar at
    boot and passed on its rerun.
- Review fixes, shaders and rooms (2026-10-05, the M15 review's findings on items 5 and 7): done on the Mac render host
  (e40a944 and 567077b) and in headless-Chrome editor-sync; the Deck checks below are open.
  - The LCD looks' edges (item 5: "make sure the gameboy is pixel perfect around the bezel"; item 5 above measured the
    shader off). With a look on, the outermost output row and column of the edge pixels were darker than the same pixel
    one in. Causes, all in the pinned slang-shaders (4812a82): lcd-grid-v2 (the GBC and GBA default looks, the DS and 3DS
    grid looks) lights each output pixel from its 2x2 source neighbours through texelFetchOffset, and a fetch past the
    source returns black, which no wrap_mode reaches (the review tried clamp_to_edge: no change); the authentic GBC look
    samples the four nearest texels, and the AGB-001 look scales its 4x pattern to the screen with a linear filter, both
    through the default clamp_to_border (black); and the Game Boy looks draw their backlight paper across the picture
    with a linear filter the same way, which reaches the paper's border once the picture outgrows it (the DMG's 1024 px
    paper under its 1120x1008 picture at 7x on 1920x1080; at 5x on the Deck it stays inside). Fix (e40a944): shaders.json
    `patches`, which shaders.nix applies to the staged tree after writing the presets, so every preset keeps pinning the
    upstream files it names. Each is a Semu-owned diff in config/assets/shader-patches with its provenance, pinned before
    (the upstream file's sha256) and after: lcd-grid-v2's fetch_offset clamps the fetched texel onto the source
    (clamp_to_edge for a fetch); authentic_gbc.slangp and agb001.slangp gain wrap_mode1 = clamp_to_edge;
    gameboy.slangp and gameboy-pocket.slangp gain BACKGROUND_wrap_mode = clamp_to_edge. An edge pixel's outside neighbour
    is now itself, and every read inside the picture is unchanged. NOTICE.md lists the patched files.
    Measured with `tests/visual/pixel-grid.sh`, extended: every screen gets a card at its own native size (the second
    through the render host's new RENDER_HOST_CARD_1), its coloured border drawn four pixels deep, so each outermost pixel
    has an inward twin of the same colour with the same neighbours once the edge carries on. Shader off, each picture
    equals its card k by k (whole). Shader on, each side's outermost k-pixel strip is compared with its twin: darker
    counts the pixels whose light (Rec. 709 luma) is more than three levels below the twin's (the Game Boy papers and the
    authentic look's 1x sub-pixels vary by a level or two, or in hue, from place to place), outer is the outermost
    line's light over its twin line's, lowest side. PIXEL_GRID_BASELINE compares the interior with another bundle,
    PIXEL_GRID_ASSET_ROOT measures one. Every handheld look on its default bezel in game and bezel placement, the DS and
    3DS also beside a large main (main_right), both sizes. Before (0f535bb's bundle), darker / outer, and after (e40a944):
    - GB DMG: 5x and 1x clean already (0 / 0.9999). 7x at 1080p 2044 / 0.942 to 0 / 0.9999 (the paper); 2x clean.
    - GB pocket: clean at every step (0 / 1.000; its paper is 2048 px).
    - GBC default: 5x 2162 / 0.938, 1x 4 / 0.987; 7x 4011 / 0.914, 2x 16 / 0.982. All now 0 / 1.000.
    - GBC authentic: 5x 44 / 0.950, 1x 4 / 0.998; 7x 274 / 0.922, 2x 16 / 0.975. All now 0 / 1.000.
    - GBA default: 5x 2516 / 0.937, 2x 16 / 0.984; 6x 3675 / 0.925, 3x 32 / 0.973. All now 0 / 1.000.
    - GBA AGB-001: 5x 2201 / 0.877 to 0 / 0.977; 6x 4800 / 0.799 to 4 / 0.960; 2x and 3x clean.
    - DS default (lcd1x_nds) and 3DS default (lcd1x, sharp-bilinear): clean already in every cell (0 / 1.000).
    - DS grid: shell 2x+2x 392+392 / 0.881 and 0.839, main_right 3x+1x 1156+4 / 0.788 and 0.968; at 1080p shell 3x+3x
      1156+1160 / 0.788 and 0.713, main_right 5x+2x 2242+392 / 0.711 and 0.839. All now 0 / 1.000.
    - 3DS grid: shell and main_right 2x+1x 17+4 / 0.989 and 0.977; at 1080p shell 3x+1x 29+4 / 0.981 and 0.977,
      main_right 3x+2x 29+480 / 0.981 and 0.865. All now 0 / 1.000.
    Every picture is whole (0) with the shader off, both screens of every DS and 3DS cell included. The interior (all
    but the outermost ring of native pixels) equals 0f535bb's bundle in every shaded cell (AE 0): the changed pixels are
    the ring's (3631 of the GBC's 800x720 at 5x). The lcd-grid looks (GBC and GBA default, DS and 3DS grid) and the
    authentic look now draw each edge strip as its twin, exactly (AE 0) but for the GBA default at 5x (143 pixels a
    level apart: float rounding) and the authentic look's 1x, whose own sub-pixel pattern varies in hue at 1x (2 pixels,
    luma within 2 levels). The DMG and pocket papers vary a level
    or two from place to place (edge and twin differ in 372 to 18767 pixels, none darker). The AGB-001 look keeps a
    blur residual: at 5x and 6x its last pass mixes each pixel's outer line with a sixth or a tenth of the neighbour's
    sub-pixel, and the picture's outermost line, with no neighbour past it, shows its own sub-pixel (outer 0.977 at 5x and
    0.960 at 6x, was 0.877 and 0.799; luma never more than 4.4 levels below the twin, 4 pixels at 6x). pixel-grid.sh
    reports that look rather than failing it (PIXEL_GRID_RESAMPLED). By eye (GBC default at 5x, 10x zoom on the top-left
    corner): the leftmost column, dark grey-green before, now shows the red sub-pixel stripe like every column in.
    Contract tests/contracts/spec/shader_edges.btrc: each patch turns its pinned upstream file (the bezel tree) into its
    pinned output (a unified-diff applier in BTRC), shaders.nix applies every patch and checks what it leaves, and no
    handheld look's pass fetches past its edge, samples past it through a border-clamped pass, or draws a linearly
    filtered texture over the picture without clamping it. Each fails under its mutation (run in a clone, restored with
    cp, checked with cmp): the lcd-grid patch dropped, its fetch unclamped, the authentic and AGB-001 wrap lines removed,
    the DMG paper's wrap removed, the pocket's patch dropped, shaders.nix never patching.
    Reversible defaults:
    - The patches touch only reads past the picture; a look is otherwise upstream's, pixel for pixel.
    - Edge pixels carry on as themselves (clamp to edge), never as black and never as the opposite edge (repeat).
    - The pocket's paper is clamped too, though it changes nothing below 13x.
    - The AGB-001 look keeps its linear scaling and so its 5x and 6x outer-line blur; a nearest final pass would change
      its interior.
  - The table under a centred TV (item 7). 38ed18a drew the room below its plate as the plate's last 32 rows averaged (a
    32-texel mip footprint at the clamped edge), so every column was one value all the way down: coarse vertical streaks
    (render host, PS2 in Fit at 1280x800: rows changed 0 levels row to row against 0.024 column to column). With the
    picture centred the band is on screen under the PS2 on the Deck (23 px in fit and bezel) and at 1920x1080 under the
    480-line TVs in fit (Dreamcast, GameCube and Wii, 102 to 135 px) and under most TVs in bezel (76 to 163 px). The
    plate's last rows are the wood below the table's front, out of the shadow under it (row light 7 to 20 of 255 over
    the last 20 of 1440 rows). Now (567077b, config/render/compositor.frag roomPlate) the room below the plate mirrors
    its last rows back and forth with the depth, a window of 0.55% of the plate's height (8 of 1440 rows, 12 of 2160,
    inside the wood and clear of the shadow), at the plate's own scale, so every row carries the plate's grain; and a
    normal-blend room layer falls off with the depth to 40% of that light over a tenth of the plate's height, as the
    TV's light falls off, never to black (the night plate, a multiply room layer, carries on as drawn). To either side
    the room now carries on from four texels in, past the dark two-texel bevel Soqueroeu draws at each end of the
    table's front, which carried out as a darker slab (17 against 23 of 255 at the left end) and showed as a step where
    the canvas ends; it now meets the table level. Render host: PS2 Fit at 1280x800, row to row 0.82 levels against
    0.26 column to column (level grain, no streaks); Dreamcast Fit at 1920x1080 0.63 against 0.21. Judged by eye at
    1280x800 and 1920x1080, all 19 TV rooms (ps2, psx, n64, snes, genesis, nes, dreamcast, gc and wii with their
    speakers, purple, black and 16:9 variants) in fit, bezel and game: the wood under the table reads as one surface
    going down into the dark, no streaks, no black bar, the side carry level with the table; the TV centred and at the
    same whole steps (the pictures are unchanged; ps2 tv and psx speakers at 1280x800 and dreamcast tv at 1080p are
    identical to the pixel before and after the rebase onto b39f57b). The editor draws the same file:
    editor-sync --card test, all 19 rooms in fit, bezel and game at 1280x800 and 1920x1080, at e40a944: 108 of 114
    cells 0.000% of pixels, MAE 0, framing equal (the band and the side carry included). The six wii:tv_wide cells
    fail on framing (87 to 866 px) with the same numbers at b39f57b without this change: since b39f57b editor-sync feeds
    production the 16:9 card under bezel_variant tv_wide, which render-env does not honour, so production drew the 4:3
    TV round a 16:9 picture, a state no launch reaches; the editor was right (corrected in the audit tooling review fix
    below, where all six match). Before the
    rebase (0f535bb with this change) the same run gave 16 of 19 per state, the N64 (crop card, fixed in b39f57b) and
    wii:tv_wide differing exactly as without the change.
    Contracts: tv_room.btrc's band (rows that follow the depth at the plate's gradients, never a fixed row or a
    stretched footprint; the 40% floor; the inset) and scene_fill.btrc's room check, each failing under its mutation
    (the old band, a fixed row, a fall to black, no inset).
    Reversible defaults:
    - The window is 0.55% of the plate's height and the fall-off 40% over a tenth of it.
    - The side carry starts four texels in, so the art's own outermost columns (the bevel) are replaced by the fourth.
    - Only a normal-blend room layer darkens; the night plate carries on unchanged.
  Deck checks (open):
  - GBC, any game (Aliens - Thanatos Encounter), default look (GBC color LCD), Fit: Screen 5x: the leftmost column and the
    top row of the picture show the same sub-pixel stripes and light as the column and row one pixel in, no dark line
    where the picture meets the lens; the same with the Authentic GBC LCD look.
  - GBA, Advance Wars, default look and AGB-001 LCD, Fit: Screen (5x, the picture filling the height): the top and
    bottom rows and both side columns lit like the next row and column in.
  - DS, LCD grid v2 look (both screens) and 3DS, LCD grid v2 look: no darker outer line round either screen (on the Deck
    it was up to 21% darker, the DS beside a large main; 29% at 1080p).
  - Docked at 1920x1080: Game Boy, DMG look, Fit: Screen (7x): the right edge column and the top row as light as the
    rest.
  - PS2, Def Jam - Fight for NY (any PS2 game), Living room CRT, Fit and Fit: Bezel: the 23 px under the table's front
    show dark wood with level grain, a little darker toward the screen's bottom edge, no vertical stripes and no black;
    at the canvas's left and right ends the table's front runs on without a step.
  - Docked at 1920x1080: Dreamcast, GameCube or Wii in Fit, and any TV in Fit: Bezel: the wood under the table runs on
    down into the dark (76 to 163 px), no stripes, no black bar.
- Review fixes, audit tooling (2026-10-05, the verification of the review fixes: the Wii's 16:9 TV in the audits and
  the editor's carry contract): done on the Mac render host (3ab17b3, c8a326b). No renderer or editor code changed.
  - The Wii's 16:9 TV in editor-sync and the reflection audit. Since b39f57b both scripts asked `semu render-env` for
    bezel_variant tv_wide. tv_wide declares "output": "widescreen" and is not in the radial's bezel list
    (tv|speakers|none), so render-env compiles the default tv-soqueroeu-wii with no _B keys, and fed the 16:9 card the
    renderer drew the 4:3 TV round a letterboxed picture (debug line "bezel tv-soqueroeu-wii", out 297,207 687x386 in
    Fit: Bezel at 1280x800, a fractional picture). No launch reaches that state: the owner's path is Living room CRT
    with the 16:9 output, which draws tv-soqueroeu-wii-16x9 through the _B keys. Both scripts now render a variant that
    declares an output through the variant listing it under "outputs" (tv) on that output's picture (a 16:9 card); the
    editor still previews wii:tv_wide. Every cell compares the package on the renderer's debug line with the variant's
    own and fails on a difference (editor-sync PACKAGE DIFFERS, the audit PACKAGE). Render host at 9529a9c:
    editor-sync --card test over every system and variant (39) in fit, bezel and game at 1280x800 and 1920x1080, 234
    cells: 0 failures, every cell 0.000 % and MAE 0, framing equal on the 31 fixed packages per state (the 8 computed
    DS and 3DS layouts are compared over the whole screen); wii:tv_wide drawn as
    tv-soqueroeu-wii-16x9 at 1335x751 (fit and bezel), 2002x1126 (game, 1280x800) and 2669x1502 (game, 1080p). By eye:
    the 16:9 TV round a 16:9 picture filling its tube, the lip mirroring the picture sharp at the edge and fading out.
    tests/visual/reflection-audit.tsv re-measured whole at 9529a9c, now with the drawn package as its third column: 238
    rows, 186 of 186 declared mirrors drawn on every screen, 0 failing; tv_wide 21464 px (fit and bezel, both sizes,
    reach 8.1 px), 37771 (game at 1280x800, 9.4), 88322 (game at 1080p, 16.6). 22 GB, GBC and GBA rows moved with
    e40a944's LCD edge patches (1 to 38 px, peak at most 1 step; gba shell fit 1280x800 10259 to 10285), verdicts
    unchanged. Contract audit_packages.btrc: both scripts resolve the base variant and guard the drawn package, the
    compositor's debug line still names the package, and every bezel row of the committed table names its variant's
    own package with every declared variant in all six states. It fails under six mutations (the 4:3 package in the
    tv_wide rows, the tv_wide rows dropped, either script's base selection or guard removed); at run time the old path
    fails the guards (editor-sync exits 1, "PACKAGE DIFFERS: production drew tv-soqueroeu-wii through tv_wide"; the
    audit exits 1 with PACKAGE and the old 135057 px).
  - The editor's carry contract. editor_dual.btrc pinned only the left and top lines of the editor's Geometry.carry:
    dropping its bottom or right line, or making Geometry.picture always the opening, moved the editor's 3DS touch lip
    off the Deck's (editor-sync n3ds:shell at 1280x800, 0.137 %) with make test green. It now pins every side's overrun
    and the hold in RendererLayout.carry and Geometry.carry, the picture rectangle (calibrated image, else the opening)
    in RendererMirror.picture and Geometry.picture, and laneUniforms carrying from Geometry.picture. It fails under nine
    mutations: the JS bottom, right, top and left sides, the hold, the picture always the opening and the uniforms'
    picture; the renderer's bottom side and its picture always the opening.
  Reversible defaults:
  - A variant that declares an output is audited the way a launch reaches it (the variant listing it under "outputs",
    on that output's picture), never by naming it to render-env; its rows keep its own id.
  - A cell whose renderer draws another package than its variant's fails rather than being measured.
  - render-env is unchanged: it still compiles the default variant for a bezel_variant outside the radial's list (no
    launch names one; the radial cycles tv|speakers|none and the output switch selects the _B keys).
  Deck check (open): Wii, any game, Living room CRT with the 16:9 output: the 16:9 TV with the picture filling its
  widescreen tube (853x480 in Fit on the Deck), its lip mirroring the picture about 8 px wide, never the 4:3 TV round a
  letterboxed picture.
- M15 Deck acceptance (seen off-screen on the Deck 2026-10-05, bdabd01: 13 of 13 cases hold; Wii U sound by ear open). Per item: system, the owner's
  game, variant, placement, what to see; numbers at 1280x800. `tests/deck/m15-check.cases` runs them
  off-screen (`tests/deck/input-check.sh PAD tests/deck/m15-check.cases OUT`; its case comments give the
  same observables as journal codes, receipts and shots). Sound and anything marked by eye need Game Mode.
  The review-fix entries above give their own Deck checks in more detail; this is the summary per item.
  Contract `tests/contracts/spec/m15_deck.btrc`: the case file plans under `input-check.sh --plan`, runs
  the owner's 13 games across the systems M15 changed, types Fit and Next Bezel with input.json's chords,
  and this list and the Status block's M14 and M15 lines exist. Each fails under its mutation (a case
  removed, a malformed token, the Fit chord changed, an item's line removed, a Status line renamed, the
  list renamed).
  - Item 1, black background (cases 2-4 and 10-12): GB Tetris (World) (Rev 1) DMG and Studio Gray, GBC Super
    Mario Bros. Deluxe (USA, Europe) (Rev 2) shell and Berry, GBA Advance Wars (USA) (Rev 1) shell and
    Arctic, PSP Ape Escape - On the Loose (USA) E1000 and Deep Red, each in Fit: Bezel and Fit: Screen; DS
    Advance Wars - Dual Strike (USA, Australia) and 3DS Ocarina of Time 3D, Duimon shell and vertical shell.
    Pure black wherever the art does not cover the screen: no wood desk, no dark canvas or vignette, the
    studio plate's old wood bands black. A TV (Super Mario 64 (USA)) keeps its living-room wall and table.
  - Item 2, DS and 3DS (cases 11 and 12, radial-check case 17): Dual Strike, Duimon shell in Fit: both
    screens 512x384 (2x) side by side, centred. Large main, second right: top 768x576 (3x), touch 256x192
    (1x) to its right, square, no rounded frames, about 118 px either side; second left mirrored; Side by
    side 2x and 2x with a 20 px gap; Stacked 2x and 2x, 6 px of black above and below. Fit on those four:
    FIT: NO BEZEL, nothing moves or is saved. DS vertical shell: 256x192 and 256x192 (1x) in the whole
    shell (about 960x540) on black; Fit: FIT: SCREEN SAME AS BEZEL, the shell stays. A tap on the touch
    screen's middle lands at melonDS 128,96 in every layout. Ocarina of Time 3D (standalone Azahar): Duimon
    shell top 800x480 (2x), touch 320x240 (1x) filling its window, nothing cut, a middle tap logs
    "semu-vulkan: touch ... -> 1" at the middle of Azahar's own touch screen (160,120 on the Deck in
    radial-check case 4); Large main right 2x and 1x, about 70 px either side; Side by side and Stacked
    1x and 1x; 3DS vertical shell 400x240 over 320x240 in the whole shell; Fit on a shell SAME AS BEZEL.
    Docked at 1920x1080, a PS2 game switched between Fit and Fit: Screen toasts FIT: FIT or FIT: SCREEN,
    never SAME AS BEZEL.
  - Item 3, N64 (case 5): Super Mario 64 (USA), Living room CRT, Fit: 626x474 centred; the left and right
    lips carry the game's colours mirrored, sharp at the picture, fading outward, no black column; the
    receipt reads surface0_native=313x237. Fit: Screen 939x711 at about 170,44, Fit: Bezel 626x474, and CRT
    with speakers: the same mirror.
  - Items 4 and 5, GB and GBC (cases 1-3): Aerostar (USA, Europe) DMG, Fit: Screen: 800x720 (5x), 40 px
    above and below, the L of LICENSED BY NINTENDO on the picture's left edge (the game's column 0), the
    dark LCD frame (about 21 px) carrying the mirror; Fit: Bezel: the whole DMG at 1x, about 160 px of
    black above and below. Studio Gray: Fit: Screen square LCD corners, every corner pixel whole; Fit:
    Bezel 2x, about 40 px. GBC shell and Berry: Fit: Bezel 1x, about 157 px; Fit: Screen 5x. GBA shell and
    Arctic: Fit: Bezel 2x, the device centred, about 74 px above and below. With each default LCD look on,
    the picture's edge pixels as bright as the ones inside (e40a944). Docked at 1920x1080, Fit: Bezel: DMG
    and GBC 2x, Studio 3x, PSP 2x.
  - Item 6, Wii U sound (case 13 shows only that Cemu plays on, silenced): Super Smash Bros. (US) (v304)
    from ES-DE in Game Mode: the confirm sound at the album-data dialog, music on the opening and menus,
    the volume buttons change it; MARIO KART 8 (US): the boot jingle and the title music, following
    headphones. Then, read-only: ~/.local/share/semu/cemu/config/Cemu/settings.xml has <api>3</api>,
    <TVDevice>default</TVDevice> and <TVVolume>100</TVVolume>; ~/.local/state/wireplumber/stream-properties
    gains "Cemu Cubeb". A PS2 and a GameCube game still have sound.
  - Item 7, PS2 (cases 6-9): Def Jam - Fight for NY (USA), Living room CRT, Fit: 597x448 (1x), rows 176-624,
    the TV body about 81 px below the top, the table's wood carried about 23 px under it with its grain,
    darkening downward (567077b); Crash Bandicoot (USA) on the PS1: 640x480 (2x), rows 160-640, at the same
    height. Fit: Bezel and CRT with speakers: still centred, no black. Genesis Aero the Acro-Bat (USA) and
    Dreamcast Crazy Taxi (USA): centred, the table carried under the TV, never streaks or black.
  - Item 8, reflections (cases 4, 5, 6, 11, 12): one mirror strength on every bezel. The PS1 lip as clear as
    the Genesis and N64 TVs'; a bright Wii screen lights the lip (Living room CRT and the 16:9 TV) as
    strongly as the GameCube's; the NES slightly softer than in 15cc2ec; the GB, GBC and GBA lips show the
    LCD mirrored (green on the DMG), never a smeared glow; the vertical DS and 3DS shells' lips as strong
    as the horizontal shells'; the 3DS touch screen's lip mirror about 7 px wide (the package's lip,
    e379c10), never a 1-px line.
  - PCSX2's picture (case 7): Def Jam at boot shows no PCSX2 notice in the top left, the launch's frame-60
    capture (~/.local/share/semu/pcsx2/semu-render-final.ppm) included; the receipt's surface0_source is
    the size of surface0_native (639x448), so PCSX2 hands over its GS picture one texel to one pixel; the
    pixels are even, with and without the CRT shader. By eye: Devil May Cry (USA)'s memory-card text
    (radial-check case 11) crisp and even, no smeared rows.

### M16. Owner feedback from the Deck, round 4 (2026-10-06)

The owner played the M15 release (3682cb6) in Game Mode and reported (verbatim where quoted):

1. **Pointer** "the mouse pointers in the systems with it should move to a crosshairs". Done when every
   pointer Semu draws (DS, 3DS) is a crosshair, with no second cursor beside it; the Wii keeps the game's own
   pointer (restated in the review round: Semu never drew one there, see the ruling under items 1 and 6).
2. **Rendering scale** "should be available through the radial / settings". Done when each system whose
   emulator can render above native offers a render-scale choice in the settings radial and the menu,
   saved per system, applied live or through Restart Game, with the picture still placed correctly.
3. **GB studio** "the gb studio gray bezel is bad. it has a weird texture to it and the proportions and
   everything are off...delete it". Done when the variant and its package are gone (the owner asked for
   the deletion).
4. **Boot resize** "why do some emulators go from correctly-sized (full screen), to 1/3 screen (oriented at
   top-left with bottom right, bottom, and right as black bars) to properly oriented again while loading a
   game? e.g. gamecube". Done when the cause is known and no emulator shows a wrongly sized frame at boot.
5. **Fit** "should cycle between integer-game, integer-bezel, non-integer-game, non-integer-bezel". Done
   when Fit cycles those four states on every system with a bezel (reversible: integer stays the default).
6. **Wii controller** "why does changing the wii controller require a restart?" Done when the reason is
   answered from Dolphin's source and every change Dolphin can make live is live.
7. **Wii sizes** "with wii, why is bezel + fit screen larger than no bezel?" Done when the cause is known and
   bezel-off follows the same Fit state as bezel-on.
8. **Dreamcast speed** "super laggy at like 23fps on sonic adventure". Done when Sonic Adventure runs at
   full speed on the Deck (measured).
9. **Shaders per system** "are we using the right shaders for each system? for example, dreamcast shaders vs
   sega genesis? both are on crts but one is easy and royal; the other is gdv-ntsc." Done when every
   system's default shader follows one stated rule (era, signal, display) and runs at full speed on the Deck.
10. **TV cutout** "there is a light bar and aliasing on the playstation bezel + screen cutout ...in fact i
    think there is a lot of aliasing with the tv curve on a few consoles". Done when every TV's curved
    opening is antialiased and the PS1's light bar is gone, checked by eye at 1280x800 and 1920x1080.

Status: started 2026-10-06.
- Items 1 and 6 (controls track): built and contract-proven on the Mac, observed on the Mac render host and in the
  podman VM (RetroArch DS and 3DS, standalone Azahar, Dolphin with Mario Kart Wii); the Deck checks are cases C1-C3 of
  `tests/deck/m16-check.cases` and the updated radial-check cases 3, 4 and 16.
  - Item 1, the pointer. Why it was an arrow: M13 drew a 12x19 arrow because the trackpad mouse needed something to aim
    with; nobody had asked for a shape. Now `renderer_cursor.btrc` draws a 15x15 crosshair: four white arms one cell
    wide and four long, each with a one-cell black outline, round a clear 3x3 centre, at one whole step per 400 rows
    (30x30 at 1280x800 and 1920x1080, 75x75 at 2160), its centre cell's middle pixel on the pointer
    (`RendererCursor.hotspot`), so a tap lands at the cross. The show and hide rules are M13's (motion or a held press
    shows it, 3 s idle hides it). Every route that draws Semu's pointer draws this one: RetroArch's DS and 3DS cores
    through the bridge and standalone Azahar through the Vulkan layer. Observed on the Mac render host (nds and n3ds
    at 1280x800 and 1920x1080, RENDER_HOST_CURSOR, by eye at 8x): the cross reads on the dark shell, on a mid colour
    and on the black and white checker; `tests/deck/cursor-crosshair.sh` (was cursor-arrow.sh) finds 64/64 white and
    224/224 black pixels centred on the pointer, and 24/64 two pixels off. Rulings, reversible:
    - The Wii keeps one pointer, the game's own. Semu draws no crosshair there: the game draws the pointer the IR aims
      (a hand, a cursor) and, at Dolphin's IR defaults, it sits about 48 px above the finger and moves about 1.34
      times as far (M13), so a crosshair at the finger would be a second pointer that disagrees with it. Dolphin's own
      X arrow stays blank (CursorVisibility = 0, Never). Dolphin draws no IR pointer of its own over a game.
    - The second cursor M15 left open (Azahar's X cursor "in 3 of 12 runs") was the harness, not Azahar. The maim pair
      ran after the slow ImageMagick check of the frame, and in those three runs it straddled Semu's own idle hide:
      Semu hid the arrow 3.2-3.8 s after the move (renderer log) and the captures with and without the X cursor were
      written at +3.2 to +4.2 s and +3.5 to +4.9 s. The "X cursor" pixels were Semu's arrow vanishing between them:
      around the pointer the capture with the X cursor is pixel for pixel the frame xwd grabbed (0 differing pixels;
      xwd never holds the X cursor), and the difference is exactly Semu's 24x38 arrow box at 40,44 (276 white
      pixels, 472 with the outline on a lighter background), present in the first capture and gone from the second.
      `tests/visual/vm-azahar-layer.sh` now grabs the frame and both maim captures back to back before any slow check,
      and a pair without Semu's crosshair in both reads unchecked.
    Observed in the podman VM (Rosetta, llvmpipe and lavapipe, Xvfb at 1280x800): `tests/integration/touch-x11.sh`
    (RetroArch's DS and 3DS routes) twice: on both cores the whole crosshair (64/64 white, 224/224 black) centred on
    40,44 1.5 s after a relative move, and none 5 s later; `tests/visual/vm-azahar-layer.sh` (standalone Azahar
    through the layer, Pushmo): the same at 40,44, the captures with and without the X cursor identical (0 pixels,
    the crosshair in both), so one pointer; by eye (4x crops) one crosshair, no arrow. In touch-x11 the first tap of
    a session was missed in 3 of 4 sessions while the host's load average was 110-180 (four other tracks building);
    every later tap landed within 1 percent and this track changed no input code.
    Contracts: right_trackpad (the crosshair's shape, symmetry, clear centre, 72 opaque and 16 white cells, placement
    with the centre on the pointer at 800 and 2160 rows, whole-step growth), standalone_cursor (cursor-crosshair.sh's
    rows, hotspot and scale equal the renderer's), deck_harness (input-check.sh needs cursor-crosshair.sh and writes
    crosshair-N with both counts).
  - Item 6, the Wii controller. Why it needed a restart: only GameCube did. Wii Remote, Nunchuk and Classic already
    switched live (M14, Dolphin's profile cycle: Next Profile, HotkeyScheduler.cpp:303-321, loads a profile and its
    extension into the running remote, InputProfile.cpp:74-90). GameCube was built as "no Wii Remote at all"
    (WiimoteSource 0), and Dolphin applies a remote's source only through its own config layer (OnSourceChanged,
    Wiimote.cpp:40-60, registered at Wiimote.cpp:190), which only its Controllers window sets while a game runs; a
    file is read at boot. So stepping onto or off GameCube said RESTART TO APPLY. What Dolphin 2606a (c77bbaa) can do
    live, and what it cannot:
    - live: a Wii Remote profile (buttons, IR, motion, extension None/Nunchuk/Classic), through Next Profile;
    - live: a remote's link, through its own "Connect Wii Remote N" hotkey (HotkeyManager.cpp:81-84 and 308,
      HotkeyScheduler.cpp:275-282, MainWindow.cpp:2065-2073), which toggles the emulated remote the way a real one
      powers off and on (WiimoteDevice::Activate, WiimoteDevice.cpp:218-238); an unlinked emulated remote asks to link
      again at any bound button it reads (WiimoteDevice.cpp:345-364 and 378-385);
    - boot only (or Dolphin's own windows): a remote's source (none, emulated, real; Wiimote.cpp:40-60), a GameCube
      port's device (SIDevice), a GameCube pad's mapping and device (GCPad.cpp:38-43; the profile hotkeys cycle Wii
      Remotes only, InputProfile.cpp:190-210), the Balance Board, real remotes;
    - so GameCube becomes live this way: every connected player boots with an emulated Wii Remote (WiimoteSource 1) and
      a GameCube pad (SIDevice 6), as before; GameCube is a Wii Remote profile with nothing bound (its IR hidden, no
      motion pointer) plus one press of that player's Connect Wii Remote (Alt+F11, Alt+F12, Alt+F1, Alt+F2), so the game sees the
      remote leave and the GameCube pad play; leaving GameCube loads the bound profile first and presses Connect
      again, so the remote comes back with its buttons. Nothing about it waits for Restart Game.
    What still waits for Restart Game on the Wii: moving a player to another pad (the players page: the GameCube pad's
    device is read at boot), and the 4:3/16:9 output (the game reads it at boot). Rulings, reversible:
    - A game booted on GameCube keeps its unbound remote linked (Dolphin links every emulated remote at boot,
      WiimoteDevice.cpp:69-80; a press timed to the game's own Bluetooth start would race it). It sends nothing, so the
      GameCube pad is the only input; choosing GameCube again unlinks it.
    - Semu tracks each player's link from boot and presses Connect only when the layout changes it. If a game unlinks
      a remote itself, the worst case is a remote linked with nothing bound (harmless) or an unlinked bound remote,
      which the next button press links again (Dolphin's own rule above).
    - Dolphin's own messages are off (Dolphin.ini [Interface] OnScreenDisplayMessages = False, also set by item 2's track; MainSettings.cpp:434,
      OnScreenDisplay.cpp:157): its "Loading input profile 'Semu' for device 'Wiimote1'" and "Wii Remote 1
      disconnected" drew over the composed picture beside Semu's own toast. The FPS overlay is drawn apart
      (OnScreenUI.cpp:421-423) and stays.
    - Connect Wii Remote 1-4 are Alt+F11, Alt+F12, Alt+F1 and Alt+F2: Dolphin's defaults (Alt+F5-F8, HotkeyManager.cpp:427-430)
      belong to Next Profile and Alt+F9/F10 to the render scale (item 2); no chord Steam sends holds Alt, Alt+F4 closes
      windows under most window managers, and a contract now checks every key the supervisor types against
      Steam's chords and Dolphin's bindings under its superset matcher.
    Observed in the podman VM (Rosetta, llvmpipe, Xvfb and openbox at 1280x800, a rootful run with one replica of
    Steam's pad; real Dolphin 2606a with Mario Kart Wii mounted read-only; `PADS=1 BASICS=0 DOLPHIN_LOG=1
    TARGET=steam-deck tests/integration/live-switch.sh` with CHORDS typing Controller Layout as XTest the way Steam
    does). Nine runs; the last on the Deck target, whose identities name the replica as Dolphin's SDL does
    (SDL/0/Steam Deck Controller, with Steam's own slot-file format; the earlier runs launched linux-desktop, whose
    player 1 is SDL/0/Xbox Controller, so no pad input reached Dolphin at all, which is why they showed none). Every
    run (Connect was still Alt+F9 then; it moved to Alt+F11 when the render scale took Alt+F9/F10 on main): journal
    83/2/0, 83/3/0, 83/0/0 (Classic, GameCube, Wii Remote, each applied, never waiting), run.log "P1
    layout gamecube: 1 press(es) of Alt+F5, then Alt+F9 to unlink it via X" and "P1 layout wiimote: ... then Alt+F9
    to link it", inotify saw Dolphin open Semu.ini at each switch, the toasts read P1: CLASSIC, P1: GAMECUBE and P1:
    WII REMOTE (by eye), no Dolphin message showed over the picture, one Dolphin process to the end (restarts=0) and
    semu.json saved players.1.layout. Dolphin's own log (Logger.ini IOS_WIIMOTE, SI, CI at INFO, OUT/*-dolphin.log):
    the game accepted the remote at boot and again about a second after the switch back to Wii Remote, with the whole
    pairing handshake (HCI_CMD_ACCEPT_CON, AUTH_REQ, SNIFF_MODE), which only an unlinked remote asks for; and with
    GameCube running and the remote away, the GameCube pad's X+Y+Start held from the replica logged
    "PAD - COMBO_ORIGIN" three times (SI_DeviceGCController.cpp:270), the line only the game's own polling of that
    pad writes. The game ran at about an eighth of full speed there, so the title's response to a single press is
    left to the Deck (case C3). A tenth run on the pushed tree (51bcddb, Connect on Alt+F11) repeated all of it:
    "then Alt+F11 to unlink it" and "to link it", 83/2/0, 83/3/0, 83/0/0, three COMBO_ORIGIN lines between the
    GameCube switch and the switch back, the relink accepted the same second, restarts=0, and GFX.ini still
    InternalResolution = 1 with no render-scale action (the Connect key fires nothing of item 2's).
    Contracts: controller_layouts (the profile key, then the link key a whole gap after its release; GameCube live with
    83 reserved 0; choosing it again leaves the link alone; Wii Remote links it back; a game booted on GameCube; a
    boot-only layout still waits; Reset relinks with Alt+F12 for player 2), players (stored profiles for all four
    layouts, GameCube's binding nothing; the Connect keys in Hotkeys.ini; the checker refuses a link-less player and an
    unknown link), emulator_runtime (WiimoteSource 1 on every layout, the bound or unbound body, Connect key, Dolphin's
    messages off), radial_render (no typed key collides). The profile compiler's {"if", "line"} now carries an @block:
    line. Mutations, each run on a copy, restored with cp and checked with cmp, each failing its checks (15 of 15):
    the crosshair placed by its corner, its gap filled, the Deck check without the hotspot's half step, input-check.sh
    needing cursor-arrow.sh, GameCube back to relaunch, GameCube keeping the remote linked, no link press, no Connect
    keys in Hotkeys.ini, Dolphin's messages back on, an if-line that cannot carry a block, a bare F9 Connect key (also
    caught by the collision check: Ctrl+Shift+F9 fires F9; after the rebase onto the render scale's Alt+F9/F10, a
    Connect key equal to Alt+F9 and a bare F11 each fail it too), the checker forgetting link keys, remotes starting
    unlinked, the link state never updated, the unbound remote showing its IR. The Deck was offline (ssh timed out
    all session), so the owner's Wii session log was not read; the code had one restart path for a layout, GameCube
    (apply relaunch), which is what the report describes.

- Items 5 and 7, Fit's four states and the Wii's sizes (the placement track, 2026-10-06): built and observed
  on the Mac render host and in the editor; the Deck is open (`tests/deck/m16-check.cases`, cases F1-F4).
  - Item 7, why bezel + Fit: Screen was larger than no bezel on the Wii: two rules sized the same picture.
    With the bezel on, Fit: Screen was the one placement M13 let go fractional on the 480-line systems
    (`display.scaling.game_fractional_below: 2` on wii, gc, ps2 and dreamcast), so the Wii's 480 lines filled the
    Deck's 800 rows (1.67x). With the bezel off the compositor ignored Fit and used a separate setting, INTEGER
    SCALING (ONE SCREEN, NO BEZEL), on by default, which put the same picture at its largest whole step: 1x,
    640x480. Now one rule decides: with no bezel the picture takes the Fit state's whole step or not, so turning
    the bezel off never shrinks it (integer bezel 1x stays 1x; non-integer game's 800 rows stay 800 rows). The
    opt-in and the separate switch are gone. Checked the same way on every system (fit_states.btrc: the eleven
    single-screen bezel systems at both sizes; the bezel-off picture is never smaller than the bezelled one in the
    same state), and Wii U and Switch, which have no bezel, now have Fit too: 720 or 1080 rows docked.
  - Item 5, decisions as reversible defaults (the owner unattended):
    - The states, in the owner's order and words: integer game (`game`, the picture at the largest whole step the
      screen holds, the bezel round it cut by the screen), integer bezel (`bezel`, the whole bezel on screen, 1% of
      it allowed past the edges, the picture at the largest whole step that allows it), non-integer game
      (`game_fractional`, the picture as large as the screen holds at its aspect) and non-integer bezel
      (`bezel_fractional`, the whole bezel as large as the screen holds, the same 1%, so it is never smaller than
      integer bezel). Journal code 81 carries the index into the variants file's four placements in that order;
      the toasts read FIT: INTEGER GAME, FIT: INTEGER BEZEL, FIT: NON-INTEGER GAME, FIT: NON-INTEGER BEZEL, and a
      press that moves nothing adds (SAME), e.g. FIT: INTEGER BEZEL (SAME): the renderer compares with the state the
      press left (the old notice compared with Fit: Bezel only; 31 characters leave no room for both names). The
      menu's FIT row and the radial's Fit slot step them; the settings page's PLACEMENT became FIT with the four.
    - Defaults kept where they were one of the four: handhelds (gb, gbc, gba, psp) integer game, the DS and 3DS
      integer bezel. The TVs' fit (the nearest whole step whose opening stayed on screen) was not; the nearest
      is integer bezel, which on the Deck draws the same step for all nine TV systems (PS1, NES 2x; SNES, Genesis
      2x of 224; N64 626x474; PS2, GameCube, Wii, Dreamcast 1x). At 1920x1080 twelve of the eighteen TV cells
      draw the retired fit's step and six draw one step less (corrected in the review round; this line said only
      the PS2): the NES on both TVs, 3x (960x720) to 2x (640x480); the speakers TV of the PS1, 3x to 2x
      (640x480), and of the N64, 939x711 to 626x474; the PS2 on both TVs, 2x (1195x896, the TV cut) to 1x
      (597x448). Why: the retired fit kept only the opening on screen. At its step, with the picture centred down
      the screen (the owner's M15 ruling), the NES set's stand ends 59 px below the screen (5.5% of the set's 1076
      rows; the black set 60 px), the speakers set's 34 px for the PS1 (3.3%) and 27 px for the N64 (2.6%), and
      the PS2 sets are taller than the screen at 2x (1250 and 1292 rows); the PS1 and N64 sets hold 3x with 9 px
      (0.9%) past the edge, inside the owner's 1% (worked out from each package's `shell` and picture rectangle,
      the rule `RendererPlacement.bodyStays` applies). Decision, reversible, the review round: the TVs keep
      integer bezel as their default at every screen size. It is the integer state whose own definition is the
      whole TV on screen, 1% of it allowed past the edges, and a default is a state, not a size: a docked default
      of integer game would show the set cut on a TV and whole on the Deck. Sliding a set up to hold the old step
      would put the picture off centre (the NES about 30 px high at 1080p), the M15 item 7 complaint. Docked, one
      Fit press from the default is non-integer game (1440x1080 for the 4:3 TVs) and integer game is three
      presses on (the NES and the PS1 4x, 1280x960; the N64 1252x948; the PS2 2x, 1195x896, its old size).
      Checked docked by `tests/deck/m16-docked.cases` K1-K4. The global default is `bezel`, and a saved `fit`
      reads as integer bezel.
    - A TV's bezel is its set, never the room: the 13 Soqueroeu packages declare `shell` (the cabinet, its stand
      and, round the generic set, its speakers, measured from the art's edges against the wall and the table), so
      the bezel states keep the TV on screen round a picture centred down the screen, the room carrying on. That
      lets integer bezel show the PS1, N64, SNES and Genesis at 3x at 1080p (the whole room held them at 2x).
    - The 3D-era opt-in is retired. A Fit: Screen saved before M16 (`game` on wii, gc, ps2 or dreamcast, or the
      global `game` they inherited, which drew 1067x800 there) is read once as non-integer game (the review round,
      `SemuSettingsMigration` in src/lib/settings.btrc), so the owner's Wii opens at the size it drew; the next save
      writes the marker `visual.fit_states` with it, and a `game` saved after that is integer game. Docked, M15's
      `game` drew 2x (1280x960) and the migrated state draws 1440x1080. Reversible: drop the two calls.
    - With no bezel to place (bezel off, the DS and 3DS layouts Semu works out, Wii U, Switch) Fit steps integer
      game and non-integer game; a computed DS layout's non-integer state is its whole-step layout grown as one to
      the screen's edge. It used to toast FIT: NO BEZEL there.
    - The DS and 3DS shells keep Fit never dropping the shell: their non-integer states scale the whole shell
      (non-integer bezel: the whole shell as large as the screen holds; non-integer game: both screens at the
      shell's spacing as large as it holds, the shell round them cut by the screen), each screen filling its
      picture rectangle. Taps follow (the touch maps read the drawn lanes).
    - Sampling: a flat lane at 1x and above is sampled sharp-bilinear (`sharpPicture` in compositor.frag, which the
      editor draws too): at a whole step every screen pixel is its texel, exactly as before; at a fractional step
      each source pixel stays flat over the screen pixels inside it and blends only across the one its edge falls
      in, so no row or column doubles. A CRT shader chain renders at the lane's own size (librashader), so its
      scanlines and mask are drawn at the fractional size by the shader itself; the curved TV path keeps its
      four-tap supersampling.
    - Steam's radial label is Fit (renamed in the review round from "Fit (Bezel/Screen)"; it reaches the radial
      with the `semu-deck-cli steam input` that item 2's new slot needs anyway). The M15 case files' Fit comments
      (FIT: BEZEL, FIT: SCREEN, FIT: NO BEZEL) predate M16; m16-check.cases F5 supersedes m15 case 12's.
  - Observed on the Mac render host, judged by eye: every system with a bezel (dreamcast, gb, gba, gbc, gc,
    genesis, n3ds, n64, nds, nes, ps2, psp, psx, snes, wii) in the four states at 1280x800 and 1920x1080, the DS and
    3DS vertical shells and computed layouts in the non-integer states, the bezel off in three states (wii, gba,
    psx, nds), Wii U in both, the speakers TVs; a no-shader zoom shows sharp-bilinear edges at 6.17x and hard edges
    at 5x. At 1280x800: PS1 integer game 960x720, integer bezel 640x480, non-integer game 1067x800, non-integer
    bezel 713x535 at 283,133 (corrected in the review round from "about 700x525"); Wii 640x480, 640x480, 1067x800,
    713x535; GBA 1200x800 (5x), 480x320, 1200x800,
    about 546x364; GB 800x720, 160x144, 889x800, about 269x242; the DS shell 512x384 each in both integer states,
    about 605x454 and 568x426 in the non-integer ones. Bezel off on the Wii: 640x480 in the integer states,
    1067x800 in non-integer game. A live Fit press on the render host toasts FIT: INTEGER GAME, then FIT:
    INTEGER GAME (SAME) on the PS2 at 1280x800 (1x either way), and FIT: NON-INTEGER GAME on the PS1; the CRT
    shaders (the Wii's crt at 1.67x, GDV-NTSC at 3.33x) show no bands at the fractional sizes.
  - The editor matches the renderer (`tests/visual/editor-sync.sh --card test --placement STATE`): every variant
    of psx, ps2, wii, gb, gba, psp, nds, n3ds, n64 and genesis in each of the four states at 1280x800, and
    non-integer bezel at 1920x1080 for psx, wii, gba and nds: framing within 1 px and MAE 0 in every cell,
    the computed DS layouts included.
  - Contracts: fit_states.btrc (the names and codes, every single-screen bezel system's four states at both
    sizes and the bezel off beside them, the DS and 3DS non-integer states, the computed layouts' growth, the
    sampling, the case file and this Status), with dual_fit, tv_room, radial_choices, radial_render,
    render_variants, scene_fill, placement, gameboy, gameboy_fit, editor_dual, live_variants, reflection_audit
    and main moved to the four states. 18 mutations, each applied alone in a scratch copy of the tree, restored
    with cp and checked with cmp, each failed checks: non-integer states snapped to whole steps (8), the TV body
    tested as if centred (1), no 1% allowance (2), the bezel-off picture always whole (1), Fit with no bezel
    stepping all four states (5), a saved fit passed through (2), Wii U offered the bezel states (1), the
    computed layouts not grown (1), the SAME notice compared with integer bezel (1), point sampling at a
    fractional step (1), the editor re-centring a room in game (1), the PS1 TV's body dropped (4), the old SAME
    toast (3), the state left not remembered (2), the launch ignoring a system's saved state (18), the DS
    layouts never fractional (1), the Wii case dropped from m16-check.cases (1), the DS shell scaled short (32).
  - radial-check.cases 15 and 17 now expect the four states (the gba's Fit to integer bezel, 81/1, and Reset's
    81/0; the DS large-main layout's Fit to non-integer game, 81/2, then the vertical shell's non-integer bezel,
    81/3).
- Item 2, the render scale (the render-scale track, 2026-10-06): built and observed on the Mac render host and in
  the podman VM (RetroArch's GLideN64, PCSX2, Dolphin); the Deck is open (`tests/deck/m16-check.cases`, cases
  S1-S4, and `tests/deck/radial-check.cases` case 18). c83689f, the live Dolphin switch 8a9a049.
  - What it is: each system whose emulator renders above native declares the scales the Deck affords
    (`display.render_scale.choices` and `.default` in system.json, its cost in `.doc`); each emulator declares how
    it takes one (`platforms.<os>.render_scale`, RetroArch per core in `retroarch.render_scale` beside core options
    of kind `render_scale`). The settings radial's Render Scale slot (Ctrl+Shift+N, Lucide `zoom-in` icon) and the
    menu's SCALE row (shown where two or more scales are offered) step it, toast SCALE: 2X, save it per system as
    `visual.systems.<id>.render_scale` ("2x") and journal 88 (the index, reserved 1 while it waits). Reset to
    Default brings the default back; Restart Game applies a waiting scale. The menu now holds fourteen rows (the
    Wii's every value row with SCALE).
  - Where it goes: GLideN64 `mupen64plus-43screensize` (native size times the scale) and
    `EnableNativeResFactor`; Beetle PSX `beetle_psx_internal_resolution` (`1x(native)`, `2x`); the Flycast core
    `reicast_internal_resolution`; the PPSSPP core `ppsspp_internal_resolution`; melonDS's core switches to its
    OpenGL renderer above 1x with `melonds_opengl_resolution` (the 2D stays native); DeSmuME
    `desmume_internal_resolution`; the Citra core `citra_resolution_factor` `2x`, the Azahar core `2`. Standalones:
    Dolphin GFX.ini `InternalResolution` (the EFB scale), PCSX2 `upscale_multiplier`, PPSSPP `InternalResolution`,
    Flycast `rend.Resolution` (lines: 480 times the scale), Azahar qt-config `resolution_factor` (a title with a
    texture pack keeps its own per-title factor), melonDS `ScaleFactor`. Ryujinx: the Switch offers 1x only (the
    Deck runs it at its limit at 1x, the game's own handheld 720p), `res_scale` stays 1. Cemu: no render scale;
    its resolution comes only from per-game graphic packs, which Semu does not ship, so the Wii U offers none.
    Beetle PSX HW: Semu runs Beetle PSX's software core (`mednafen_psx`), whose software renderer upscales on the
    CPU (2x offered, 4x would be sixteen times the rasterising); the HW core is not built.
  - Live or Restart Game: Dolphin switches live. Its Hotkeys.ini binds Internal Resolution/Increase IR and
    Decrease IR (HotkeyScheduler.cpp:386-398 at c77bbaa0) to Alt+F9 and Alt+F10, and a press types the steps
    between the running scale and the chosen one, paced (40 ms held, 40 ms apart, for Dolphin's 5 ms poll); 88
    then carries reserved 0 and nothing waits. Dolphin.ini now sets OnScreenDisplayMessages = False, so Dolphin's
    own "Internal Resolution: 2x" (and its other OSD messages, the layout ones included) never draw over the
    composed picture, as PCSX2's do not; Semu's toasts announce them. Every other emulator reads its scale at
    boot: RetroArch has no command that sets a core option while running (its network commands cover states,
    shaders and the menu), PPSSPP, Flycast, Azahar and melonDS have no resolution hotkey, and PCSX2's Increase and
    Decrease Upscale Multiplier hotkeys would change the GS texel size its render hook reports one frame before
    the journal reaches the renderer, a frame drawn at the wrong size; so they say RESTART TO APPLY.
  - The renderer keeps placing by the native size. SEMU_RENDER_SCALE carries the scale and
    SEMU_RENDER_SURFACE_SCALED=1 marks a producer that reports the scaled picture's own size (RetroArch's single
    screen, whose tap reports the core's frame; PCSX2's hook, which reports GS texels): the renderer divides that
    size back to the game's before the crop and anything else counts pixels (renderer_render_scale.btrc). Above
    native the lane keeps the picture as the emulator drew it at up to the scale times native
    (renderer_scaled_lane.btrc, with its mips); the native picture is made from its last mip still at least native
    (2x and 4x exact, a 2x2 box average per level), so every shader reads native lines averaged from the larger
    render, never a single sample of it. Decision, reversible: all shaders read native (the CRT and LCD presets
    draw scanlines, masks and cells per game pixel), so under a shader a higher scale shows as smoother edges and
    steadier textures (supersampling); with the shader off the game pass draws the kept picture itself: exact where
    the screen draws it at its own size, bilinear larger, trilinear smaller, sharp-bilinear in the non-integer Fit
    states, never point-sampled shimmer, and the reflections mirror it from its own pixels. The receipts and the
    debug line name `render_scale`, `frames_scaled`, `surface0_native` and `surface0_scaled`.
  - Offered scales and the Deck's GPU (estimated, not measured: the Deck was off): an emulator's fill grows with
    the square of the scale, so 2x is four times the pixels of 1x and 3x nine times. The maxima follow the Deck's
    RDNA2 GPU (8 CUs, 1.6 TFLOPS) and the community's settled Deck settings for each emulator: GameCube, Wii and
    PS2 2x (1280x1056 and 1280x896, already past the Deck's 800 lines and about 1080p; 3x is past the GPU in most
    games), Dreamcast 2x (960 lines), 3DS 2x (800x480 top screen), PS1 2x (CPU, see above), N64 3x (GLideN64 is
    light), PSP 3x (1440x816), DS 3x (melonDS GL). RetroArch draws a frame at whole steps of its own size, so a
    scale whose frame does not fit the target's screen is never offered (`fit: display`): on the Deck the 3DS and
    Dreamcast cores and the PSP core's 3x drop out, the standalones keep theirs. Semu's own cost was measured on
    the render host (RENDER_HOST_GPU_TIME, 70 frames): N64 with GDV-NTSC 2.94 ms at 1x, 3.03 ms at 2x, 2.96 ms at
    3x; no shader 0.75, 0.29, 0.44 ms; GameCube 1.31 ms at 1x, 1.45 ms at 2x: no measurable cost.
  - Decisions as reversible defaults (the owner unattended): every default stays native (the M14 N64 frame, the
    M15 PCSX2 texel picture) except the PSP at 2x, the size its libretro path drew on the Deck before (PPSSPP
    standalone used 0, its window's size, about 3x, read back down to native anyway); the global
    `visual.render_resolution` (the libretro cores' display-sized whole step) is gone, the per-system scale replaces
    it; saved scales the target cannot offer fall back to the default; chords Ctrl+Shift+N (I is a PCSX2 keyboard
    alternate), Alt+F9 and Alt+F10.
  - Observed, the Mac render host (judged by eye, a 2560x1920 card of thin lines and a sunburst, frame producers
    drawing it at the scale and reporting that size, window producers drawing it at the scale and presenting it
    letterboxed): N64 at 1280x800, the TV at integer bezel, the same 626x474 picture at 1x, 2x and 3x (native
    313x237, kept 626x474 and 939x711): at 1x the lines break into dashes and the sunburst steps, at 2x and 3x
    they run whole; with GDV-NTSC the 3x lines are whole too, the scanlines unchanged; in non-integer game
    (3.37x) the 1x picture shows sharp-bilinear blocks, the 3x one smooth edges. GameCube (Dolphin window
    1067x800) and PS2 (PCSX2 at 2x: 1067x800 fitted at 800 rows, 1280x896 texel-exact at 1080p) the same: the TV
    unchanged (640x480 and 597x448 pictures), the edges smooth at 2x. The editor still matches the renderer
    (`editor-sync.sh --card test`, every variant of psp, n64, gc, ps2, n3ds, psx, dreamcast, wii and nds: framing
    within 1 px, MAE 0).
  - Observed in the podman VM (Xvfb 1280x800, llvmpipe, the owner's games through `semu launch`,
    `tests/integration/live-switch.sh` with SCALE and the Scale and Restart Game chords): Super Mario 64 on
    GLideN64 started native (debug line native 313x237, kept 0x0, source 939x711: RetroArch's 3x viewport);
    Ctrl+Shift+N toasted SCALE 2X: RESTART TO APPLY and saved n64 {"render_scale":"2x"}; Ctrl+Shift+D restarted
    once (restarts=1), and the relaunched core options read 43screensize "640x480", EnableNativeResFactor "2"; the
    renderer then read native 313x237, kept 626x474 from a 626x474 source, the TV in the same place (out 327,163
    626x474, tube 311,150 658x500 before and after); the logo's edges are visibly smoother under GDV-NTSC (zoomed
    side by side). Def Jam on PCSX2 the same way: native 639x448, then after the restart upscale_multiplier = 2,
    native 639x448 (1278x896 texels halved), kept 1068x749 from PCSX2's 1068x800 rectangle; the TV one pixel
    wider (598 against 597 px of 448 rows, PCSX2's own rectangle rounded without a whole step), the licence text
    clean under the CRT shader.
    The Wind Waker on Dolphin, started at 2x: the receipt reads render_scale=2 frames_scaled=0
    surface0_native=640x480 surface0_source=107,0,1066,800 surface0_scaled=1066x800 (Dolphin's window kept whole),
    the TV at integer bezel (640x480 at 320,160); the first Ctrl+Shift+N logged "semu: render scale 1x: 1 press(es)
    of Alt+F10 via X" and journaled 88/0/0, the second "2x: 1 press(es) of Alt+F9 via X" and 88/1/0, restarts=0,
    the title scene animating throughout, no Dolphin text over the picture; saved gc {"render_scale":"2x"}. Run
    again with the shader off and Fit on non-integer game (1067x800), the live switch shows in Dolphin's own
    render: the logo and the ship's stripes crisp at 2x, visibly softer after the Alt+F10 press (SCALE:
    NATIVE, Dolphin at 1x), crisp again after the Alt+F9 press (zoomed side by side), restarts=0. The
    N64 started at 2x: receipt render_scale=2 frames_scaled=1 surface0_native=313x237 surface0_scaled=626x474.
  - The settings page: SEMU SETTINGS' per-system visuals offer RENDER SCALE (the declared scales, saved where the
    radial saves them; a launch whose emulator cannot take the choice renders at the default).
  - Contracts: `tests/contracts/spec/render_scale.btrc` (the declared lists, what each emulator takes on the Deck,
    the renderer's environment, each emulator's key at a scale, the supervisor's restart and live paths with the
    typed presses, the renderer's division, crop order, kept size and mip, the SCALE row and toasts, the settings
    page), the libretro options in main.btrc's RenderResolutionContract, the variants file's scales, the
    fourteen-row menu, the Steam settings ring's eleventh slot; 19 mutations, each killed (no fit filter, no
    division, nothing kept, Restart Game blind to the scale, the binding, the environment, the core options, the
    renderer's live scale, Reset, the header, the radial slot, the pending flag, the data, the mip level, the live
    typing, Dolphin's bindings, the direction, the OSD switch, the settings field).
  - Open on the Deck (`tests/deck/m16-check.cases` S1-S4, `tests/deck/radial-check.cases` case 18): the four
    cases' pictures, receipts and rate lines (the N64 at 2x and the PS2 at 2x after Restart Game, the GameCube
    live, the PSP at 3x), each at full speed in the rate lines; the GPU load of the 2x cases in
    `tests/deck/system-matrix.sh`'s waits (lower an offered maximum where a system falls short); the melonDS
    core's OpenGL renderer at 2x through RetroArch (not drawn anywhere yet: neither the render host nor the VM ran the melonDS core above native); Azahar at 2x on a
    title without a texture pack (both written as cases S5 and S6 in the review round, Reset's live switch back as
    S7). The radial's Render Scale slot reaches Game Mode only after the release is deployed and
    `semu-deck-cli steam input` with Steam stopped installs the templates with the eleventh settings slot and
    semu-scale.png (packaging/deck/install.sh prints the step; every case types the chord itself, so the cases pass
    without it); then check by eye that the settings radial shows the zoom-in Render Scale slot between Aspect and
    Controller Layout, and that Fit's binding reads Fit.
  - On the Deck (2026-10-06): release af0470d8 installed (a delta of 9 store paths over the M15 release),
    `semu-deck-cli prepare --target steam-deck` run, then Steam's controller templates republished with Steam
    stopped (`~/.cache/semu-matrix/steam-input.sh`: Steam down in 4 s, `semu-deck-cli steam input` exit 0, Steam
    back, the screen's brightness restored to the 1209 it found). The written profile carries the slot: the
    owner's `Steam Controller Configs/<user>/config/semu/controller_neptune.vdf` (04:34) has the Semu Settings
    radial's touch_menu_button_7 "Render Scale", Ctrl+Shift+N with `semu-scale.png`, between Aspect and
    Controller Layout (the M15 file had no semu-scale at all), and the Fit slot reads Fit; the neptune-full
    template carries it, `semu-scale.png` is in Steam's binding_icons, and the user's configset already loads
    semu (autosave 1). Seeing it in the settings radial in Game Mode is the owner's (radial-check case 18 types
    the chord off-screen).
- Item 3 (GB studio): done on the Mac, the Deck check open. The owner asked for the deletion, so the `studio`
  variant left `config/systems/gb/bezels.json`, and `config/bezels/gb-studio/` and its render
  `config/assets/bezels/gb/classic.png` (nothing else used it) were removed with git rm, with the render's
  recipe in `config/assets/bezels.json`. Every reference went with them: the gb, Game Boy fit, render-variant,
  visual-cycle and settings contracts, `tests/visual/pixel-grid.sh`'s cells, the six gb rows of
  `tests/visual/reflection-audit.tsv`, M15's case 2 comment and the README's example (now gbc berry). The radial,
  menu and settings page list variants from the manifest, so gb now offers DMG and OFF. A "studio" an owner
  saved in semu.json is a stale id and falls back to the default: `tests/contracts/spec/render_variants.btrc`
  launches it and requires exactly the environment of no choice (the DMG shell), Next Bezel starting on DMG of
  `dmg|none`, the gb page showing DMG and no diagnostic.
- Item 10 (TV cutout): done on the Mac render host (M1 Max, the real renderer offscreen), the Deck check open.
  Why, in plain words: the TV screen's frame (the lip between the curved picture and the TV's opening) was
  painted on top of the TV after the room's night lighting had already dimmed the TV, so it stayed at full
  brightness, and it had a lit edge drawn round it on top; together they read as a light bar round the screen.
  The jagged curve came from how that frame meets the curved picture: the edge itself was antialiased, but the
  colour it faded into was taken at each pixel's centre, and the frame's reflection of the picture switched on
  only when a pixel's centre was outside the picture, so each edge pixel jumped between two colours as the curve
  crossed pixel centres, a notch every few rows on every curved edge. Seen at 1280x800 and 1920x1080 on all 19 TV
  variants (ps2, psx, n64, snes, nes, genesis, dreamcast, gc and wii with their alternates, the wii's 16:9 TV on
  a 16:9 card) in the old fit, bezel and game, and again after the placement track's four Fit states landed, in
  each of integer game, integer bezel, non-integer game and non-integer bezel, zoomed 4x and 5x with the test
  card, a white card and Crash Bandicoot's
  screenshot (from ES-DE's media, read-only): on the PS1 at 1280x800 the lip was #3C3E3A with a 3 px #6A6A68
  outer line inside a #24221E TV (the lit chamfer, `ring.bevel` 1 on every TV, +0.22 over 1.5% of the lip's
  half-size); an N64 column along the curved top edge ran 81 8A 93 9C A5 AE then 8E (the snap), likewise the
  PS1's and SNES's NTSC fringe rows broke into dark segments. Not the cause: the plates are mip-mapped and
  sampled trilinearly with explicit gradients at every scale, the opening mask is an analytic rounded-box
  distance smoothed over 1.5 px, and the picture is sampled through the bend with four rotated taps. Fix: the
  lip's paint stands in the scene's night light, the first multiply layer under the screens placed as the
  layer pass places it (`src/renderer/renderer_room_light.btrc`, unit 9, `roomLight` in compositor.frag); no
  TV draws the chamfer (`ring.bevel` 0 in all 13 TV packages; a new ring's default is 0, `semu bezel emit`);
  a pixel centre inside the picture edge takes the mirror at its foot on the edge (`reflectEdge`, `edgeFoot`),
  so the lip it blends toward is one colour either side of the edge; and the blend's coverage is the signed
  distance over its own screen gradient (`edgeCover`, the bend's dFdx/dFdy taken before the screen pass's
  discard), one device pixel wide however the bend stretches it, the picture carried a pixel past the edge so
  the blend never reaches the surround. The painted-lens mirror (handhelds) uses the same coverage and foot.
  After: the PS1's top lip #181714 beside the TV's #1C1B19, the left #2B2922 beside #25221F, the picture's
  colours still mirrored on it; the N64 column runs 84 8D 95 9E A6 AE B6 BE C5 CD D4 DB E2, monotonic; every
  curved edge and fringe line smooth at 4x on every TV variant at both sizes. Measured on a flat white card with
  the shader off (the edge's sub-pixel row per column along the middle 80 % of the top edge, every TV variant
  but the 16:9 Wii in the four states, 54 cells per size with the top edge on screen): before, every cell had
  notches (455 jumps over 0.25 px at 1280x800, 695 at 1920x1080); after, none (the largest jump 0.08 and
  0.19 px). Handheld edges sit on whole
  pixels, so their output is unchanged. The bezel editor compiles the same compositor.frag and now binds the
  same light (`config/editor/bezel-renderer.js`). Reversible defaults: the lip in the room's light (drop the
  `roomLight(p)` factor to undo), `ring.bevel` 0 on every TV (1 brings the lit rim back per package). Contract
  `tests/contracts/spec/tv_cutout.btrc`: the edge functions ported and run over the PS1's whole curved edge at
  1280x800 and 1920x1080 (coverage within 0.08 of each pixel's true area past the edge away from the corners,
  measured 0.031 and 0.030 over 6212 and 9004 edge pixels; every blended pixel mirroring from within a pixel of
  the edge), the shader carrying exactly those functions where both lips and the painted lens blend, the
  gradients taken before the discard, every TV variant's lip lit by its night plate with no chamfer and nothing
  else lit, the compositor and the editor binding the light before the screen pass. Each fails under its
  mutation (12 run, the file put back with cp and checked with cmp each time): the lip back on the pixel-centre
  ramp, reflectAt in place of reflectEdge, edgeFoot returning its point, roomLight dropped from the lip, bend
  without dFdx/dFdy, the gradients after the discard, the PS1's bevel back to 1, the compositor's or the
  editor's light call removed, a new ring's bevel 1, and in the port a step for the coverage (worst 0.5) or no
  foot (no mirror on 2184 blended pixels at 1280x800); item 3's studio variant restored fails six checks.
  Deck checks: `tests/deck/m16-check.cases` cases T1 to T5 (PS1 in the integer and non-integer states, N64 on both TVs,
  Genesis, GameCube in Dolphin, Dreamcast in Flycast) and G1 (GB, item 3). The Deck was unreachable from
  the Mac all session (ssh to 10.241.117.98 timed out), so no Deck capture was read.

- Items 4, 8 and 9 (the deck track, 2026-10-06): built and observed on the Mac and in the podman VM; item 4 was
  then filmed on the Deck (`tests/deck/boot-capture.cases`, below); `tests/deck/m16-check.cases` D1-D6 are open
  (the Deck did not answer ssh from 00:55 until the morning).
  - Item 4, why a game went from full size to a third of the screen in the top left and back, in plain words:
    Dolphin opens its game window at a small default size (640x480) and only asks for full screen once the game
    is running, and on Linux it draws into a separate inner window that it sizes once, from the outer window, when
    its graphics start. Gamescope stretches the small window to fill the screen (looks right), then the full-screen
    request makes the outer window 1280x800 while the inner one stays 640x480 in its top-left corner (a third of
    the screen, black to the right and below) until the game draws its next frame, which a disc boot on the Deck's
    slow SD card can delay by seconds. Sources at Dolphin 2606a: MainWindow.cpp:1266-1290 (ShowRenderWidget
    shows the widget normal), 1234 and 422-426 (full screen requested only once the core runs),
    GLX11Window.cpp:35-50 (the child window created at the parent's size), OGLGfx.cpp:475-485 (resized only at
    the next present). Semu's hook composes into that inner window, so the bezel shrank with it. Fix
    (`config/emulators/dolphin/semu-fullscreen-at-boot.patch`, X11 builds only, 1ba2de8 and 11c5b12): a game
    booting full screen shows the window full screen before the core boots and runs the event loop until the
    window manager has made it the screen's size (at most a second), and the deferred toggle is skipped. Built in
    the VM (MainWindow.cpp compiles with it). Dolphin itself does not run under Rosetta in the VM (a bus error
    with the JIT and the cached interpreter alike), so the boot is filmed on the Deck only.
  - Item 4 on the Deck (2026-10-06, `tests/deck/boot-capture.sh` over `boot-capture.cases`, off-screen in a
    private headless gamescope at 1280x800, a screenshot every 0.25 s, judged by eye on the contact sheets).
    Before, on the M15 release still installed: The Wind Waker showed Dolphin's 640x480 window stretched to
    1066x800 (9.1-10.7 s), then at 10.95 s exactly the owner's symptom, the 640x480 picture in the top left of the
    screen with black to the right and below (the X tree that instant: the outer window 1280x800, its inner GL
    window still 640x480), then the whole TV room from 11.2 s; New Super Mario Bros. Wii the same at 12.6-12.9 s;
    Dolphin's yellow messages ran over the picture. After, on release af0470d8 (2026-10-06, patch and Dolphin's
    messages off): both Dolphin cases compose into 1280x800 from their first frame (one size line each,
    "framebuffer 1280x800 (was 0x0) at frame 1"), the window and its inner GL window are 1280x800 from the first
    sample that shows them, every lit shot is the full screen (81 of 86 and 81 of 85, the rest black before the
    first frame), no shrink and no Dolphin text. Flycast, Azahar, PPSSPP, RetroArch (Beetle PSX) and melonDS
    never showed a shrunken or misplaced frame, before or after: Flycast and RetroArch go from black to the full
    picture, PPSSPP draws the PSP shell from its first shot, Azahar shows its own loading box centred, then the
    shell (it composes one frame into a hidden 100x30 widget first, never seen).
    Three emulators showed a window of their own before the game, which this item's done criterion covers too:
    PCSX2 its game list (the 1050x666 main window scaled to 1261x800) for 2.7 s before and 0.5 s after, because
    -batch still shows the main window at start (pcsx2-qt/QtHost.cpp:2428-2433 at v2.6.3) and
    HideMainWindowWhenRunning hides it only once the VM runs; Cemu its 1280x720 window with the menu bar for
    about 3.5 s, its first two shots 25 px down the screen, because CemuApp::OnInit shows the window normal
    (CemuApp.cpp:335 at v2.6) and only MainWindow::FileLoad, after mounting the title from the SD card, makes it
    full screen (MainWindow.cpp:586); and Ryujinx, in one shot of one film in four, its 1278x776 window with the
    menu bar round the loading screen just before full screen, because with a game on the command line Ryujinx
    1.3.3 goes full screen only when the game begins loading (ShowLoading, MainWindow.axaml.cs:247-256). Fixes:
    PCSX2 starts with -nogui, which never shows the main window and implies batch mode (QtHost.cpp:2146-2150), and
    Cemu carries `config/emulators/cemu/semu-fullscreen-at-boot.patch`, which shows the window full screen and
    black from the start when a game is launched with -g and -f (9d52f6f); Ryujinx's Linux build carries
    `config/emulators/ryujinx/semu-fullscreen-at-boot.patch`, which makes the window full screen in its
    constructor when --fullscreen came with a game (a70ece76; macOS's build unchanged). Contract
    boot_resize.btrc, twelve mutations killed. Filmed again on a70ece76, all ten cases (Ryujinx for 120 s): no
    shot shrunk or misplaced and no emulator window of its own. PCSX2's X tree holds only its game window,
    1280x800 from the first sample, and its shots go from black to the game; Cemu's are black from the first
    until its own full-screen shader screen, then the game (1280x720 at 0,40, composed into 1280x800 from frame
    1); Ryujinx's window is full screen 0.5 s after it takes its saved size (11.2 s, before its first painted
    frame at 14.2 s; on 9d52f6f full screen came at 13.4-13.8 s, with the loading screen's first paint), its
    loading screens full screen, then the game composed into 1280x800 from its first frame (50.5 s); its render
    window sits at 1278x706, 35 px down, while the loading screen shows and is 1280x800 by the game's first frame,
    so no frame is drawn into the small one. Dolphin's two cases, Flycast, Azahar, PPSSPP, RetroArch and melonDS as
    before.
  - Tools for every Deck question here: the renderer logs, under SEMU_RENDER_DEBUG, each framebuffer size it
    composes into ("semu-renderer: framebuffer WxH (was ...) at frame N ms=") and every two seconds the frame
    rate, the longest gap and the game phase's CPU cost; SEMU_RENDER_GPU_TIME=1 adds its GPU time (GL_TIME_ELAPSED
    queries read four frames later, never waited on). `tests/deck/boot-capture.sh` films a launch's first seconds
    (a gamescope screenshot every 0.25 s, the X window tree, the size lines; --analyse marks shots SHRUNK into
    the top left and lays them out on a sheet). input-check.sh records size, rate and scanline lines;
    system-matrix.sh also each wait's GPU load and clock and the emulator's four busiest threads.
  - Item 9, are we using the right shaders: no, not by one rule. In M14 the owner chose Retro Crisis's GDV-NTSC
    for Semu's CRTs; it went to the 240-line consoles (PS1, N64, SNES, Genesis), the NES kept its NTSC composite
    by eye, and the 480-line consoles (Dreamcast, PS2, GameCube, Wii) kept Sharp CRT (crt-easymode-halation)
    with CRT Royale one step away, because royale's mask aliased at their 1x and 1.67x sizes on the Deck. The
    rule now (reversible default): a console played on a CRT television defaults to Retro Crisis's GDV-NTSC
    (guest's crt-guest-advanced-ntsc) with his Steam Deck values for its cleanest stock signal, the closest
    published console's where he publishes none; a handheld to an LCD look; an HD console (Wii U, Switch) to
    none. He publishes Steam Deck presets for the NES, SNES, Mega Drive, N64, PlayStation, Dreamcast and PS2
    (ShaderGlass's import, retro-crisis/720p Steam Deck), none for the GameCube or Wii, which take his PS2 -
    Clean (the same 480i/480p video). The NES moved to his NES - Clean (6562dd9, the NTSC 256px composite one
    step away). The Dreamcast (Dreamcast - Clean), PS2 (PS2 - Clean), GameCube and Wii offer it one step after
    Sharp CRT (variant `gdv`) until the Deck measures it: a 640-wide frame makes its NTSC passes 2560 px wide.
    Renderer GPU time on the Mac render host (M1 Max), Sonic Adventure's frame at 1280x800 in the TV: no shader
    0.80 ms, Sharp CRT 1.45, CRT Royale 2.14, GDV-NTSC 4.33 (bezel off: 0.32, 0.95 and 3.70); the NES's GDV 1.85
    against the composite's 1.18, the Genesis's GDV 1.82. Judged by eye on the render host (Sonic Adventure, Def
    Jam, The Wind Waker, Super Mario Bros. 3 from ES-DE's media): GDV is brighter and smoother than Sharp CRT,
    whose dot mask grains at 1x; the PS2 values are soft, as he publishes them.
    Corrected in the review round: that rule holds today for the 240-line consoles and the NES only. The four
    480-line consoles still default to Sharp CRT, so the owner's own example (Dreamcast against Genesis) still
    differs and item 9 stays open until `tests/deck/m16-check.cases` D7-D10 measure their `gdv` on the Deck; the
    default moves where a console meets the bar (in integer bezel and non-integer game, every rate line at least
    59.8 per second with the longest gap under 25 ms, and the composition's GPU time at most 4 ms mean), else
    Sharp CRT stays and GDV-NTSC is one Next Shader away. The plain answer to the owner: yes, GDV-NTSC is the right
    family for the Dreamcast too, but on the Mac its 640-wide NTSC passes cost three times Sharp CRT's GPU time,
    and the Dreamcast is already the system reported slow, so it moves only once the Deck shows it holds 60 (it
    did, and the four moved: "Item 9 on the Deck" below).
    6562dd9's title ("One shader rule for every system") overstated what it changed. The toasts now name each
    preset whole (SHADER: SHARP CRT (EASYMODE), SHADER: GDV-NTSC (DREAMCAST)), so pressing Next Shader shows
    which one runs.
  - Item 9, the scanlines: guest's shader draws one scanline per `intres` source lines. The renderer now sets
    intres for each lane's size (`src/renderer/renderer_scanline_pitch.btrc`, librashader's set_param when a
    chain is built), so every scanline spans whole output rows: one per line at a whole step, and at a
    fractional one round(scale) rows (a 480-line picture on 800 rows: 400 scanlines of 2 rows, intres 1.2,
    instead of 1.67-row ones that beat into bands; never a 240-line look on a 480-line picture).
  - Item 9 on the Deck (2026-10-06, release 6c40e186; off-screen, `tests/deck/input-check.sh` in a private headless
    gamescope at Game Mode's 90 Hz and, docked, at 1920x1080 and 60 Hz, the sound on SDL's dummy driver, render scale
    1x, the owner's Sonic Adventure (USA) (Rev A), Def Jam: Fight for NY, The Wind Waker and Mario Kart Wii from the
    SD card, plus Animal Crossing: City Folk on the Wii's 16:9 output and Twilight Princess docked). Each shader ran in
    a home of its own (`visual.systems.<id>.shader_variant` and `placement` seeded), so both drew the same scenes at
    the same times; beside each run, gpu_busy_percent read ten times a second, the GPU clock and the APU's socket power
    (amdgpu power1_average). Means over each case's rate lines after its first 10 s, Sharp CRT then GDV-NTSC:
    - Integer bezel (the TVs' default; the picture 1x, 640x480, Def Jam 597x448), 1280x800: the composition's GPU
      time 1.70-1.80 ms against 5.44-5.83 ms; GPU load 10-18% against 24-41%. The frame rates the same line for line:
      Sonic's title at 60.2 per second (22 ms gaps: 60 frames on a 90 Hz screen), its attract scenes at 30.0; The Wind
      Waker's title at 30.0; Def Jam at 59.9 (16.9 ms gaps); Mario Kart Wii's attract race at 59.6-59.7, both missing
      the same 6 of 56 lines, at its track changes. A first run on a fresh release compiles Dolphin's shaders: the
      first Mario Kart Wii run (Sharp CRT) missed 41 of 55 lines, its rerun 6.
    - Non-integer game (1067x800; City Folk's 16:9 picture 1280x720), 1280x800: 2.81-3.00 ms against 7.25-7.84 ms;
      load 15-26% against 30-57%; with GDV-NTSC the socket power 4.8-10.7 W. The same rates and the same missed lines
      (scene loads: 8 of 48 on both for Sonic, 3 of 41 on both for The Wind Waker, none for Def Jam).
    - Docked, non-integer game (1440x1080; City Folk 1920x1080), 1920x1080 at 60 Hz: 4.86-5.81 ms against
      9.95-11.96 ms; load 20-46% against 37-77%; socket power 4.2-10.6 W against 4.9-12.3 W (peak 14.7 W, City Folk
      with GDV-NTSC, under the APU's 15 W limit). The same rates; the missed lines fall at the same scene loads (one
      more on GDV-NTSC, a 26.6 ms gap as City Folk's title changed scene).
    - In no sample of any run did the GPU clock pass 1040 MHz of its 1600 (power_dpm_force_performance_level auto),
      and Def Jam, the steadiest 60-frame scene, held 16.9 ms gaps on both shaders in every state.
    - So GDV-NTSC holds full speed on all four: its cost is GPU time the Deck had spare. The review round's bar capped
      the composition's GPU time at 4 ms, and every case exceeds that (5.4 ms at integer bezel), but that time is
      measured at the clock the GPU chose, and the Deck runs its GPU at the slowest clock that keeps up (200-1040 MHz
      here), so the time grows as the clock drops without a frame coming late. The bar is now what the owner sees:
      the frame rates Sharp CRT gets in the same scenes, with the GPU never the limit (its load well under 100% and
      its clock under the top). Reversible decision; the cost is battery, 0.7-1.7 W more socket power docked in
      non-integer game while a 480-line game runs.
    - Moved (reversible default): the Dreamcast, PS2, GameCube and Wii default to GDV-NTSC (variant `default`,
      Dreamcast - Clean; PS2 - Clean on the other three), so every console played on a CRT television follows the one
      rule; Sharp CRT is one press of Next Shader away (`sharp`), CRT Royale the press after, then off. The ids are the
      240-line consoles' (the NES's move in 6562dd9 did the same): a saved `default` follows the rule (the owner's PS2),
      a saved `royale` keeps CRT Royale (the owner's Dreamcast and Wii, saved on 2026-10-05: two presses of Next Shader
      reach GDV-NTSC, or Reset to Defaults), and a `gdv` saved while it was one step away is stale and falls back to
      the default, the same preset. Contracts crt_rule.btrc (every TV console's default is guest's, PLAN keeps this
      record) and crt_gdv.btrc (his values on the four, Sharp CRT the next press, the saved ids).
    - By eye (the Deck's shots, 1:1 and at 4x): the Dreamcast on GDV-NTSC is brighter and cleaner than on Sharp CRT,
      whose dot mask grains the whites at 1x; at integer bezel one scanline per line (no doubled lines), in
      non-integer game 400 even scanlines of 2 rows (no bands), docked 540 of 2 rows. His PS2 values (the PS2,
      GameCube and Wii) are softer than Sharp CRT, with his red convergence offset (deconvergence 2 output pixels)
      showing as a red edge on dark text (Def Jam's autosave notice) and a light grain (his noise): his published look,
      which the owner can rule on; Sharp CRT is the next press.
    - Deck cases: `tests/deck/m16-check.cases` D7-D10 now start on GDV-NTSC, step the Fit states and end one press away
      on Sharp CRT (80/1); D12-D15 compare the two in one run each (Reset to Defaults, four shots at integer bezel,
      four in non-integer game, four on Sharp CRT); D3's pitch line and K5's defaults name GDV-NTSC.
  - Item 9 for the owner, in plain words: no, they were not all on the right shader, and now they are. Since M14 the
    consoles that drew 240 lines (NES, SNES, Genesis, N64, PlayStation) used Retro Crisis's GDV-NTSC, the look the
    owner picked. The four that draw 480 lines (Dreamcast, PS2, GameCube, Wii) stayed on Sharp CRT, with CRT Royale as
    the other choice, because GDV-NTSC costs about three times the graphics work per frame and the Dreamcast had just
    been reported slow. That slowness was Flycast's timing on the 90 Hz screen (item 8), not the shader. Measured on
    the Deck, GDV-NTSC runs those four at exactly the speed Sharp CRT does, on the Deck's screen and docked, with the
    graphics chip never near its top speed. So the rule is now one rule: every console that was played on a CRT
    television uses GDV-NTSC with Retro Crisis's Steam Deck values for that console (his Dreamcast and PS2 ones; the
    GameCube and Wii take his PS2 values, as he publishes none for them); handhelds use an LCD look; the Wii U and
    Switch use none. Sharp CRT is one press of Next Shader away. The Dreamcast and the Wii were saved on CRT Royale in
    the owner's settings, and Semu keeps a saved choice, so they stay on it until Next Shader is pressed twice.
  - Item 8, Dreamcast speed: the measurement is ready and open. Sharp CRT plus the TV cost 1.45 ms of the M1
    Max's GPU; the Deck's GPU has about a sixth of its compute, so the composition alone may take 7 to 9 ms of the
    16.7 ms frame Flycast shares with it. Open on the Deck: Sonic Adventure's rate lines, GPU load and threads
    with the TV and Sharp CRT, with the shader off and with both off (`system-matrix.sh` with
    SEMU_MATRIX_SETTINGS), and the 480-line consoles on `gdv`. Item 8 is not done: no M16 build has reached the
    Deck, so nothing about the 23 fps has been measured yet.
  - Item 8 for the owner, in plain words: answered on the Deck on 2026-10-06 (the guess first written here, Semu's
    drawing on the GPU, was wrong). The Deck is the OLED model, and in Game Mode its screen refreshes 90 times a
    second. Flycast times each new frame by counting screen refreshes, as many per frame as the screen's rate
    divided by 60. Sonic Adventure draws many of its scenes at 30 frames a second (its movies, its story scenes and
    stages such as Speed Highway), so Flycast asked for a new frame every third refresh, and under Game Mode each
    one took the fourth: 22.5 frames a second, the "like 23fps". Flycast also waits for each frame to reach the
    screen before it emulates on, so the whole game, its sound included, ran at three quarters of its speed: the
    lag. A 60 Hz screen (a TV, an LCD Deck) never showed it, and every off-screen check ran at 60 Hz until this
    round, so none caught it. Neither the Deck's power nor Semu's TV and shader were the cause: the emulator used
    half to two thirds of one processor core, the graphics chip idled at its lowest clock, and the TV with the
    owner's CRT Royale took 2.4 ms of each frame. Semu now builds Flycast with a small change: on a screen that is not a whole
    multiple of 60 Hz it shows each frame at the next refresh and lets the sound keep the game's time, so the game
    runs at full speed (a 30-frame scene at 30, a 60-frame one at 60, the game's own clock gaining a second each
    second); at 60 and 120 Hz Flycast keeps its own timing, which was already right there.
  - Item 8's decision table: run `tests/deck/system-matrix.cases`' two Sonic Adventure lines (Flycast standalone,
    the owner's route, and the RetroArch Flycast core) four ways, as the case file's comment writes them: the
    defaults; `SEMU_MATRIX_SETTINGS='{"visual":{"crt_shaders":false}}'`; `'{"visual":{"bezels":false,
    "crt_shaders":false}}'`; and `SEMU_MATRIX_LAUNCH_ARGS='--project TRIAL'` with a copied config tree whose
    Flycast has render_preload false and rend.ShowFPS = yes (Semu's renderer not loaded, Flycast's own counter in
    the shots). Then, with m16-check.cases D3 (rate lines at least 59.8 per second, longest gap under 25 ms):
    full speed only with Semu's composition off, or GPU busy near 100% with it on: make the Dreamcast's default
    lighter (Sharp CRT off, or a lighter TV), measured again; full speed without the renderer but not with the
    bare renderer (both off): the hook's per-frame copy is the cost (renderer_compositor's extraction), profile it;
    slow even without the renderer: Flycast itself, try its rend.ThreadedRendering, rend.DelayFrameSwapping and
    pvr.AutoSkipFrame in [config] (the keys Flycast reads, flycast_keys.btrc) and compare the RetroArch core; one
    thread at 100% (the four busiest threads): the SH4 or the PVR thread is the limit, an emulator setting again;
    the SD read rate high during the stalls: the CHD's reads.
  - Item 8 on the Deck (2026-10-06, release a70ece76; off-screen, `tests/deck/input-check.sh` and
    `system-matrix.sh` in a private headless gamescope at 1280x800 with the sound on SDL's dummy driver, the owner's
    Sonic Adventure (USA) (Rev A) CHD from the SD card, the renderer's rate lines every two seconds and, beside each
    run, Flycast's busiest threads, the GPU's load and clock and the disc reads every two seconds; the decision table
    above was not needed past its first row):
    - At 60 Hz, the harnesses' refresh until now: the Sega logo and the title at 60.0 per second (longest gaps about
      17 ms); the opening movie, the attract mode's story scene (Sonic against E-101) and its Speed Highway demo at
      30.0 (gaps mostly 34 ms), the game's own rate, since Flycast presents only the frames the game draws
      (rend.DelayFrameSwapping); the demo's HUD clock read 17:33, 19:83 and 23:33 at 150.0, 152.5 and 156.0 s, full
      speed. Flycast's emulator thread used about 50-70% of one core and its render thread under 15%, the GPU sat
      at its lowest clock (200 MHz) and 0% busy in most samples, the disc read under 1.4 MB/s, and the composition took
      1.78 ms of GPU time with Sharp CRT and 2.43 ms with CRT Royale, the owner's Dreamcast shader in
      ~/.config/semu/semu.json: no processor, graphics, composition or SD limit.
    - Game Mode's own display is 1280x800 at 89.89 Hz (xrandr on its :0; this Deck is the OLED model, DMI Galileo).
      At 90 Hz (gamescope -r 90) the same run drew the 30-frame scenes at exactly 22.5 per second (gaps 45-49 ms,
      four refreshes a frame) and the 60-frame ones at 60.2, and the HUD clock read 18:16, 20:03 and 22:66 at 180.0,
      182.5 and 186.0 s: the game at 75%, the owner's report reproduced. `system-matrix.sh` at Game Mode's refresh,
      on the owner's semu.json (CRT Royale): the movie at 22.5 per second.
    - Why: Flycast 5aa091fd swaps every refresh / 60 times the game's own swap interval, truncated
      (core/wsi/sdl.cpp:86-96 and 137-153), so 3 refreshes for a 30-frame scene at 90 Hz, and under gamescope each
      such swap took 4; its emulator thread waits for its render thread (Renderer_if.cpp:59-109, 592-593), so the
      game ran no faster than its frames reached the screen.
    - Tried first, `rend.vsync = no` in a trial config tree: at 90 Hz the scenes at 30.0 and 60.2 per second and the
      HUD clock 19:00, 21:53 and 25:03 at 150.0, 152.5 and 156.0 s (full speed), but at 60 Hz, and docked at
      1920x1080 and 60 Hz, the frames bunched to the audio callbacks that alone paced the game then: longest gaps
      22 ms for 60-frame scenes and 44 ms for 30-frame ones where vsync gives 17 and 34. So the fix is Flycast's
      interval: `config/emulators/flycast/semu-swap-interval.patch` swaps at the next refresh wherever the refresh is
      not within 5% of a whole multiple of 60 Hz (90 Hz, also 75 or 144) and keeps Flycast's own interval at 60 and
      120 Hz; emu.cfg pins rend.vsync, rend.ThreadedRendering and rend.DelayFrameSwapping on, Flycast's defaults
      the measurements hold for. Built in the podman VM before the release (`patching file core/wsi/sdl.cpp`, exit 0).
    - The RetroArch Flycast core, the other route ES-DE offers, in `system-matrix.sh` at 90 Hz: about 72 composed
      frames a second with its emulator thread busy a whole core. The harness's null ALSA device never blocks
      RetroArch's audio sync, so the core ran on the 90 Hz vsync alone, faster than the game's own speed: no
      measure of the owner's route, and the core was not changed.
    - The Deck harnesses now run their private gamescope at the refresh Game Mode's display runs (xrandr on :0,
      rounded; input-check.sh's SEMU_CHECK_REFRESH and system-matrix.sh's SEMU_MATRIX_REFRESH set it, 60 when it
      cannot be read) and a case's result names it. Deck cases: `tests/deck/m16-check.cases` D3 (the movie, no
      input) and D11 (the attract mode on CRT Royale, with the HUD clock), `tests/deck/m16-docked.cases` K5 (a 60 Hz
      TV). Contract flycast_pacing.btrc.
    - After, on release 6c40e186 (installed 2026-10-06 with `deploy.sh install-delta` and `prepare`; Steam's
      templates unchanged, so no `steam input`), with the committed harnesses at Game Mode's refresh (90 Hz, read
      from :0): D3's line (the movie, no input, Sharp CRT, a fresh home at integer bezel): the logo at 60.2 per
      second (longest gap 22.1 ms), then the movie at 30.0-30.2 for 80 s (longest gaps 40.3-41.9 ms). D11's line,
      its home seeded where D7 leaves the Dreamcast (gdv, integer bezel): 80/2, CRT Royale; the title at 60.2 (gaps
      22 ms), the story scene and Speed Highway's demo at 29.9-30.3 (gaps 43.5-44.0 ms), and the HUD clock 0:79,
      6:83, 12:86, 18:86, 24:89 and 30:93 in after-17 to after-22 (132 to 162 s, 6 s apart): 6.00-6.04 s a step, the
      game at full speed (36:63 at 168 s as the demo ended, black at 174 s). `system-matrix.sh` on the owner's
      semu.json (CRT Royale): the movie at 30.0-30.2, Flycast's emulator thread 68-72% of one core, the composition
      2.5 ms of GPU time. K5 docked at 1920x1080 and 60 Hz (Sharp CRT, the picture 1x at 640,300 in the whole TV):
      the logo at 60.0 (gaps 17.1-17.5 ms), the movie at 30.0 (gaps 34.6-40.7 ms): Flycast's own interval, as before.

- Review round (2026-10-06, the M16 review): the reviewers' 30 findings on the five tracks, each checked by hand
  (on the Mac render host, in the source, at the pinned upstream), fixed in gated commits whose contracts fail
  under mutation (each mutation on a copy of the tree, restored with cp and checked with cmp), or answered below.
  This round was read-only on the Deck: the release, its install and the Deck runs belong to the Deck chain
  running beside it (items 4 and 8's measurements, item 9's shader defaults and Flycast's speed settings
  included), so every Deck check below stays open.
  - Renderer. A 16:9 picture on a 4:3 TV (the speakers TVs, the generic sets) drew below 1x in integer bezel (the
    Wii's 480 lines at 682x384 on the Deck), an "integer" state downsampled, identical to non-integer bezel:
    `RendererPlacement.wholeStep` now keeps 1x with the set cut by the screen while the picture fits the screen
    at 1x (853x480, what 3682cb6 drew), the editor's port too; only non-integer bezel shows the whole set there
    (tv_room.btrc: 32 wide cells on the GameCube, Wii, PS2 and Dreamcast TVs at both sizes). The render scale's
    game-phase lines had no contract: `RendererRenderScaling.apply` (a live switch, then the division back to
    native) is one GL-free call before the crop, and `keeps`/`picture` choose the kept picture for the extraction
    and every unshaded binding (render_scale_wiring.btrc; the reviewers' four surviving mutations, the division
    back to native, the live switch, the kept picture's binding and the capture branch, each fail it now, the last
    two rerun on a copy of the tree this round). Rounding: a bare picture keeps its unrounded size with its corner rounded as a bezel's whole
    corner is, and a framed picture is fitted on the canvas with only its edges rounded (`fitInside`), so the bezel
    off sits exactly where the bezel on does (the Genesis 896 wide at 3x, was 897; non-integer pictures at x 107
    and 196, were 106 and 195) and the PSP shell's non-integer game covers all 1280 columns (it drew 1279, its LCD
    surround in the last). Swept before and after on the render host (the render host of af0470d against this
    round's, every system's every bezel variant and the bezel off, the Wii's TV on a 16:9 card too, in the four
    states at 1280x800 and 1920x1080, shader off, 424 cells): 50 pictures moved, every one by a pixel and only
    where the old rounding put it, and no pixel changed in any other cell; no canvas moved. The bezel-off
    pictures (34 cells: non-integer x 106 to 107, the Game Boys' 195 to 196 and the N64's 111 to 112; integer
    897 to 896 wide on the Genesis and SNES at 1280x800, 1196 to 1195 at 1080p, the PS2's 1194 to 1195 at
    1080p; the N64's 939x711 from 170,44 to 171,45), which now sit exactly where the same state draws them
    with the bezel on in every single-screen system (the PSP's integer game excepted by design: its shell is
    centred down the screen there, so the picture sits 31 px above the bare one); the TVs' non-integer bezel
    pictures (14 cells), one column or row nearer their unrounded size (681 to 682 wide on the speakers sets at
    1280x800, 963 to 962 on the PS1's TV at 1080p); the PSP shells' non-integer game (2 cells, 1279 to 1280).
    The editor matches (editor-sync.sh, the four states at both sizes). Contract: picture_edges.btrc.
  - Settings. A Fit: Screen saved before M16 on the four 480-line consoles is read as non-integer game once (item
    7's record above), fit_migration.btrc. Labels: every bezel, shader and output label fits its toast whole (the
    Dreamcast's read "SHADER: SHARP CRT (EASYMODE + H"; toast_labels.btrc); Steam's Fit label is Fit; the radial,
    menu, README, action ABI, cursor policy, shader and Azahar cursor docs name what the code does.
  - Flycast. Semu's emu.cfg wrote `[rend]` and `[Dreamcast]` blocks Flycast 2.7 never reads (every option is a
    dotted key in `[config]`, option.h:107 and 414, option.cpp:68-127 at 5aa091fd), so the OpenGL pin was only
    Flycast's default and `WidescreenGameHacks = yes` never applied: now `[config] pvr.rend = 0`, `rend.*` and
    `Dreamcast.*`, the widescreen cheats written as no (the 4:3 picture the owner plays; yes would draw an
    anamorphic 16:9 picture in the TV), the dead `[input] enable_x11_keyboard` gone, and flycast_keys.btrc checks
    every written key against the pin's declared options (the profile's `upstream_keys`).
  - Dolphin's typed keys. Connect Wii Remote 3 and 4 sat on Alt+F1 and Alt+F2 (KDE Plasma's launcher and KRunner,
    GNOME's overview and Run a Command), and GNOME also claims Alt+F5 to F8 (Next Profile) and Alt+F10 (Decrease IR),
    so on a desktop those presses never reached Dolphin. Now Next Profile 1-4 Alt+Insert, Alt+Home, Alt+Delete,
    Alt+End; the render scale Alt+Prior and Alt+Next (Page Up and Down); Connect 1-4 Alt+F11, Alt+F12, Alt+F9,
    Alt+Pause (names as Dolphin's XInput2 reads them, XInput2.cpp:441-461 at c77bbaa); the chord contract refuses a
    typed key any default KDE Plasma or GNOME shortcut claims. Game Mode intercepted none of the old ones.
  - Tools. gallery.sh's flat-card check re-derived the retired fit's contain or cover and failed 34 cells; it now
    reads where the renderer placed the canvas (its debug line's bezelrect) and checks the package's picture
    rectangle on it, in each of the four Fit states (GALLERY_STATES narrows them), a DS or 3DS shell's screen in an
    integer state at its own whole step centred on its rectangle as RendererDualShell draws it (the 3DS shell's
    touch screen 1x, 320x240, round a 302x227 rectangle at 1280x800; 2x, 640x480, in a 907x680 one at 4K), and a
    picture that covers the whole screen (the Wii's 16:9 TV at 4K) measured through a 1-px border. On the render
    host at 4d1d246 with this round's tools: 272 checks at 1280x800 and 3840x2160, none failing, the worst edge
    2 px (the first run's five failures were these two gaps in the check, not in the renderer); gallery.sh and
    edge-seam.sh leave their scratch behind instead of an rm trap; tests/visual/reflection-audit.tsv is
    regenerated for the four Fit states (on the render host at 4d1d246, 1280x800 and 1920x1080: 308 rows, none
    failing; 240 cells mirror wherever a package declares one, the 64 computed-layout cells and the 4 bezel-less
    rows none), and audit_packages.btrc now wants every variant's rows in all four at both sizes; input-check.sh takes SEMU_CHECK_SIZE (a docked 1920x1080 gamescope; inject.sh already
    read the frame's size from the receipt) and hands the size to inject.sh, whose move and tap on an emulator Semu
    does not compose (standalone melonDS) now land at fractions of the whole screen instead of being skipped;
    system-matrix.sh takes SEMU_MATRIX_LAUNCH_ARGS (a trial config tree).
  - Deck cases, `tests/deck/m16-check.cases`: F1 and F2 name the TVs' non-integer bezel as the renderer draws it;
    F3's second press leaves integer bezel, so it grows the picture (no SAME), and a bezel-off press shows SAME; F5
    runs Fit through the Vulkan layer (Azahar) with taps; S5 to S7 the melonDS core's OpenGL renderer at 2x, Azahar
    at 2x and Reset's live scale back on Dolphin; C4 the Wii's single pointer; C5 standalone melonDS's own arrow
    (no Semu crosshair, the ruling below); D1 and D3 fail a dropped frame (at
    least 59.8 per second, longest gap under 25 ms: one missed frame at 59.94 Hz logs 33 ms, which the old 34 ms
    bar passed) and D3 expects no pitch line on Sharp CRT; D7 to D10 measure GDV-NTSC on the four 480-line
    consoles with the bar that moves their default. `tests/deck/m16-docked.cases` K1 to K4 run the docked sizes;
    `tests/deck/system-matrix.cases` runs Sonic Adventure for item 8's decision table. m16_review.btrc keeps them.
  - Answered, not changed:
    - The docked TVs a step smaller than release 3682cb6 (the NES on both TVs, the PS1's and N64's speakers TVs,
      the PS2 on both; item 5's record above has the sizes and the overflow): kept on integer bezel, decided per
      the owner's four states and the 1% rule. The retired fit's step left 2.6% (the N64 speakers set) to 16%
      (the PS2 speakers set) of the set past the screen edge, so no state of the four draws it: integer bezel is
      the whole TV within 1%, integer game the largest whole picture (bigger still, the TV cut). Sliding the set
      up to hold the old step would put the picture off centre, the complaint of M15 item 7. The owner can rule a
      docked TV onto integer game; m16-docked.cases shows both.
    - Item 1's other pointers: the Switch (Ryujinx) and Wii U (Cemu) show their emulator's own arrow while the
      trackpad moves (Ryujinx hides it when idle, hide_cursor 1), and standalone melonDS (offered for the DS on
      Linux, not composed by Semu) its own arrow; Semu draws no pointer there, since none of them hands Semu its
      pointer as Azahar's touch patch does. Decision, reversible: they keep the emulator's arrow for now (a touch
      screen needs a visible aim); a crosshair there needs the Vulkan layer to read the X pointer, or a Semu cursor
      theme on XCURSOR_PATH, open. So: DS and 3DS a crosshair, the Wii the game's own pointer (m16-check.cases
      C4), Switch, Wii U and standalone melonDS the emulator's arrow (C5 shows melonDS's).
    - Docked, the render scale's offered list is the target's (visual.display 1280x800), not the live screen:
      the 3DS and Dreamcast cores and the PSP core's 3x stay withheld on a TV. Reversible decision; reading the
      live display at launch is the route.
    - SemuComposedScale (a texture pack's render scale) treats the two non-integer states as the integer step
      below and counts a TV's body as if centred: approximate for TV rooms, logged.
    - The renderer's intres (item 9's scanline pitch) overrides the GDV wrappers' static intres 1 on every guest
      chain, so a PS1 or N64 480-line mode now draws 480 scanlines at a whole step: a reversal of M14's ruling of
      240 scanlines for those modes, logged in the wrappers' docs (config/assets/shaders.json); reversible by
      dropping the renderer's set_param.
    - No M16 build has reached the Deck: building and installing a release is the Deck chain's step, and this
      round was read-only on the Deck.
  - Open on the Deck, in order: build and install a release of origin/main at or after this round
    (tests/deck/build-release.sh, deploy.sh install-delta, `semu-deck-cli prepare --target steam-deck`), then
    `semu-deck-cli steam input` with Steam stopped; boot-capture.cases before and after; m16-check.cases (F1-F5,
    S1-S7, C1-C5, T1-T5, G1, D1-D10); `SEMU_CHECK_SIZE=1920x1080` m16-docked.cases; system-matrix.cases' Sonic
    Adventure lines the four ways; read-only, the owner's semu.json: a Wii saved on `game` launches in
    non-integer game and gains `visual.fit_states` with the next save.
  - Done of that list (2026-10-06, the Deck chain): the release (af0470d8, then 9d52f6f and a70ece76), prepare,
    `semu-deck-cli steam input` with Steam stopped, and boot-capture.cases before and after (item 4 above); item 8
    (Sonic Adventure at Game Mode's 90 Hz, release 6c40e186, its record above): the four-way table was not needed
    past its first row, and m16-check.cases D3 and D11 and m16-docked.cases K5 ran on their own; item 9 (the
    480-line four on GDV-NTSC against Sharp CRT, each in its own home, at 1280x800 and docked; its record above),
    whose cases are now D7-D10 and D12-D15; the rest of m16-check.cases and m16-docked.cases is still open.

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
- DS/3DS computed layouts always take whole steps from 1x up, whatever `visual.integer_scaling`
  says, from the owner's rule that dual screens sit at the largest integer scale (2026-10-03).
  That switch governs one screen without a bezel (a bezel follows PLACEMENT), and its menu
  label, INTEGER SCALING (ONE SCREEN, NO BEZEL), says so.
- Radial input delivery (2026-10-03), an exception to "agnostic of X11, Wayland, and gamescope
  in production paths": in Game Mode Steam sends the radial's key_press and the trackpad mouse
  only as XTest into the game's Xwayland (observed on the Deck: steamclient maps libXtst, Steam's
  only uinput device is the X360 pad), so no agnostic path can see them. The supervisor gets an
  optional X raw-key adapter beside evdev (`src/launch/x11_keys.btrc`): libxcb and libxcb-xinput
  loaded with dlopen from the store paths Nix bakes in, never Xlib, idle when DISPLAY is unset or
  the libraries are missing (macOS). evdev stays the contract; the adapter is an observed-
  environment shim behind a key-source seam that the contracts drive with synthetic XI2 bytes.
  Revisit if gamescope ever exposes Wayland to games or offers an input receiver.
- RetroArch keyboard ownership (2026-10-03): on Linux targets every keyboard chord goes to
  RetroArch over the command port, and RetroArch's keymap rows plus the player-1 defaults that
  share a chord key are "nul" (`formats.linux`, `keyboard_player_defaults`). On every target the
  hotkey defaults that share a chord key are cleared, plus RetroArch's three shader hotkeys
  (comma, m, n) because Semu's renderer owns shaders. The 30 hotkey and 14 player-1 keyed defaults
  of RetroArch 69a4f0e are vendored in `profile.json` `keyboard_defaults`, so a new chord that
  takes one of their keys is cleared automatically. Keyboard play in RetroArch on Linux loses
  those keys (x, s, a, q, Enter, Up, Down); pads are unaffected. macOS keeps them all.
- Pads listed in `gamepad_chords.xpad_layout_ids` (28de:11ff, 045e:*) are xpad-ordered (BTN_WEST
  is the top button), so Select+Y is Select+Y on Steam's virtual pad and on Xbox pads; a
  positional pad (Semu's test pad, macOS GameController) is unchanged (2026-10-03).
- An action an emulator does not declare in `input.actions` never reaches it: no route, no slot
  change, no journal record (2026-10-03).
- Emulator hotkey hygiene (2026-10-03). No emulator keymap or `input.actions` names `ui.menu*` or
  `visual.*` (PCSX2 OpenPauseMenu, PPSSPP Pause and Flycast btn_menu are gone, so the radial's
  Ctrl+M no longer opens a second menu). `app.quit` keymap rows render only on macOS, whose
  supervisor reads no keyboard; on Linux the supervisor quits the process group alone (Azahar Exit,
  Dolphin Stop; PPSSPP Exit App and Flycast btn_escape are gone). Each standalone `profile.json`
  declares `native_shortcuts` from its pinned source: `fallback` true means the emulator keeps a
  default for every key Semu's file leaves out, so a `collisions` line group writes every default
  that shares a Semu chord empty (Azahar: Audio Mute Ctrl+M, 3D factor Ctrl+-/Ctrl++, Browse Rooms
  Ctrl+B, Frame Advancing Ctrl+A, Status Bar Ctrl+S, Exit Fullscreen Esc, Exit Ctrl+Q on Linux);
  `fallback` false names the section Semu writes whole and the source lines that make the defaults
  inert (Dolphin, PCSX2, PPSSPP, Flycast, Ryujinx); `fixed` lists compiled-in keys (melonDS menu
  shortcuts, Cemu's window keys), each either the same action as the Semu chord or out of every
  Steam binding's reach (Cemu's Esc leaves fullscreen; no Steam binding sends Esc any more).
- Azahar reads its hotkeys only from `[UI]` `Shortcuts\<group>\<name>\KeySeq` (+ `\default=false`,
  values with a comma quoted for QSettings); the old `[Shortcuts]` group was never read, so Ctrl+S,
  Ctrl+A and Ctrl+P did nothing in standalone Azahar. Swap Screens, Toggle Screen Layout and Rotate
  Screens Upright are always empty and `screen.swap` left Azahar's actions: SemuCompose re-derives
  Azahar's layout, so any of them would break picture and touch (2026-10-03).
- PCSX2 2.6.3 applies an input profile only through a per-game settings ini (VMManager
  UpdateGameSettingsLayer), so the profile's `[Hotkeys]` never loaded and PCSX2 had no hotkeys at
  all: its keymap is now written into PCSX2.ini `[Hotkeys]` too. Qt emulators spell the main Enter
  key Return (Qt's Enter is the keypad), for Azahar and PCSX2 (2026-10-03).
- Save slots (2026-10-03): `emulator.json` `save_states` {slots, first_slot} from each source:
  RetroArch 10 from 0, Dolphin 10 from 1, PCSX2 10 from 1, PPSSPP 5 from 1, Flycast 10 from 1,
  melonDS and Azahar one fixed slot, Cemu and Ryujinx none. Semu's counter wraps both ways like
  the emulator; each profile pins the emulator's starting slot (Dolphin Qt.ini, PPSSPP StateSlot and
  SaveStateSlotCount, Flycast SavestateSlot). Dolphin saves and loads the selected slot, not Slot 1.
  RetroArch saves and loads name Semu's slot (`SAVE_STATE_SLOT n`, `LOAD_STATE_SLOT n`) and its own
  slot counter is never moved, so RetroArch's own slot OSD no longer shows; Semu's menu shows the
  slot as the emulator numbers it (`SEMU_MENU_SLOTS`, `SEMU_MENU_FIRST_SLOT`). Flycast gains next
  and previous slot (its btn_next_slot/btn_prev_slot). melonDS binds slot 1 only to its compiled
  Shift+F1 and F1, never Ctrl+S and Ctrl+A as the old package claimed, so its rows carry
  `native_chord` and the supervisor types Shift+F1 or F1 for a save or load from any source.
- Steam Input (2026-10-03, completed 2026-10-04). One definition, `config/input/steam/steam_input.json`
  (with its icons), is named by both Linux targets as `steam.input`; macOS has none. Radial rings
  are touch_menu_button_1..20 and button 0 is the centre: the quick radial is Save, Load, Previous
  Slot, Next Slot, Next Bezel, Next Shader, Menu, Screenshot, Quit with an empty centre, and the
  menu radial is Up, Down, Confirm, Back with Open Menu in the centre. A slot fires the way Steam's
  fire type 1 (button release) does: click the pad on the icon and it fires when the click is
  released; lifting the thumb without clicking fires nothing. Every radial button ticks
  (haptic_intensity 2), icons within one radial differ, and every action a binding may send has its
  own icon (Next Bezel picture-in-picture-2, Next Shader sparkles; the On/Off toggles stop
  borrowing the menu's). No binding may send a key without a modifier, so Esc (which exits
  RetroArch) is never sent and R3 in the hotkey set is Menu Back. The lower grips are Steam's
  button_back_left/right (the old *_lower names do not exist, so the Wii layer was unreachable).
  Controllers are data: each declares its controller_type, the vocabulary Steam accepts on it
  (from Valve's own templates) and a layout of presets mapping sources to group kinds, which
  `SteamInputVdf.render` walks; a contract fails on any emitted name outside the vocabulary. The
  new Steam Controller (controller_triton) gets exactly the Deck layout. The 2015 Steam Controller
  (gordon), as the owner asked of the left trackpad: left pad = the quick radial, right pad = the
  pointer with click to tap, holding the left grip turns the left pad into a d-pad, holding the
  right grip turns the right pad into the right stick (Valve's gordon joystick_move with
  output_joystick 1), long-press View = the menu radial; it has no Wii layer (no spare grips).
  Both ship as templates only (picked once under Controller Settings), since their per-user file
  names were never observed. `semu steam input` also upserts `"semu" { "autosave" "1" }` into each
  user's configset_controller_neptune.vdf (`src/steam/configset.btrc`): every other byte kept, a
  dated `.semu-*.bak` first, nothing written when the entry is there, an existing file that
  reads as empty left alone, and it refuses while Steam runs unless `--steam-root` names a root;
  without that entry Steam ignored Semu's profile and used its Last Resort template (55 loads
  observed on the Deck). Trigger groups carry output_trigger 1/2 as every Valve template does.
- Menu pause (2026-10-03): the compositor draws the Semu menu at the emulator's own present, so
  the menu pauses an emulator only where it was seen to keep presenting while paused
  (`emulator.json` `menu.pause` emulator: RetroArch, PPSSPP). Azahar, Dolphin and PCSX2 stop
  presenting when paused (VM, `tests/integration/menu-pause.sh`), and Flycast, Cemu and Ryujinx
  declare no pause action: their menu opens over the running game. Until 2026-10-04 the pad reached
  the game as well as the menu (seen on the Deck: OoT 3D went from its title to file select under
  the menu); the modal menu below ends that. Standalone melonDS has no compositor on any platform,
  so Select+Y and Ctrl+M open no menu there rather than an invisible one that paused the game.
- Modal menu (2026-10-04, follow-up F1): the open menu holds the pads for itself on every emulator,
  paused or not (`src/launch/menu_modal.btrc`). A pure state machine (free, arming, held,
  releasing) runs after every supervisor tick's drain: opening the menu arms it, and it grabs every
  pad the supervisor reads (EVIOCGRAB) only once nothing on any pad is down, then releases them after
  the menu closes once the closing press is up. Reversible defaults: an axis is at rest within an
  eighth of its travel (or its flat zone, if wider) from where it rests; signed axes rest at their
  middle, unsigned sticks (ABS_X/Y/RX/RY, or any axis centred when the pad was opened) at their
  middle, other unsigned axes (triggers) at their minimum; a pad opened mid-press is seeded from
  EVIOCGKEY and EVIOCGABS; a pad that never comes to rest after the close is released 2 s later
  (it can only lose presses that way, never gain a stuck one), but arming never times out, since
  grabbing a held button would leave it held in the game; a pad plugged in while the menu is open is
  grabbed too; quit, a stop signal, the emulator's exit and the supervisor's end release at once,
  and closing a descriptor ends its grab in the kernel, so not even a killed supervisor leaves a pad
  held. Only pads are grabbed: keyboards (and so Steam's radial keys, as XTest), the right
  trackpad's X pointer and the touchscreen still reach the game while the menu is open. macOS has no
  equivalent (GameController cannot be grabbed), so the menu over a macOS standalone stays
  non-modal. Follow-up F2, an Azahar patch so a paused Azahar keeps presenting, stays open: with
  async_presentation=false it presents from the emulation thread, which blocks while paused, and
  with async presentation its present thread also only presents queued frames (fbd3fb0
  vk_present_window.cpp:310-324 waits on present_queue), so the patch would mean
  re-presenting from a paused emulation thread inside Azahar's renderer, neither small nor safe,
  and F1 already keeps the game from seeing the menu's input.
- Per-system bezel and shader choices (2026-10-04). A system's own `bezel_variant` or
  `shader_variant` (including `none`) beats the global `visual.bezels` / `visual.crt_shaders`,
  which are only the default for systems without their own value; the settings page shows what
  the launch draws and labels the globals "(UNLESS A SYSTEM CHOOSES)". Next Bezel (Ctrl+Shift+B),
  Next Shader (Ctrl+Shift+V), the menu's BEZEL and SHADER rows (confirm or right forward, left
  back) and the old On/Off chords write only `visual.systems.<id>.*_variant`, never the globals;
  On/Off goes to none and back to the last real choice, and keeps the old global toggle only on a
  system with nothing to choose. A stale id starts on the manifest default.
- The launch writes `$state/semu-render-variants.env` (schema 1): the system, each kind's ids
  (variants in settings-page order, then none), toast-safe labels (A-Z 0-9 space : / + - , . ( ),
  at most 31 characters, none is OFF), the starting indices, and one `[bezel,shader]` section per
  combination holding only the variant-scoped SEMU_RENDER_* keys (`RendererVariantKeys`). The
  journal records `visual.bezel.select` 79 and `visual.shader.select` 80 with the absolute index in
  `slot`, so a replay after a context reset is idempotent. nds writes 21 sections in about 0.2 s.
- Wii's 16:9 partner (`widescreen_variant` tv_wide) is attached only to the manifest's default
  bezel: a chosen tv_wide or speakers bezel stays itself on widescreen games (2026-10-04).
- Toast texts are data: `actions.<id>.toast` in input.json (`{slot}` the emulator's slot label,
  `{label}` the chosen variant's), passed to the renderer as `SEMU_MENU_TOASTS` beside
  `SEMU_MENU_ITEMS` and `SEMU_MENU_ACTIONS`; the composed text is cut to the toast's 31 characters.
- The renderer's live switch (2026-10-04). It trusts only its own launch's variants file: the
  header's `token` must equal `SEMU_MENU_VARIANTS_TOKEN` (wall-clock nanoseconds and the launcher's
  pid), and schema and system must match, so a preview, the bezel editor or a stale file from
  another session never replays a session's selects. While a section is applied, a variant-scoped
  key comes from the section or nowhere, never from the launch environment. A select applies two
  game frames after the post-UI phase reads it, so the toast is on screen before any decode or
  compile stall; only the variant images reload (`renderer_variant_images.btrc`), a shader-only
  change decodes nothing, and a context reset keeps the live choice. Only records younger than
  2 s toast (a journal replay never does); a toast lasts 1.5 s, counted again from the end of a
  switch that began while it showed (the VM's software GL stalled up to 1.7 s), drawn from the menu texture with
  the menu pass at the top of the screen, and replaces the menu's SEMU header while it is open; two
  selects at once show the later one's toast. The GL-free halves (config parsing and lookup, the
  variants file, the journal's menu and toast state, the menu and toast raster) live in files the
  contracts import.
- Shader chains use librashader's disk cache (until now disabled): it lives in librashader's
  per-user cache directory (`~/Library/Caches/librashader` on macOS, `$XDG_CACHE_HOME/librashader`
  on Linux). On the M1 Max render host, Wii default to royale builds its chain in 573 ms the first
  time and 268 ms once cached. A switch to a shader that must compile shows input.json's
  `loading_toast` ("LOADING <label>") until the chain is built, then the plain toast; no worker
  precompiles chains (that would need a second, shared GL context).
- Right trackpad on DS and 3DS (2026-10-04). Steam's trackpad mouse is relative ("As Mouse"), so
  the player needs a visible pointer. On RetroArch routes Semu's renderer draws it: the RetroArch
  bridge puts its last pointer sample into the frame (ABI 3 grew a `cursor` at the end of
  SemuRenderFrameGl; the renderer reads it only when `struct_size` covers it, so emulators built
  earlier still compose, without a cursor), only on systems with SEMU_RENDER_TOUCH_SURFACE_* and
  only while RetroArch lets the core read the pointer (never in RetroArch's menu). The arrow
  (`renderer_cursor.btrc`, 12x19, whole steps of 400 rows) shows on motion or a press, hides 3 s
  after the last one, and draws after the final-frame capture, above the bezel, menu and toast,
  so screenshots never hold it. No RetroArch X11 patch: RetroArch keeps blanking its X cursor and
  the directive "agnostic of X11, Wayland, and gamescope" holds. Standalone Azahar keeps its own
  Qt cursor, which Azahar draws while the pointer moves; the pinned default (`hideInactiveMouse`
  false) never hides it, so the profile pins it true and Azahar blanks it 2.5 s after the last
  motion, like Semu's cursor (the orchestrator's ruling assumed that default).
  All four RetroArch DS/3DS cores read the same RETRO_DEVICE_POINTER snapshot: the Azahar core's
  x is centred the way the bridge cuts the 320-wide screen out of its 400-wide frame (it was spread
  over all 400, up to 40 px off at the edges, a stale point in the outer 10 %), the Citra core
  reads the pointer as a touch screen (`citra_touch_touchscreen`, mouse off), the Azahar core's
  touch is pinned on (`citra_enable_touch_touchscreen`, a default before), and DeSmuME takes
  `desmume_pointer_type` touch instead of its own relative mouse. Each core's own screen-swap
  button is taken away by a core remap file, because a swapped frame keeps its size and Semu cannot
  see it (the picture lands in the wrong hole, taps on the wrong screen): melonDS R2, Azahar and
  Citra L3 (Citra loses HOME with it), DeSmuME R3, under each core's retro_get_system_info
  library_name (melonDS libretro.cpp:114, Azahar and Citra environment.cpp:269 and :180, DeSmuME
  libretro.cpp:517); retroarch.cfg pins auto_remaps_enable and input_remap_sort_by_controller_enable
  so the files load from where Semu writes them, and video_windowed_fullscreen and
  input_auto_mouse_grab (their desktop defaults) so RetroArch never grabs the pointer. DeSmuME's
  L2 still closes the virtual lid (not a swap; the owner's call). SemuCompose's pointer map,
  written at present on the render thread and read on Qt's GUI thread, is behind one mutex.
  SEMU_RENDER_DEBUG logs each RetroArch press edge (`semu-retroarch: touch X,Y -> surface S native
  NX,NY core CX,CY`, or `-> no surface`). Out of scope, with reasons: standalone melonDS has no
  compositor on any platform, so its window is unframed and maps its own clicks (no Semu cursor);
  Switch touch (Ryujinx keeps `enable_mouse` false) and the Wii U GamePad (wiiu declares one screen
  with no touch surface) have no touch surface in Semu's model yet.
- The Deck harness for the radial (2026-10-04). `tests/deck/inject.sh` types a case's radial chords
  and moves and clicks its pointer with xdotool from the second Xwayland of the case's private
  headless gamescope (`--xwayland-count 2`; the game runs on the first). That XTest reaches the game
  only through gamescope's libei server, the route Steam's XTest takes (Game Mode's Xwaylands carry
  `LIBEI_SOCKET=gamescope-0-ei`, read on the Deck). It never types into a display it has not proven
  private. Its displays are the Xwaylands in its own mount namespace whose environment holds the
  case's clock (`SEMU_INJECT_START_MS`). The design said "the private gamescope's descendants", but
  wlroots double-forks Xwayland: on the Deck, Game Mode's Xwayland :0 and :1 are children of
  `systemd --user` (pid 1392), not of gamescope-wl (pid 1593). The rule (`inject.sh --choose`):
  exactly two displays, the game's DISPLAY one of them, both numbered 2 or higher, both sockets present
  in `/tmp/.X11-unix`, which must be a tmpfs mounted in that namespace with no X0 or X1 in it.
  Otherwise it notes the reason and sends nothing. `--unshare-net` was not added to the harness's
  bubblewrap: it would change the environment of every existing input case, and the rule already
  confines typing. Every input-check case, the old pad-only ones too, now runs with
  `--xwayland-count 2`, Game Mode's `--hide-cursor-delay 3000`, `SEMU_RENDER_DEBUG=1`,
  `SEMU_RENDER_CAPTURE_FRAME=60` (a receipt of the drawn screens before the first token) and the
  run's own `--semu-home OUT/home`, whose semu.json moves only `paths.content_root`. Isolation
  stops there: the state root stays the owner's (the journal, the variants file, receipts, the
  frame-60 capture, compiled configs that the owner's next launch rewrites, and each standalone
  emulator's own saves and states), so no case saves or loads a state on a standalone emulator.
  Only the shots right after a move or tap use gamescope screenshot type 3 (full composition, the
  cursor plane included; gamescope 3.16.30, which the Deck runs, takes the type as the screenshot
  command's third argument, steamcompmgr.cpp:1594-1600); the rest keep the base plane as before.
- One pad read twice (2026-10-04, review follow-up). The supervisor opens every gamepad node, so with
  Steam running and an external pad it read the physical pad and Steam's 28de:11ff copy of it; since
  the face swap both copies fired Select+Triangle, and the menu opened and closed in one tick. Each
  evdev pad now deduplicates as its own input (`identity`, SEMU_IDENTITY_PAD_BASE plus its slot; a
  GameController pad likewise), kept apart from the routing `source` that decides "already reached
  the emulator", so one chord read from two pads within 250 ms runs once (two players pressing the
  same chord inside a quarter second would be read as one: accepted). The supervisor also honours
  `SDL_GAMECONTROLLER_IGNORE_DEVICES` and `_EXCEPT` as SDL does (an allow list wins), the vid/pid list
  Steam puts in a launched game's environment: a pad on it is never opened, because the game reads
  only Steam's copy of it too. Keyboards are never skipped. What a device is (face layout, source,
  identity, ignored or not) is decided by `SemuInputDevices.admit`/`adopt` in
  `src/launch/input_devices.btrc` from `classify()`'s EVIOCGID and EVIOCGBIT answers, so contracts
  drive it with socketpairs. Reversible.
- Emulator keyboards against Steam's chords (2026-10-04, review follow-up). Steam types Semu's
  chords into the game's X display, so any game-button keyboard alternate on a chord's last key also
  pressed that button. PCSX2 2.6.3 registers pad buttons as axis handlers that fire on every key
  whatever the modifiers: Ctrl+A also pressed Square, Ctrl+X Circle, Ctrl+Up/Down the d-pad and
  Ctrl+Backspace Select. Its pad block now writes each keyboard alternate as {keyboard_key, line}, and
  on the platforms in `compiler.keyboard_guard` (Linux) the profile compiler leaves out every one a
  config/input chord ends on (Up, Down, X, A, Backspace today; Left, Right, I, Z and keypad Enter stay;
  macOS keeps all, as RetroArch keeps its player keys there). Dolphin has a native guard, so its Wii
  keyboard alternates are written (`KEY` & !`Ctrl`): every Semu chord holds Ctrl. Reversible: keyboard
  play in PCSX2 on Linux loses those five keys.
- Dolphin's Wii controls read the SDL pad (2026-10-04, review follow-up). The Wii Remote, Nunchuk and
  Classic Controller had read the Deck's own controller through Dolphin's SteamDeck backend, which
  opens hidraw, so the Semu menu's EVIOCGRAB never covered them; the upper-right grip R4 was Wiimote B
  while Steam's template holds the Hotkeys preset on it (Wiimote B turned A into Ctrl+P and the menu
  button into Ctrl+Q plus Select+Start); and C, Z and IR Hide used SDL names the SteamDeck device does
  not have (the desktop had the reverse). Now buttons, d-pad, sticks and triggers read `${pad}` with
  Dolphin's SDL names, as GCPadNew does, and only motion reads `${wiimote}`; Wiimote1 and the three
  mode profiles share one body, so the first launch has motion too. Dolphin lexes a bare digit as a
  number, so `Buttons/1 = 1|...` and `Buttons/2 = 2|...` had held 1 and 2 down in every Wii game;
  keys are backticked now. The main Enter key is Return to Dolphin (Toggle Fullscreen was the dead
  `@(Ctrl+Enter)`). `native_controls` vendors each backend's names at 2606a and a contract resolves
  every control token of every Dolphin file on both Linux targets. Rumble reads the pad's Strong motor.
  Reversible.
- Steam types + as KEYPAD_PLUS (main-row + needs Shift), so the standalone emulators bind the keypad
  key for Ctrl++ (Fast Forward): PCSX2 Keyboard/NumpadPlus, PPSSPP 1-157 (NKCODE_NUMPAD_ADD), melonDS
  with Qt's keypad modifier (603979819). Before, the supervisor read the chord and routed it nowhere
  while no emulator matched it.

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
- Follow-ups since: a compositor file broken at launch falls back to the built-in plain
  program (a58626a); every `system.json` declares `display.refresh_hz` (the console's rate,
  contract-tested; `SEMU_RENDER_FPS` follows it); RetroArch and PCSX2 link the renderer
  loader, not the renderer; standalone Azahar, Cemu and Ryujinx are composed through M12's
  Vulkan path, standalone melonDS stays uncomposed by design. Still true: a single-screen
  standalone is assumed to letterbox itself at `SEMU_RENDER_ASPECT` (`semu_compose.btrc`).

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
  whole); inspected at 1280x800. Ruled 2026-09-23 and in place: gb, gbc, gba and psp default to
  `game`, every other system to `fit` (checked 2026-09-25 through `semu render-env`).

### G5. Launch, input, settings, owned paths — done

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
- Decided (2026-09-25): the supervisor listens to every pad for the quit chord and the menu;
  the `device_identities` in `config/input/<target>/input.json` are for the emulators' own
  pad profiles, and narrowing the chord to them would leave a different pad (the owner's Xbox
  controller on a profile written for another) unable to quit. Since done:
  seed copies are atomic (copied beside the target, then renamed; an interrupted copy is
  replaced, contract-tested), and the owner ruled that ES-DE may sit under a link (2026-09-23).

### G6. Packaging and directive compliance — mostly done

- Dead fields (`gl_wrapper`, `sandbox`, `backend`, `launch_contract`, `managed_profile_files`,
  `settings_overrides`, `fallbacks`, `build_contract`), `install.json`, the unused PCSX2 patch,
  `retroarch.nix` and the Python gallery template are gone; RetroArch patches apply with
  `--fuzz=0`; the Deck's Steam Virtual Gamepad autoconfig ships in the bundle; PCSX2 follows the
  root renderer and btrc; the slang tree is pinned by rev; the platform matrix also checks
  `emulator.json` slices and `package.json` systems against each flake.
- Tried 2026-09-25: the root on `nixos-26.05` (c508844) does not evaluate for aarch64-darwin:
  its libretro core builder references `retroarch-bare` while installing a core, and that package
  is marked broken on darwin. Moving would mean overriding nixpkgs' problem handlers in all
  eleven core flakes, so the root stays on its exact unstable revision (e554fab), which is as
  reproducible; revisit when 26.11 lands or the core flakes stop using the nixpkgs builder.
- Remaining: nixpkgs still tracks `nixos-unstable`; ES-DE's nixpkgs (2026-01-02, insecure
  FreeImage) and the pins M4 names (Cemu v2.6, Ryujinx 1.3.3, ES-DE, RetroArch 1.22.2, PCSX2
  v2.6.3) are not refreshed; `librashader`, `syncthing` and `retroarch-joypad-autoconfig` are
  nixpkgs packages used as-is. Since done (2026-09-25): definitions can `extends` another (objects
  merge key by key, a null removes an inherited key); the macOS input file is the desktop's and the
  Deck's refines only its device and pad identities, and every target emits byte-identical
  configs to before (contract-tested). The targets stay whole: they differ mostly in whole path
  blocks.

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
angrylion. Closed 2026-09-25: the twin sends its command lists to the RDP straight from the CPU
(DPC registers), which mupen64plus-next's angrylion leaves undrawn even after 15 s; GLideN64 draws
it (spread 0.12, state saved, clean QUIT). Games submit through the RSP, which angrylion draws, so
the plugin stays and the CPU-drawn program remains the check. Closed 2026-10-03: `checks.real-cores`
now boots a freely licensed program on all 12 RetroArch systems, none needing BIOS, keys or firmware,
and passed 3 of 3 runs in the VM: genesis SGDK's image sample (MIT), snes nesdoug's SNES_09 (MIT),
nds asie's DLDI benchmark built with BlocksDS (MIT, melonDS's FreeBIOS), psp thePratz's Blue
Lightning demo (MIT, PPSSPP's HLE), n3ds mtheall's ftpd 3.2.1 (GPL-3.0-or-later, a `.3dsx` needs no
keys), dreamcast Hunter Davis's Curse of the Herder ELF (MIT, Flycast's HLE BIOS) and psx filipalac's
PS1 port of the 240p Test Suite (GPL-2.0-or-later). For psx, the raw track comes from the release zip,
and Beetle PSX 82d8e051 falls back to the MIT OpenBIOS it embeds when no Sony image is present.
The harness changed in three ways. ppsspp writes no state when SAVE_STATE follows SCREENSHOT at
once, so there is a 1 s pause. Azahar takes about 4 s to save or load and writes to its log every
frame, so the script waits until RetroArch answers again and dates files from a start marker
instead of the log. psx waits 12 s, so the screenshot shows the suite's menu and not OpenBIOS's
spinning-cube shell, which also passes the spread test. Watch: one earlier attempt had Flycast never
answer on the command port once under Rosetta; it did not happen again in 16 runs since. Still open:
nixpkgs on a release branch with the M4 pin refresh (a full rebuild of every emulator; FRACTAL-NORTH).

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
- Launch transition (2026-09-24). Every emulator entered macOS's native fullscreen, a new
  Space: ES-DE gave way to a small titled window, a slide to a black Space, then the window
  grew. ES-DE itself is a borderless window on the desktop Space, so game windows now are
  too. RetroArch has a source patch (`retroarch_darwin_fullscreen.patch`): its nib window stays
  transparent, fullscreen is a borderless screen-sized window with the Dock and menu bar
  hidden, and it fades in over its first ten presented frames. Dolphin, Azahar and Ryujinx
  (Qt and Avalonia, whose fullscreen state machines assume native Spaces) run with
  `libsemuwindow.dylib` (`src/renderer/preload/semu_window.btrc`, `window_shim` in each macOS
  platform entry): it takes `toggleFullScreen:` into the same borderless form, keeps titled
  windows transparent until then, fades them in, and reveals a window that never goes
  fullscreen (Ryujinx's controller applet) after 1.5 s. `tests/visual/mac-launch-trace.sh`
  records every emulator window at 15 ms steps with the screen locked: all four open at
  3008x1692 from their first visible frame and reach full opacity in about 200 ms; no small
  window is ever visible. Seen live on the unlocked display 2026-09-25 (Azahar: full size, alpha 0
  to 1 in about 260 ms).
- RetroArch's macOS draw loop only iterated on AppKit events, so with the display asleep
  (locked, dimmed) the game stopped after a few seconds. `retroarch_darwin_runloop.patch` keeps
  the loop awake. With the display asleep behind the lock (2026-09-25) `mac-locked.sh` passes
  all ten RetroArch systems with real library games (frames moving, state saved and loaded,
  QUIT in about 1 s); one PPSSPP save aborted inside the core in the full run, and two PSP
  reruns passed.

Still open on the Mac:
- Since then (2026-09-24/25): ES-DE through Semu.app starts in about 2 s with all 2125 games
  listed and the owner launched SNES, 3DS and N64 games from it; the note below is history.
Still open on the Mac:
- ES-DE through Semu.app, seen only through its log and stacks (the owner declined computer-use
  control): SDL3's CoreAudio open waits about six minutes on the default device, a Neural DSP
  Quad Cortex, where RetroArch's CoreAudio plays at once; listing the Drive-hosted ROM folders
  then blocks, most likely on macOS's one-time "access files in Google Drive" consent for
  Semu.app, which only the owner can grant. ES-DE's own launch command for a gb game (the `semu-retroarch` shim with an absolute `%ROM%`) plays, saves and quits via QUIT, after the bundle started defaulting and exporting `SEMU_TARGET` (the shims had run as linux-desktop and refused every macOS ROM, 3609a69). Linux checks pass in the VM with the system-folder copy (contracts 2764, launch-systems, real-cores).
- Dolphin on Vulkan (MoltenVK ships in the build) exits at once in batch mode; OpenGL stays.
- Done 2026-09-25: the bundle's Ryujinx launcher is signed ad hoc with
  `com.apple.security.hypervisor` (`ryujinx_semu.nix`, no hardened runtime so the window shim
  still loads) and macOS Ryujinx runs with `use_hypervisor` on; Animal Crossing reached its
  first scene and was composed (captured, screen locked).

### G9. Observation still missing (needs hardware)

- On FRACTAL-NORTH: Xbox pad input, Start+Select on hardware, save and load by pad, reboot
  survival (M2-M4); `menu-e2e.sh` with the BTRC pad; captures of wii, ps2 and n3ds through real
  emulators; a 16:9 title for the widescreen switch; Azahar and Cemu GL after a reboot; Steam
  launching the shortcut; `nix flake check` built on x86_64-linux.
- On the Deck: all of M5 and the Deck halves of M6 and M8 (M8: `tests/deck/radial-check.cases`
  off-screen, then the owner's Game Mode checks listed in its status).
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
  genesis, n64, psx, nds, psp, dreamcast; psx needs about 20 s to boot). The
  PPSSPP core segfaulted there under glcore on the Wayland (EGL) context: its
  bundled GLEW is built for GLX, `glewInit()` fails with no GLX display, and the
  core then dereferences a null draw context. It was first worked around with a
  per-core Vulkan driver, which left the only RetroArch tap (glcore) and showed
  the raw picture on black on the Deck (2026-10-02). PPSSPP now stays on glcore
  and is pinned to the GLX context (`video_context_driver = "x"`), which is
  what gamescope already gives every core on the Deck. `nix flake check` runs
  the contract tests and a sandboxed headless RetroArch run (Xvfb, llvmpipe,
  synthetic core) that must answer VERSION, write a non-blank screenshot and
  exit on QUIT. Not yet observed: pad input,
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
  2026-10-01, on the physical Deck (SteamOS 3.9, OLED): the old AppImage-era Semu is gone. Its
  saves, keys and captures were archived first and checked byte for byte
  (SD `Emulation/backups/old-semu-2026-10-01`, 10,422 files). Removed: the old install, its nine
  user flatpak emulators and their runtimes, its state, caches, dev trees and 25k unrooted store
  paths; /home went from 27 to 220 GB free. Release `63c8fca1` (built on the Mac in podman's VM
  under Rosetta by `tests/deck/build-release.sh`) installed through `install.sh`. `doctor` finds
  all nine emulators, and `prepare` wrote the SD ES-DE home. RetroArch gb Tetris rendered
  off-screen in a headless gamescope on the Deck's GPU; inspected: the DMG shell and LCD shader at
  5x. Two release faults found and fixed:
  - SteamOS keeps its GPU drivers against its own libc, so the release now carries nixpkgs Mesa
    and points GL, EGL, GBM and Vulkan at it (67e0855).
  - `semu steam input` failed in every bundle: the stdlib reads regular files only, and bundle
    icons are store links (c0fbf12).
  Game Mode upholds `steam-launcher.service`, so Steam is only down long enough to write its
  files while the unit is masked in /run and `steam -shutdown` quits it cleanly. Release
  `45f8abe0` (with c0fbf12) is current, `63c8fca1` kept for rollback. Done that way: the Semu
  shortcut is in the library and survived Steam's restart; the Steam Input templates, 21 icons and
  the per-user Semu profile are published; the two dead AppImage-era shortcuts and controller
  configs are gone. Still to observe in Game Mode, per system: pad input, save/load and
  Start+Select.
  The owner's first Game Mode launch showed a long loading screen, then a crash. Three causes,
  none visible from SSH:
  - Steam preloads its overlay, which needs the host's libGL.so.1. The bundle's first program
    died on it.
  - The launcher's per-store-path `basename` forks each loaded that overlay (925 of them).
  - ES-DE (on the older FreeImage nixpkgs, glibc 2.40) could not load the bundled Mesa (glibc
    2.42, `GLIBC_ABI_GNU2_TLS`). It got no OpenGL and stopped at "Couldn't create SDL window".
  Fixed: 303226d, 8657a6c, and c5e3b99 (a copy of ES-DE on the bundle's glibc and libstdc++).
  A Steam-like launch (overlay preloaded, headless gamescope) now reaches ES-DE's window code.
  In the VM, Mesa's GLX and EGL drivers force-loaded into the old ES-DE reproduce the Deck's error
  exactly, and load in the patched one. Release `5484d585` carries all three. It is to be
  installed and checked on the Deck when it is next online.
  The owner's next launch, 2026-10-02, showed ES-DE stalling from 90 to 2-16 fps on every move
  through a list, and freezing after a 3DS game. Off-screen on the Deck, with audio on and
  keyboard events sent only to a private headless gamescope:
  - the main thread deadlocked in SDL3's `ObtainLogicalAudioDevice` under the old pin's
    sdl2-compat 2.32.56 / SDL3 3.2.20, with the PipeWire backend and with Pulse;
  - on the current sdl2-compat 2.32.72 / SDL3 3.4.16, three runs (b70f198) launched a 3DS game
    through standalone Azahar, quit it through Semu, and kept ES-DE drawing and taking input
    after it. The capture shows the 3DS list live with "last played 0 seconds ago".
  The 3DS gamelist's `Azahar (Standalone)` pin is an alias now (4d74563), so the startup warning
  is gone and 3DS games open in the standalone Azahar the owner chose. ES-DE's launch shell exits
  at once and the game runs under gamescope's reaper, so ES-DE waits on the launch pipe, not on
  a child. Release `a2aec13f` carries all of it.
  The owner's capture of Ocarina of Time 3D showed both screens small inside the 3DS shell's holes,
  with the reflection fill around them. Semu pins Azahar's `use_integer_scaling`, but the
  compositor modelled a fitted layout. On the Deck's 1280x800 Azahar draws both screens at 1x in
  the middle; on a Mac screen the two models nearly agree, which is why it never showed there.
  `SemuFrameLayout` (bbd9f66) ports Azahar's LargeFrameLayout arithmetic exactly. Contracts cover:
  - the layout at 1280x800 (integer and fitted, matching the capture to the pixel) and 3024x1964;
  - Azahar's pin agreeing with its `render_frame_integer` declaration;
  - the launch passing `SEMU_RENDER_FRAME_INTEGER`;
  - render-environment completeness over every bound emulator.
  The owner refuses rebuild-and-redeploy loops for picture checks. Deck deploys are deltas now
  (682b945, dc5cad9): `build-release.sh --delta` packs only the store paths the Deck lacks, and
  `install.sh install-delta` hard-links the rest. The fix shipped as 4 paths, 7.5 MB, installed in
  21 s, with no emulator rebuilt; RetroArch loads the renderer at run time. The first delta's
  launchers carried a store-shell shebang from fixup, rolled back within minutes. The tree now skips
  fixup and checks for `#!/bin/sh`. A delta is named by its launchers as well as its store, with an
  installer case for it. Release `04fd2d58` is current. The 3DS picture on it awaits the owner's
  Game Mode launch.
  Every system on the Deck, 2026-10-02 evening, with the owner away. `tests/deck/system-matrix.sh`
  runs each of the 23 launch commands ES-DE offers through the installed release, in a private
  headless gamescope at 1280x800 on the Deck's GPU, takes screenshots, and quits through
  semu-btrc. `tests/deck/esde-tour.sh` does the same for ES-DE itself. A workflow judged every
  screenshot by eye, put each claimed defect to three refuting lenses (19 confirmed, 5 refuted,
  among them a suspected DS mis-cut that is Phantom Hourglass's own letterbox), and root-caused
  each group offline. Fixed, each with a contract or a render-host proof:
  - Standalone Azahar deadlocked at start, 0% CPU, no window. Its main thread sat in
    `SDL_LockJoysticks` under `InitJoystick`, against its own SDL poll thread: a lock-order
    inversion in upstream Azahar that the old release had too. Patched (3d05d20, Azahar
    rebuilt): 3 of 3 Ocarina of Time cold starts on the Deck now open in 20 s with both screens
    filling the shell's holes (before: 0 of 3).
  - Azahar's UI pins used names it ignores, so `confirmClose` stayed on behind the compositor
    (f34705a). ES-DE crashed on its first sound when no sound server answered; dummy is its last
    SDL audio driver (1aba0f6). Cemu's notification box over the game (a470c5f).
  - Dolphin drew the VI's analog aspect (and Wii in 16:9), so its own black bars showed inside
    the 4:3 hole: pinned CustomStretch 4:3 and `SYSCONF.IPL.AR=False` (bffc1f2); the single-screen
    cut takes only pixels wholly inside the letterbox (f74a176).
  - PSP on the PPSSPP core ran Vulkan, outside the GL tap, with no bezel: glcore with a GLX
    context (56e7d6f). GBC, GBA and PSP LCD grids, and the DS grid's banding (2a533c7).
  - Reflections now mirror the shaded picture, the curved CRTs lose their moire rings, and no
    dark seam rims a bent picture edge (e09c10a). A frame larger than the screen (PPSSPP set for
    a bigger display) is scaled down to fit instead of drawn at 1x past the edges (268c8a8,
    observed in the VM with real RetroArch and PPSSPP).
  Not product defects: Ryujinx off-screen stops at its controller applet, because Steam's
  virtual pad exists only in Game Mode (Ryujinx itself starts and composes). The SD card reads
  2.0 MB/s even sequentially, so big zipped ROMs open about 20 s late on RetroArch.
  `tests/visual/gallery.sh` without `--quick` had gone red (16 curved 4K cells, up to 7 px): it
  measured placement on a picture bent, bloomed and mirrored by the tube. Its measuring renders
  now turn those off, and all 68 cells sit within 2 px again. The release with all of the above
  is built from 268c8a8 as a 55 MB delta (5 store paths, no emulator rebuilt) and installs when
  the Deck is next online.
  Installed and re-swept 2026-10-03: all 23 cases quit cleanly on releases 6ce00f0e, 7ffab6db and
  df7c9f07, and a by-eye before/after review confirmed the round's fixes on the Deck. The
  overnight pass (owner asleep, Deck held awake) then found and fixed, each with a contract or a
  render-host proof:
  - Bezels off and every computed DS/3DS layout drew a black picture since e09c10a (a zero-residue
    picture test; 91029ed), seen on the Deck too and fixed there.
  - PCSX2 drew PS2 into its first 640x480 corner in 3 launches of 4: a fullscreen resize arrived
    before its GS device existed. Semu's patch follows the EGL surface's real size each present
    (559c3ee); 4 of 4 correct on the Deck. The sweep now gives PCSX2 a silent ALSA device, so its
    audio error dialog no longer hides the game (4e094e3).
  - The bezel editor draws its preview through the renderer's own compositor.frag (WebGL2), frames
    each system as the renderer does, previews the computed DS/3DS layouts, and has a Production
    view (the real renderer's frame); `tests/visual/editor-sync.sh` puts editor and renderer side
    by side for every variant (MAE 0 on the variants checked). Production fixes from the same
    comparison: computed layouts choose their scale from the screens alone (stacked DS 2x, not 1x)
    and centre the pair when that is better; no edge-clamped glow on drawn frames; gb-studio's
    integer placement and lens reflection; lip bevel as ring data; LCD packages lose the CRT look.
  - GBC/GBA LCD grids and colour: Gambatte's own GBC correction is turned off when the chosen
    shader has a colour-matrix pass (a variant's `core_options`), saturation stays under each
    colour pass's clip point, the 3DS touch screen resamples sharp-bilinear. The render host now
    settles 12 frames (history passes), with ramp, line, bar and clip cards.
  - About fifty cleanup commits removed verified dead code, unread keys (system.json render,
    verification, aliases and bios blocks; emulator default_system; steam_input key_names) and
    stale docs. Left for the owner, since nothing was deleted: config/assets/fonts/menu-font.png
    and packaging/sync/{package.nix,semu-syncthing.service.template} are orphaned.
  - All ten Linux checks pass in the VM (platform-matrix again after Azahar's and RetroArch's
    package.json learned their macOS build), and the macOS contracts in the Nix sandbox.
  Open: the Deck's charger cannot keep up with Steam's idle UI (it discharged about 5 W net while
  plugged in), so the final release waits for the Deck to come back charged; GBC/GBA mid-greys
  are darker after removing the gamma lift that forced the clipping (owner's call on the Deck).
  2026-10-03, the owner's "Dreamcast crashed when opening": ES-DE launched Resident Evil Code:
  Veronica at 09:19:43 with its folder's .m3u, play time 1 s. Standalone Flycast 2.7 opens no
  .m3u (OpenDisc knows chd, gdi, cdi, cue) and a command-line start exits with code 0 on "Unknown
  disk format"; PCSX2 2.6.3 has no .m3u reader either (Devil May Cry 2). Read from the pinned
  sources and the Deck's logs, not run there (SSH stayed read-only). Fixed offline, no emulator
  rebuilt (abd75a8, deaa703, d3b7318):
  - Each emulator.json platform declares `playlist.open`: `first_entry` for flycast and pcsx2
    (Semu passes the playlist's first disc, held to the ROM-directory rule, with
    `playlist.args`), `native` for dolphin and retroarch (byte-identical argv). Flycast also gets
    a transient `-config config:Dreamcast.ContentPath="<the game's folder>"`, so Commands >
    Eject Disk, Insert Disk lists that game's discs. SemuChecker fails any binding of a system
    listing .m3u whose emulator declares nothing.
  - Dolphin.ini `AutoDiscChange = True` (it loaded GC playlists but waited at the swap prompt);
    Flycast's disc picker in list mode, no box-art fetch.
  - Extensions no bound emulator opens are no longer offered: dreamcast .iso, ps2 and gc
    .7z/.zip, psx .ecm, genesis .32x (none in the library).
  `--print-plan` over the Mac library gives disc 1 plus the ContentPath pair for all 7 Dreamcast
  playlists and DMC2's disc 1, and keeps the .m3u for the 8 GC playlists. Awaits observation on
  the Deck once launches are allowed: a `--delta` release, the four multi-disc cases at the end
  of `tests/deck/system-matrix.cases` (the script now gives repeated cases their own directory
  and records the emulator's argv), and a manual disc swap in each emulator. Left for the owner:
  gc `Baten Kaitos Origins (USA).m3u` lists Disc 1 twice and no Disc 2.
  Review follow-ups, 2026-10-03 (offline, Deck untouched):
  - The extension check is general now. Each emulator.json declares `content.extensions`: what
    the emulator opens at its pinned revision, with file and line in `doc.content`. RetroArch
    declares `content.archives` (.7z and .zip, unpacked unless a core sets `block_extract`) and
    `content.cores.<core>.extensions` (each core's own valid_extensions). For every target,
    SemuChecker requires each extension a system offers to be opened by one of its bindings,
    matched on the last suffix, so .iso covers .nkit.iso. A first_entry playlist counts as opened.
    That trimmed the extensions no bound emulator declares; none of them occurs in the Mac or
    Deck library (read-only find on both):
    - wii .7z/.zip: Dolphin opens no archive;
    - switch .7z/.zip: Ryujinx 1.3.3's AppHost.cs loads .xci/.nca/.nsp/.pfs0 and treats anything
      else as homebrew, so it opens no archive either;
    - gb .bin/.gbs, gbc .bin/.cgb, gba .agb/.bin/.cgb, n64 .d64/.ndd, nes .3dsen/.bin,
      snes .bin/.bml/.bsx, psx .psexe, nds .app.
    Reversible default: a format becomes available again once its emulator's declaration lists
    it, with source evidence.
  - A native playlist binding must open .m3u itself, judged per RetroArch core. Adding .m3u to n64
    (Mupen64Plus-Next) now fails `build configs`. Contracts now pin three rules: fileInside's
    regular-file check (a playlist naming a folder is refused), an absolute playlist entry inside
    the ROM directory (kept as written), and `native` as the default `playlist.open`.
  - Flycast's ContentPath argument is quoted as data (`playlist.quote`): double quotes, or single
    quotes when the folder holds a double quote. A folder holding both quotes or a semicolon gets
    no picker argument, and the plan's `notice` says why; disc 1 still boots. A contract port of
    Flycast's -config grammar and list loader reads the argv back to the exact folder.
  - `system-matrix.sh` and `input-check.sh` record the emulator's argv the moment it appears,
    with every error silenced. In a container with an emulator that exits after 1 s, the old
    scripts printed `/proc/<pid>/cmdline: No such file or directory` and recorded nothing; the new
    ones record disc 1.
  Still open from that review: a loose `X.m3u` sitting in the system folder points Flycast's
  ContentPath at the whole Dreamcast folder. Booting is unaffected, and every library playlist
  sits in its own folder.
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
  ES-DE settings menu: ES-DE 3.4.1 (upgraded from 3.4.0 on 2026-09-23) carries the settings-menu patch; the
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
  2026-10-03, radial input delivery (offline, Deck untouched): radial chords reach the
  supervisor however Steam sends them and fire once (rulings: the X raw-key adapter, RetroArch
  keyboard ownership, xpad layouts). Observed in the podman VM under Xvfb with
  `tests/integration/input-x11.sh` (xdotool XTest into a real RetroArch 1.22.2 running the
  synthetic core, through `semu launch retroarch --system gb`): the supervisor logged "listening
  for keys on X display :91"; Ctrl+M, Ctrl+M, Ctrl+K, Ctrl+H and Ctrl+Shift+F9 each ran exactly
  once; the journal read menu, back, state.next slot 1, shader switch; RetroArch answered
  GET_STATUS PLAYING after Ctrl+K and Ctrl+H never reset the core. The same session with the
  previous RetroArch profile (a3cb872) was PAUSED after Ctrl+K (frame advance on k) and reset the
  core on Ctrl+H. A positional virtual pad and a replica of Steam's virtual pad each opened the
  menu with Select+north and closed it with B, Select+west on the replica did nothing (its
  BTN_WEST is the top button), and Start+Select quit. semu-btrc in that build loads
  /nix/store/cdpwb5...-libxcb-1.17.0, the libxcb the Deck's Mesa already ships. Not observable
  there: Semu's uinput typing never reaches a bare Xvfb, so the echo suppression rests on the
  contracts (a socketpair through one real pollDevices tick, among others). Open for the Deck:
  Steam's own XTest from :0 through gamescope's EI into the game's Xwayland, and the radial.
  2026-10-03, emulator hotkey hygiene and the menu pause (offline, Deck untouched; rulings above).
  Observed in the podman VM under Xvfb with `tests/integration/menu-pause.sh` (real games through
  `semu launch`, llvmpipe and lavapipe, ROMs and the PS2 BIOS folder mounted read-only):
  - Standalone Azahar now reads Semu's hotkeys: Ctrl+P paused Pushmo, and Ctrl+S wrote the quick
    save `states/0004000000068E00.00.cst`. With the previous profile (779555c, the same harness)
    Ctrl+P left it running, being Azahar's default Capture Screenshot, and Ctrl+S wrote nothing.
  - Paused, Azahar presents nothing: a menu record written while paused stayed invisible and showed
    the moment it resumed (the same with the old profile and Azahar's own F4). Its `menu.pause` is
    none, so the menu opens over the running game.
  - PPSSPP keeps presenting while paused: Ctrl+P froze Burnout Legends' attract mode and the menu
    showed over the frozen frame at once, with SLOT 1; its `menu.pause` is emulator. Ctrl+S wrote
    `PPSSPP_STATE/ULUS10025_2.00_0.ppst`, slot 1 as PPSSPP numbers it.
  - Dolphin and PCSX2 stop presenting while paused, as Azahar does (Animal Crossing and Marvel vs.
    Capcom 2, openbox as the window manager because Dolphin takes no hotkey without focus): Ctrl+P
    froze each game and a menu record written meanwhile showed only after Ctrl+P resumed it. Both
    are none. Ctrl+S wrote Dolphin's `StateSaves/GAFE01.s01` (the selected slot 1) and PCSX2's
    `sstates/SLUS-20486 (4D228733).01.p2s` (slot 1).
  - PCSX2 had no hotkeys at all until now: with the previous profile (779555c, the same harness,
    hotkeys only in the input profile) Ctrl+P left the game running and Ctrl+S wrote nothing.
  The real renderer on the Mac (render host) draws SLOT 10 for Dolphin's last slot, SLOT 3 for
  RetroArch's index 3 and no slot line for Azahar's single quick slot. Not observable offline:
  Steam's radial chords reaching Azahar and the others in Game Mode, and melonDS typing Shift+F1
  (uinput typing never reaches a bare Xvfb).
  2026-10-04, per-system variant choices (offline, contracts only; rulings above): every
  combination of every system compiles into the variants file with only scoped keys differing,
  the launch's own section equals compile's, nds through `semu render-env --variants-file` gives
  21 sections, and Next Bezel, the menu rows, left/right and the On/Off chords journal 79/80 with
  the right index and save only the per-system key (also through one real pollDevices tick fed raw
  XI keys). The renderer applying a select live is the next commit.
  2026-10-04, the renderer applies the choice live (offline, Deck untouched; rulings above). On the
  Mac render host (`tests/visual/render.sh OUT 1280x800 'system:bezel:shader>bezel:shader'`, which
  journals the selects through `render-host --switch`), nds shell>main_right, nds main_right>none:none,
  n3ds shell>stacked:grid, gba shell>arctic:agb001, gb dmg:dmg>dmg:none and Wii 16:9
  tv:default>speakers:default and >tv:royale each showed the toast band two frames after the
  selects and then the new bezel, layout or shader; every switched frame is pixel-identical to a
  fresh launch on that choice outside the toast band (Wii speakers on a 16:9 picture: the 4:3 TV,
  no tv_wide). phase=switch receipts give reload_ms 464 for the gba arctic shell, about 70 for the
  DS/3DS computed layouts (their wood background) and none for a shader-only change. A context
  reset after the switch (`RENDER_HOST_RESET`) redraws the switched picture exactly, and the menu
  rows read BEZEL ARCTIC and SHADER AGB-001 LCD. In the podman VM (`tests/integration/live-switch.sh`,
  xdotool typing the radial's chords as XTest, llvmpipe), RetroArch gba (240p Test Suite, whole
  shell), RetroArch nds (melonDS core, DLDI benchmark) and standalone Azahar n3ds (Pushmo) each
  journalled 79 then 80, switched (indigo to arctic and AGB-001; shell to large main right and LCD
  grid, LOADING shown while its chains compiled), saved `visual.systems.<id>.*_variant` and
  listed the new values in the menu. The arctic shell took 970 ms to decode under llvmpipe and the
  DS layout's two chains 1.7 s; with the toast counted again after the stall, BEZEL: ARCTIC still
  showed over the new shell 2 s after the chord. Not observable offline: Steam's radial in Game
  Mode, and the Deck's own decode and compile times.
  2026-10-04, Steam Input (offline, Deck read only; ruling above). `semu steam input --target
  steam-deck --steam-root <mktemp>` with a configset holding two other apps wrote neptune-simple,
  neptune-full, triton-full and gordon-full, 25 icons byte for byte, the per-user profile (equal to
  neptune-full) and the configset with `"semu" { "autosave" "1" }` added before the closing brace,
  every other byte kept, one `.semu-<date>.bak` equal to the old file; a second run printed
  "already loads semu" and wrote nothing. Read: the quick ring is touch_menu_button_1..9 with no
  button 0, the menu radial's button 0 is Open Menu, the Wii layer is held with button_back_left/
  right, triton-full equals neptune-full apart from controller_type and title, and gordon-full has
  no dpad or right_joystick source (left pad radial, grip-held d-pad and right stick). The same
  command for linux-desktop wrote the same files, macOS has no definition, and with a live
  `~/.steam/steam.pid` and no `--steam-root` it refused before writing anything.
  `packaging/steam/render-icons.sh --check` re-rendered all 21 Lucide icons (17 old, 4 new) from
  Lucide b442632 at RMSE 0 against the committed files; the four new ones were inspected on a dark
  background. Still for the Deck: Steam loading the profile through the configset (controller_ui.txt
  naming config/semu/controller_neptune.vdf as the Local Selection Path), the ring's feel and icons
  in Game Mode, and both Steam Controllers on hardware.
  2026-10-04, the right trackpad on DS and 3DS (offline, Deck untouched; ruling above). In the podman
  VM under Xvfb (`tests/integration/touch-x11.sh`: xdotool XTest pointer, real RetroArch 1.22.2 with
  this tree's bridge and renderer, the synthetic core standing in under each library name, the
  launch exactly `semu launch retroarch`'s plan plus --verbose, llvmpipe at 1280x800), taps half way
  down the bottom screen of the default shells at 5, 50 and 95 % across it reached the core at
  0.1397, 0.5000 and 0.8576 across the Azahar core's 400-wide frame (wanted 0.1 + 0.8 f: 0.14, 0.50,
  0.86; native 15.8, 159.5 and 302.1 of 320) and 0.7489 down, and at 0.0498, 0.5000 and 0.9484 on
  the melonDS core (native 12.7, 127.5, 241.8 of 256) and 0.7500 down; a tap on the bezel between
  the screens pressed nothing and was logged `-> no surface`; RetroArch's log shows it loading
  `remaps/Azahar/Azahar.rmp` and `remaps/melonDS/melonDS.rmp` from Semu's state root. Semu's cursor
  showed in the xwd capture 1.2 s after the pointer moved onto a static corner of the plate (472
  pixels differ, the whole 24x38 arrow) and was gone 4.5 s later (inspected). On the Mac render host
  the cursor is exactly that arrow over the 3DS, DS and GBA shells, and the menu and toast frames of
  a live switch are pixel-identical to the previous commit's. Standalone Azahar
  (`tests/visual/vm-azahar-layer.sh`, now with Semu's own compiled qt-config.ini, Pushmo, lavapipe
  under the layer, 1280x720): a click held on OK of the composed bottom screen (1093,560) mapped to
  639,556 in Azahar's layout and dismissed "Save data created" into the intro, with the mutex in
  SemuCompose; Azahar's X cursor (the default arrow, captured with maim against maim -u) showed
  while the pointer moved (94 pixels) and was blanked 4 s after it stopped (0); with the pin
  replaced by the pinned default (`HIDE_INACTIVE_MOUSE=false`) it was still there after 4 s (94).
  RetroArch with the new bridge also builds for aarch64-darwin, as does the macOS renderer (window
  shim and MoltenVK stand-in, both with the SemuCompose mutex). Of the emulators, the header change
  rebuilds RetroArch and PCSX2 (both link the loader, which carries the header) in the next
  release; no other emulator does.
  Still for the Deck: Steam's real trackpad through gamescope (the cursor over RetroArch, Azahar's
  own cursor under gamescope's hide delay), a soft-press tap in a real game on each core, and the
  remap files taking R2/L3/R3 away on the virtual pad.
  2026-10-04, the Deck harness for the radial (offline, Deck read only; ruling above). Built:
  `tests/deck/input-check.sh` takes radial and trackpad tokens beside pad buttons, all on one clock
  that the pad and the new `tests/deck/inject.sh` share. Token i fires FIRST + i*GAP seconds after
  launch, a press's own time comes off its pause, and `a+b` is a pad chord. `key:CHORD` types the
  chord as XTest from the private gamescope's second Xwayland. `move:FX,FY` parks the pointer, then
  moves it relatively to that fraction of the touch screen as the newest receipt of the launch draws
  it (a switch receipt included); `tap:` adds a 0.15 s click. Shots 0.5 s and 4 s after a move or tap
  keep gamescope's cursor plane (type 3). A case's result now also lists the X key adapter's
  display, each action by source, deduplicated chords, the touch lines, the journal (`journal.od`
  plus action/slot pairs), this launch's receipts (art, preset, layout, switch indices) and the
  choices saved in OUT/home. `input-check.sh --plan CASES` prints every case's pad argv, injector argv
  and merged timeline without launching anything. `tests/deck/radial-check.cases` holds 13 cases:
  RetroArch gba with Next Bezel, Next Shader, Next and Previous Slot and Screenshot; gba again with
  the menu from Select+Y (BEZEL to OFF behind it); melonDS-core taps before and after a layout
  switch; standalone Azahar taps and a switch; the menu over running Azahar from Ctrl+M (the F1
  probe: until the menu holds the pad, its A also reaches the game); taps on the Azahar, Citra and
  DeSmuME cores; psx Ctrl+H and Ctrl+M twice; slot chords and a live switch on Dolphin, PCSX2 and
  PPSSPP; Next Bezel on Cemu (no bezels). Each case's expected evidence is written above it, and no
  case saves or loads a state. Observed on the Mac: `bash -n` on both scripts under bash 5.3 and macOS
  bash 3.2, and `--plan tests/deck/radial-check.cases` printing all 13 timelines, the same under both
  shells. The contracts (`tests/contracts/spec/deck_harness.btrc`) run that plan and the display
  rule against bound socket fixtures (3 accepted, 10 refused cases), tie the cases' chords to
  input.json and their journal codes to action_abi. They fail under each of 10 mutations of the
  scripts and the cases (no :2 floor, no tmpfs check, X1 allowed, any namespace, typing on after a
  refusal, one Xwayland, a press's time left in its pause, the cursor shots without type 3, the old
  On/Off chord, the owner's Semu home). The nix contracts check's source now includes tests/deck,
  which the older harness spec also reads and the fileset left out; the aarch64-darwin check passes
  with all 5221 checks (unsandboxed, as this Mac builds). In the podman VM (scratch runs, not
  committed), a stubbed run of input-check.sh (gamescope, the CLI, semu-btrc, the emulator,
  gamescopectl and the pad replaced; bubblewrap, xdotool and inject.sh real) wrote the planned
  inner.sh, passed the gamescope flags and the compensated pad argv, captured within 0.2 s of each
  time with type 3 on the cursor shots, and parsed the listener, actions, touch lines, journal and
  this launch's receipts while skipping an older session's. It also found two harness faults,
  now fixed: a capture without a display waited 10 s for a file, and an unsilenced /proc read.
  inject.sh itself ran inside a real bubblewrap, with Xvfb copies named Xwayland standing in for
  gamescope's (XTest stays local there: no gamescope, no EI). It found
  only its namespace's :12 and :13, ignoring two "owner" displays outside that carried the same
  clock. It typed 5 keys and 2 clicks into :13 alone, put the pointer at exactly the computed
  640,560, 665,661 (from the switch receipt written meanwhile) and 1151,699, and fired each token
  within 85 ms of its time under Rosetta. It refused outside a bubblewrap, beside an X0, and with one
  display.
  The Deck run, still to come: build one release with every radial commit (`build-release.sh
  --delta`), deploy it once, and run `semu-deck-cli steam input` in the Steam-stopped window. Then run
  `input-check.sh PAD tests/deck/radial-check.cases OUT` with tests/deck copied over and PAD the
  x86_64 virtual_pad. It must show:
  - result lines `inject: typing into :N`, `x-listener: :M` and each chord's `action: <id> keyboard
    x1`, which proves XTest from another Xwayland goes through gamescope's EI to the game's display
    and the XI2 raw listener, once;
  - journal records 79/80/77/78/9 as each case lists, and switch receipts naming the new art and
    preset;
  - choices in OUT/home/semu.json, and gba's second launch starting on them;
  - `ui.menu gamepad` for Select plus the top face button;
  - `semu-retroarch: touch ... -> surface 1 native` near 128,96 on the DS cores and core x near
    0.14/0.50/0.86 on the Azahar core, plus `semu-vulkan: touch ... -> 1` on standalone Azahar, the
    same after a layout switch;
  - toasts and bezels in the shots, Semu's cursor in cursor-n and none in idle-n;
  - Ctrl+H not resetting psx, and nothing journaled on Cemu.
  Only the owner can check, once in Game Mode: Steam loading the profile through the configset
  (controller_ui.txt naming config/semu/controller_neptune.vdf instead of Last Resort); the ring's
  layout, icon-only drawing, click-and-release firing, the empty centre and the haptics; Steam's
  own XTest process and key timing; the trackpad's sensitivity, soft-press threshold, drag and the
  cursor's look under gamescope; gamepad chords that Semu types through uinput into standalone
  emulators (melonDS Shift+F1, Azahar's pause), with their echo suppression, which headless gamescope
  cannot see (no libinput); the Deck touchscreen, Dolphin's Wii remotes and the lower-grip Wii layer;
  both Steam Controllers; and how a shell-bezel decode stall feels.
  2026-10-04, the modal menu (follow-up F1; offline, Deck untouched; ruling above). Built: the open
  menu grabs the pads the supervisor reads once all of them are at rest and releases them after the
  closing press (`src/launch/menu_modal.btrc`, run after every tick's drain); pads now track their
  sticks and triggers, and are seeded from EVIOCGKEY and EVIOCGABS when opened. Observed in the
  podman VM with `tests/integration/menu-modal.sh` (rootful stage, /dev/input bound in): `semu
  launch azahar` ran a stand-in Azahar, sdl2-jstest printing every SDL joystick event with a
  timestamp, while the replica of Steam's virtual pad (`virtual_pad --steam-virtual-pad`, SDL GUID
  030079f6de280000ff11000001000000, "Microsoft X-Box 360 pad 0") played A; A held through
  Select+Y, then released; d-pad down, down, up, up, R1, L1, X, L3 and R2 in the menu; B; then A and
  d-pad up; Start+Select. The supervisor logged ui.menu, "the menu holds 1 pad(s)" in the same
  millisecond as A's late release (1.1 s after Select came up), two downs and two ups, ui.menu.back, then
  "gave 1 pad(s) back" 78-89 ms after B (two runs); SDL saw exactly A down and up, A, Select, Y down, Y and Select
  up, A's release, then nothing for 4.2 s (both runs), then A and d-pad up, and Select of the quit chord (Start
  was cut off by the quit). The previous semu (4707ffb, the same script) let SDL see the whole menu:
  the four d-pad moves, R1, L1, X, L3, R2's axis and B. `make test` carries the state machine, the
  rest rules and the Select+Y sequence through real pollDevices ticks on socketpair pads (one
  standing in for Steam's pad, one plugged in mid-menu), and fails under each of 7 mutations: no
  grab, no wait before grabbing, no wait for the closing release, the EVIOCGRAB ioctl dropped, the
  tick never updating the hold, sticks and triggers untracked, and the emulator's exit not releasing.
  Still for the Deck: `radial-check.cases` case 5 (Ctrl+M over Azahar's OoT 3D title, d-pad and A in
  the menu) must now leave the title where it was, with `menu-holds: 1 gave-back: 1` in its result;
  and in Game Mode the same on Steam's real virtual pad. macOS stays non-modal (no grab in
  GameController); F2 (Azahar presenting while paused) stays open (ruling above).
  2026-10-04, review follow-ups on input (offline, Deck untouched). A physical pad and Steam's copy
  of it run each chord once, and Steam's ignore list keeps the physical one closed (ruling above).
  `tests/integration/input-x11.sh` now also plays a positional pad and the Steam-pad replica at once,
  and expects Ctrl+H's shader select of none (journal code 80 with the variants file's none index,
  D5) instead of the old shader toggle. Run in the podman VM on this tree: every check passed but the
  harness's own first guess at the duplicate count; the pair opened and closed the menu once
  (`menu_gamepad=3`, `back_gamepad=3`, one "already ran from another input source", journal
  `1:0 5:0 77:1 80:2 1:0 5:0 1:0 5:0 1:0 5:0`, `shader_none=2`), RetroArch PLAYING after Ctrl+K, no
  core reset on Ctrl+H, Start+Select quit. The pair's B finds the menu already closed, so only
  Select+north is dropped; the expectation now says one. New contracts
  (`tests/contracts/spec/input_devices.btrc`, `menu_modal.btrc`) fail when the X adapter stops
  connecting in begin or on rescan, stops selecting or decoding raw keys, connects without DISPLAY,
  when adopt stops swapping Steam's pad or tagging keyboards, classify drops EVIOCGID's vendor, the
  menu grabs a keyboard, the echo is no longer armed by typing or is consumed from evdev, keyboard
  Left/Right stop stepping the menu's rows, or two pads share one identity.
  2026-10-04, review follow-ups on the renderer and the RetroArch bridge (offline). The GL-free core
  of the live switch moved into `src/renderer/renderer_live_switch.btrc` (the due check, the section
  load, reloadVariants, the image-signature staleness, the live choice and its re-apply after a
  context reset, and the image-reload predicate); the post phase draws what `RendererToast.mode`
  says; the bridge's surface walk (`SemuRetroArchCoreAxis.column`/`row`, the composite size, the
  surface structs) and its cursor fill (`SemuRetroArchCursorPolicy.fill`) moved into the
  header-free `surface_math.btrc`. Contracts run them against the launch's own nds variants file and a
  400/320 3DS contract and fail under each of 15 mutations: reloadVariants, the stale mark, the live
  update or the re-apply dropped from the switch; the images-due predicate ignoring staleness; the
  renderer no longer calling the switch, the re-apply or `RendererToast.finish`; the toast never
  drawn over a closed menu; coreX's width arguments swapped or its surface index pinned to 0; the
  cursor's x zeroed in the fill or after it in the bridge; and the image reload bypassing the
  predicate. `nix build .#semu-renderer` and `.#retroarch` (macOS) build with the moved code.
  Two more contracts close review gaps in the choices: On/Off after Next Bezel returns to the bezel Next
  chose (it returned to the launch's), and a stale per-system id resolves to the manifest default in
  `bezelVariantId`/`shaderVariantId` and the launch draws that default (both caught by a mutation).
  2026-10-04, emulator keyboards, Dolphin's Wii controls and the keypad + (offline; rulings above).
  `tests/contracts/spec/emulator_keyboard.btrc` compiles both Linux targets and fails when a PCSX2 pad
  alternate shares a Steam chord's last key, when a Dolphin game control presses such a key without
  !Ctrl, uses a bare number, or names a control its device lacks (vendored SDL, SteamDeck and XInput2
  names at 2606a), when any emulator reads a grip Steam's Deck template holds for a preset, and when
  PCSX2, PPSSPP or melonDS bind main-row + for Fast Forward; Azahar's eight colliding defaults are
  pinned row by row (blanked == 8). 11 mutations each fail it: Square = Keyboard/A restored, the
  compiler guard off, Wiimote B on R4 again, an SDL name on the SteamDeck device, Dolphin's Enter
  unmapped, its !Ctrl guard dropped, a bare digit, main-row + for each of the three emulators, and
  Azahar's Audio Mute key changed. Not observed in a running Wii game yet: the Deck run should check
  that Wiimote buttons follow Steam's pad, 1 and 2 are no longer held, and the menu holds them.
  2026-10-04, the installer and the style review. `install.sh` prints both Steam steps after a full
  and a delta install (`steam shortcuts && steam input`): without `steam input` Steam keeps its
  fallback layout, so neither the radial nor the right-trackpad pointer exists. The style scan
  (`tests/contracts/spec/harness.btrc`) now covers every source and spec the radial series wrote or
  grew, reads "Type name," and "Type name)" parameters and the C integer types, and also refuses the
  abbreviations the reviews named (cfg and the like). It found and the series renamed: `id` in
  render_variants and rendering (variantId, packageId), `x`/`y` in renderer_menu_raster (left, top),
  `gl` in renderer_variant_images (graphics), `gb`, `at`, `ds` and `cfg` in the specs; the bridge's
  int16_t `x`/`y` went with the cursor fill. renderer_config.btrc (476 lines) is split: the screen and
  layer keys and the lookup that a live switch scopes are `RendererConfigKeys` in
  `renderer_config_screen.btrc`. Still long and left as they are: runtime_bridge.btrc (539 lines; a
  split means a RetroArch rebuild for no behaviour) and renderer_menu.btrc (353); older files outside
  the series (rendering, renderer_post_ui, surface_contract, runtime_bridge, plan) still bind short
  names and are outside the scan.
  The Mac render host, built from this tree (the split config and the live switch of the renderer
  commit), drew nds shell>main_right and gba>arctic: each switched frame equals a fresh render of the
  target below the toast band (ImageMagick AE 0), the toast frame shows BEZEL: ARCTIC over the closed
  menu, and a context reset after the switch reproduces the switched frame (AE 0).
  The podman VM on e399744 (the whole review series): `tests/integration/touch-x11.sh` PASS with RetroArch
  rebuilt around the moved bridge code (3DS taps at 5/50/95% reach the core at 0.1397/0.5000/0.8576,
  DS at 0.0498/0.5000/0.9484, 0.75 down, a bezel tap presses nothing, the cursor shows after motion
  and is gone 4.5 s later); `tests/integration/input-x11.sh` PASS (the pair's duplicate Select+north
  dropped once, journal as above); `tests/integration/live-switch.sh` on RetroArch gba (240p Test
  Suite) and nds (DLDI benchmark) logged both chords, journalled `79 1; 80 1`, wrote two switch
  receipts each and saved `arctic`/`agb001` and `main_right`/`grid`; the nds captures, judged by eye,
  show the toast over the shell, then the large main right layout with the LCD grid, and the menu
  naming both. `nix build .#semu-renderer` (macOS) builds the split config.
  2026-10-04, the settings radial (offline, Deck untouched; M13 item 3 part 3): the quick ring's Settings
  slot switches Steam to a settings page whose left-pad radial holds every radial-v2 choice (Next Bezel,
  Bezel On/Off, Next Shader, Shader On/Off, Fit, Aspect, Controller Layout, Players, Reset) with Close in
  the centre, on the Deck, the new Steam Controller and the 2015 one; `radial-check.cases` (16 cases)
  types every chord those radials send. Still for the Deck: `semu-deck-cli steam input` with Steam
  stopped, then the radials in Game Mode and cases 14 to 16.
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
- M12 standalone bezels (2026-09-25): built. Linux Vulkan layer and macOS MoltenVK stand-in share
  images with the OpenGL compositor. Observed with real games: Azahar (Pushmo, Linux VM, touch
  lands on the composed bottom screen), Azahar (Ace Combat, Mac), Dolphin (Aggressive Inline,
  Mac), Ryujinx (Animal Crossing, Mac). Ryujinx and Cemu load the layer on Linux but need native
  hardware for a picture; the Mac touch check and the Deck remain.
- M13 owner feedback from the Deck (2026-10-04): all six items built and observed off the Deck. On the
  Deck (release b1bf425): OoT 3D and MM3D load their packs and patches and TotK its mod from the central
  library after `semu mods migrate`; Semu's arrow shows on standalone Azahar after a tap; the radial v2
  chords reach the supervisor, Fit on the Wii places the picture in the bezel and its Aspect double press reboots the game. The series' review is fixed
  (Azahar's first move, the harness's keysyms and greps, Reset keeping play_mode, Fit with the bezel off,
  Dolphin's SDL names on the desktop, the missing contracts); cases 4, 14 and 16 pass on the Deck with
  release 5f55703. `semu mods migrate` moved the Mac's Drive library too (6 renames, the 5 Lime3DS
  duplicates and 2 empty folders left in place). Open, needing the owner in Game Mode: the Wii IR, the
  players page, Steam's radial pages and icons, item 1's glow and item 5's PSP by eye.
- M14 owner feedback, round 2 (2026-10-04): all nine items implemented (191bea1 to 15cc2ec), observed on
  the Mac render host and in the podman VM (Super Mario 64 on GLideN64, Flycast, the build flags in the VM).
  The owner played 15cc2ec in Game Mode, and what the owner still saw is M15. Open on the Deck: the reruns M14
  names (the Wii layouts and Restart Game, Wii U Bezel, the N64's speed, the Dreamcast flicker, GDV-NTSC's
  look and frame time).
- M15 owner feedback, round 3 (2026-10-05): all eight items implemented (db92462 to 0f535bb, then the
  review fixes), observed on the Mac render host, in the editor (headless Chrome) and in the podman VM
  (PCSX2, Beetle PSX, Mupen64Plus, Cemu with Mario Kart 8, the DS and 3DS touch routes). On the Deck
  (bdabd01, 2026-10-05) M15's "Deck acceptance" list, run off-screen by `tests/deck/m15-check.cases`, held
  13 of 13; open: Wii U sound by ear.
- M16 items 5 and 7 (Fit's four states, the Wii's sizes): built 2026-10-06 and observed on the Mac render host
  (every bezel system in the four states at 1280x800 and 1920x1080, judged by eye) and in the editor; contract
  fit_states.btrc. Fit cycles integer game, integer bezel, non-integer game and non-integer bezel; with no bezel
  it steps the two game states, so turning the bezel off never shrinks the picture (item 7's cause: the bezel-off
  path used a separate integer switch while the Wii's Fit: Screen filled fractionally). The TVs keep integer bezel
  as their default at every size (the review round's reversible decision: docked, the NES, the PS1's and N64's
  speakers sets and the PS2 draw a step under the retired fit, whose stand ran 2.6-16% past the edge, outside
  the owner's 1%). Open on the Deck: `tests/deck/m16-check.cases` (Fit cases) F1-F5 off-screen,
  `SEMU_CHECK_SIZE=1920x1080` `tests/deck/m16-docked.cases` K1-K4, then by eye in Game Mode the CRT look and a
  scrolling scene at the non-integer sizes.
- M16 item 2 (render scale): built 2026-10-06 (c83689f, 8a9a049) and observed on the Mac render host (frame and
  window producers at 1x, 2x and 3x, judged by eye) and in the podman VM (Super Mario 64 on GLideN64 and Def Jam
  on PCSX2 at 2x through Scale and Restart Game, The Wind Waker switched live on Dolphin); contract
  render_scale.btrc, 19 mutations killed. Scale (settings radial, menu SCALE row, SEMU SETTINGS) steps each
  system's declared scales, saved per system; Dolphin applies it live, the rest at Restart Game; the picture keeps
  its native placement. On the Deck (2026-10-06): release af0470d8 installed and `semu-deck-cli steam input` run
  with Steam stopped; the owner's controller_neptune.vdf carries the eleventh settings slot, Render Scale
  (Ctrl+Shift+N, semu-scale.png). Open on the Deck: `tests/deck/m16-check.cases` S1-S4 and `radial-check.cases`
  case 18 (pictures, receipts and full-speed rate lines at 2x) and the review round's S5-S7, then the GPU load
  per system, and the slot by eye in Game Mode.
- M16 items 3 and 10 (the GB studio bezel deleted; TV cutout: the lip stands in the room's night light with no
  lit rim, and the curved picture edge blends over one device pixel toward a mirror that no longer snaps on at
  pixel centres): done on the Mac render host, all 19 TV variants at 1280x800 and 1920x1080 in every Fit state,
  the editor matching the renderer (MAE 0 on every variant at 1280x800, every TV at 1920x1080); contracts
  tv_cutout.btrc and render_variants.btrc. Open on the Deck: `tests/deck/m16-check.cases` T1-T5 and G1 (the
  Deck did not answer ssh this session).
- M16 items 4, 8 and 9 (boot size, Dreamcast speed, shaders per system): built 2026-10-06 (1ba2de8, 6562dd9,
  11c5b12) and observed on the Mac render host and in the podman VM; contracts boot_resize.btrc, crt_gdv.btrc and
  crt_rule.btrc. Item 4's cause is Dolphin's late full-screen switch over an inner GL window sized once at start
  (patched); item 9's rule, Retro Crisis's GDV-NTSC on a CRT television console, holds for the 240-line consoles
  and the NES only (corrected in the review round: the 480-line four keep Sharp CRT, `gdv` one press away, so item
  9 stays open until measured; measured on the Deck and moved, the item 9 line below), LCD on handhelds, none on HD consoles, with CRT scanlines on whole output rows at
  every size; item 8's cause is Flycast's swap interval on the OLED's 90 Hz screen (next line). Item 4 observed
  on the Deck (2026-10-06, `tests/deck/boot-capture.cases` before and after the releases): the owner's shrink filmed on the M15 release in
  both Dolphin cases and gone from af0470d8 on; the other emulators never shrank; PCSX2's game list, Cemu's
  menu-bar window and a frame of Ryujinx's, also shown at boot, are gone too (9d52f6f: -nogui and Cemu's
  full-screen-at-boot patch; a70ece76: Ryujinx's). Open on the Deck:
  `tests/deck/m16-check.cases` (deck cases D1-D6) and the review's D7-D10 (GDV-NTSC on the 480-line four), and
  Sonic Adventure four ways (`tests/deck/system-matrix.cases`, PLAN M16 item 8's decision table).
- M16 item 8 (Dreamcast speed): done 2026-10-06, observed on the Deck off-screen at Game Mode's own refresh; the
  owner's Game Mode by eye is the owner's. Game Mode runs the Deck OLED's screen at 90 Hz; Flycast's swap interval
  (refresh / 60 times the game's, truncated) asked for 3 refreshes per 30-frame scene, the swap took 4 under
  gamescope, so Sonic Adventure's 30-frame scenes drew 22.5 per second and the game ran at 75% (full speed at
  60 Hz, where every earlier check ran). Fixed by `config/emulators/flycast/semu-swap-interval.patch` (the next
  refresh off a whole multiple of 60 Hz, Flycast's own interval at 60 and 120 Hz; 6c40e18); the Deck harnesses now
  run at Game Mode's refresh. On release 6c40e186 at 90 Hz: the 30-frame scenes at 30 per second, the 60-frame ones
  at 60, the HUD clock gaining 6.00-6.04 s every 6 s, on Sharp CRT and on the owner's CRT Royale; docked at 60 Hz
  unchanged. Not the composition, the GPU, the processor or the SD card (PLAN M16 item 8's record). Deck cases
  m16-check.cases D3 and D11, m16-docked.cases K5.
- M16 item 9 (shaders per system): done 2026-10-06, measured on the Deck off-screen (release 6c40e186, Game Mode's
  90 Hz at 1280x800 and docked at 1920x1080 and 60 Hz, render scale 1x, each shader in its own home on the same
  scenes): GDV-NTSC holds the frame rates Sharp CRT holds on the Dreamcast, PS2, GameCube and Wii in integer bezel
  and non-integer game, its composition 5.4-5.8 ms of GPU time at integer bezel, 7.3-7.8 in non-integer game and
  10.0-12.0 docked (Sharp CRT 1.7-1.8, 2.8-3.0 and 4.9-5.8) with the GPU clock never past 1040 of its 1600 MHz. So
  the four moved to it and one rule holds for every system: a console played on a CRT television defaults to Retro
  Crisis's GDV-NTSC with his Steam Deck values for its cleanest signal, a handheld to an LCD look, an HD console to
  none; Sharp CRT one press of Next Shader away. The review round's 4 ms GPU-time cap is retired for the frame rate
  with the GPU never the limit (PLAN M16 item 9's record, reversible). The owner's saved CRT Royale on the Dreamcast
  and Wii stays. Deck cases m16-check.cases D7-D10 and D12-D15.
- M16 items 1 and 6 (crosshair, the Wii's controller): built 2026-10-06 (51bcddb, 47c1303) and observed on the Mac
  render host and in the podman VM (RetroArch's DS and 3DS cores, standalone Azahar, Dolphin with Mario Kart Wii);
  contracts right_trackpad, standalone_cursor, controller_layouts, players, emulator_runtime, radial_render. DS and
  3DS show Semu's crosshair, the Wii the game's own pointer, Switch, Wii U and standalone melonDS their emulator's
  arrow (the review round's reversible ruling); every Wii Remote layout, GameCube included, switches live through
  Dolphin's own keys, which the review round moved off desktop shortcuts. Open on the Deck:
  `tests/deck/m16-check.cases` C1-C5 and radial-check.cases 3, 4 and 16.
- M16 review round (2026-10-06): 30 findings checked; fixed in ca99076 and the commits after it (integer Fit never
  below 1x while the picture fits, the render scale's wiring contracted, pictures fitted before they are rounded,
  a pre-M16 Fit: Screen read as it drew, whole toasts and a Fit label, Flycast's keys where Flycast reads them,
  Dolphin's typed keys off desktop shortcuts, the gallery measuring the placed canvas in the four states, the
  reflection audit for the four states, the Deck cases and a docked harness); answered in PLAN M16's review block
  (docked TV sizes, other pointers, the docked render-scale list). Item 8 has no Deck observation (item 4 had none
  either until the Deck films after this round, see the items 4, 8 and 9 line) and item 9 is open for the 480-line
  four. Observed on the Mac render host (the rounding swept over 424 cells at both
  sizes before and after, the editor matching in the four states at both sizes, the gallery's 272 flat-card
  checks and the reflection audit's 308 rows in the four states passing); open on the Deck: everything in the
  block's list, `tests/deck/m16-docked.cases` with SEMU_CHECK_SIZE=1920x1080 among it.
- M16 release for the Deck run: none installed by the review round (read-only on the Deck; the Deck chain builds
  and installs it); build it from origin/main at or after the commit that adds this line, which carries all ten
  items, the review's fixes and every case file named above, then run `semu-deck-cli steam input` with Steam
  stopped. Installed on the Deck 2026-10-06: af0470d8 (then `steam input` with Steam stopped), 9d52f6f and
  a70ece76, the last after that commit; Steam's templates have not changed since af0470d8, so the radial is
  current. Then 6c40e186 (item 8's Flycast patch), installed with `install-delta` and `prepare`, no game running.
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
  within 2 px at Deck and 4K (true then; after M16's four Fit states its expected() re-derived the retired fit and
  failed 34 cells until the M16 review made it read the renderer's placed canvas); hot reload proven; no Python in the tree. Corrections to older
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
