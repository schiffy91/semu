# Semu TODO

## Steam Deck Production Gate

- [x] Keep source flattened around `src/{compiler,generators,lib}`.
- [x] Keep deleted nested runtime trees out of the architecture.
- [x] Build the BTRC CLI with strict imports on macOS and Steam Deck.
- [x] Rebuild the Deck AppImage enough to verify settings entries no longer
  fail through AppImage FUSE/extract fallback.
- [x] Prove the process-scoped compositor route is too fragile for production
  on the Deck and pivot to emulator source render hooks.
- [x] Move per-system compositor effect selection into
  `config/assets/systems/*.json`.
- [x] Document the rendering architecture as Semu-owned declarations ->
  content viewport resolver -> emulator source hook -> generated hook/native
  configs.
- [x] Implement the first content-layer render proof: RetroArch GB launches on
  the Deck with Semu-generated Slang config and visible DMG LCD tint/grid.
- [x] Add first bezel composition proof without shading emulator UI/settings:
  RetroArch GB compiles `semu-gb-handheld.slang` and screenshot shows a
  prototype Game Boy shell around the LCD game viewport.
- [ ] Replace the prototype GB shell with production-quality artwork and carry
  the same asset model to GBC/GBA/CRT/DS/3DS/PSP systems.
- [x] Implement first-stage source render proof hooks for emulator routes and
  rebuild them through Nix: RetroArch, Dolphin, PPSSPP, Flycast, Azahar,
  melonDS, PCSX2, Cemu, and Ryujinx. These prove the game-framebuffer hook
  boundary; final shader/bezel composition still has to run through the same
  hook contract.
- [x] Finish the current Steam Deck full bundle rebuild with source-hook proof
  packages and the ES-DE settings utility patch.
- [ ] Smoke the installed AppImage: `doctor`, `apprun prepare`, generated
  settings entry, stale RetroArch core rewrite, and bundled rendering assets.
- [x] Verify the active Deck 3DS ROM set is updated enough to boot: Deck and
  Mac both list 172 ROM-like files, sampled active Deck hashes match Mac, and
  Super Mario 3D Land boots in Azahar.
- [ ] Keep Steam Deck SD-card ROM/BIOS content read-only for remaining work; do
  not delete, rewrite, resync, move, or otherwise mutate SD-card content.
- [x] Launch a real ROM for every Steam Deck system route and capture
  screenshots proving launch, fullscreen window/process presence, input-induced
  visual change, content load, and Select+Start quit evidence.
- [ ] Add screenshot proof of production shader/bezel visibility for every
  non-modern system. The Deck harness now reports proof-only hooks as
  `render=proof-only`; `render=ok` requires a promoted
  `implemented_source_hook`.
- [x] Re-test N64 orientation on Deck after removing compositor shader path:
  Mupen64Plus Next screenshot is upright.
- [x] Fix DS route ordering: melonDS is primary on Linux/Steam Deck and
  RetroArch DeSmuME is fallback.
- [x] Smoke DS melonDS launch on Deck: Tetris DS boots fullscreen.
- [x] Add melonDS first-stage source-hook proof and package assertions.
- [ ] Add melonDS shader/bezel composition and screenshot proof.
- [x] Fix generated Semu Settings entry fallback: it finds the source-built CLI
  via `SEMU_ASSET_ROOT`, ignores `SEMU_BIN` in the settings launcher so it does
  not route through AppImage/FUSE, and passes SSH/no-terminal verification.
- [x] Replace the generated pseudo-system settings integration with an actual
  ES-DE Start/Main Menu/Utilities Semu Settings entry.
- [x] Clean the live Deck project of old settings pseudo-system XML/find-rules
  and generated `.semu` settings game entries.
- [x] Verify basic input and unified Select+Start quit path for every emulator
  route in the Deck loop.
- [ ] Verify save/load actions where each emulator supports them.
- [ ] Verify Game Mode Steam Input behavior for the left-trackpad radial quit
  action after Desktop Mode evidence is clean.

## Compiler Architecture

- [x] Keep `docs/semu-compiler-refactor.md` as the source plan for the target
  build architecture.
- [x] Keep definitions under `config/targets/`, `config/systems/`,
  `config/emulators/`, `config/input/`, `config/assets/`, and
  `config/settings/`.
- [x] Treat Semu settings and sync defaults as owned config under
  `config/settings/`; project user edits go through `semu.json` and
  `overrides/**`.
- [x] Keep root `Makefile` as a thin wrapper around BTRC/Nix commands.
- [x] Keep `settings` and `assets` commands editing only Semu-owned files.
- [x] Keep emulator-native config as generated output, not user-edited source.
- [ ] Remove any remaining unused generated manifests or compatibility command
  references after Deck verification proves they are dead.
- [ ] Add focused golden tests for generated ES-DE systems/find-rules,
  settings entries, stable launchers, and per-system render environment.
- [ ] Add focused verification for content viewport resolution and renderer
  exclusion of ES-DE/settings/emulator control surfaces.
- [ ] Keep README and docs aligned with the active `src/`, `config/`, `build/`,
  and `tests/` layout.

## Deferred Until Deck Is Complete

- [ ] Do not start Bazzite parity until every Steam Deck route is implemented
  and verified with screenshots/loop evidence.
- [ ] Bazzite parity harness.
- [ ] Bazzite VM performance cleanup.
- [ ] Bazzite/desktop-only test expansion.
