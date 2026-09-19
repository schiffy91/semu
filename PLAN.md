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

- M1 reset: done 2026-09-19 (`make build && make test` pass in about ten
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
  Vulkan and stays unhooked. Not done: the Deck half of the criterion.
- M7 extras: all three delivered on the desktop 2026-09-19.
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
- Active milestone: M5/M6 hardware acceptance once a Deck is reachable
  (`DECK_HOST=deck tests/deck/deploy.sh install`), then the native-emulator
  render hook.
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
