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
    still restarts (the page saw a change; every file then comes from the saved choice). Fit stays a
    two-way integer toggle (bezel and screen, the owner's "always integer scale"), so on TVs the default
    fractional fit (686x515 on the Wii at 1280x800) is reached only by Reset, which also returns the
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
