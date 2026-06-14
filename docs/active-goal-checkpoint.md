# Semu Active Goal Checkpoint

This checkpoint keeps the current production contract short enough to survive
context compression without preserving stale implementation history as
guidance.

## Current Source/Data Split

- Source repo: `/Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu`
- Local project/data root: `/Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/media/Games/Emulation`
- Steam Deck project root: `/home/deck/.local/share/semu`
- Steam Deck ROM/media root is external and must not be mutated:
  `/run/media/deck/SD` or `/mnt/SD`
- Canonical architecture doc:
  `/Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/docs/semu-compiler-refactor.md`

## Current Verified Progress

- Strict-import BTRC builds pass on macOS and Steam Deck.
- `nix build --impure .#default` passes on the Steam Deck.
- `semu build configs --target steam-deck --project /home/deck/.local/share/semu`
  passes on the Steam Deck and emits per-emulator hook JSON.
- Latest full Deck bundle rebuild passed on 2026-06-13 after the ES-DE
  settings utility patch was shell-quoted for paths with spaces.
- Latest Deck `verify`, `build configs`, and `tests/esde-settings-entry.sh`
  passed against `/home/deck/.local/share/semu`.
- Full physical Deck emulator loop passed mechanically on 2026-06-13:
  `/home/deck/.cache/semu-emulator-loop-final-20260613-203609`.
  Every configured route launched a real ROM, captured a changed screenshot,
  responded to input, loaded content, and recorded `quit reason=select+start`.
- After that run, the Deck loop was tightened so proof-only hooks report
  `render=proof-only`; a green loop now requires `implemented_source_hook` for
  systems with enabled shader/bezel assets.
- `tests/deck/quit-watch-smoke.sh` passed on Deck inside `nix develop`; the
  synthetic Select+Start input killed the launched process group.
- The generated `semu-settings` launcher returns status 0 and no longer falls
  back through AppImage/FUSE. ES-DE Utilities menu integration is built through
  the patched ES-DE package and checked by `tests/esde-settings-entry.sh`.
- RetroArch GB launches a real ROM and has content-layer shader plus first
  bezel proof from Semu-owned config.
- N64 orientation was re-tested on Deck through RetroArch/Mupen64Plus Next; the
  latest loop screenshot is upright after removing the compositor shader route.
- DS route order is corrected: melonDS is primary on Linux/Steam Deck and
  RetroArch DeSmuME is fallback. melonDS boots Tetris DS fullscreen on Deck.
- 3DS active Deck ROMs were checked against the Mac ROM set by count and sample
  hashes, and Super Mario 3D Land boots in Azahar.
- Production-quality bezel artwork and final shader/bezel composition are not
  complete. First-stage source-hook proofs are wired for RetroArch, Dolphin,
  PCSX2, PPSSPP, Flycast, Azahar, melonDS, Cemu, and Ryujinx; these are
  hook-boundary proofs, not proof of final shader/bezel composition.

## Current Test Harness Status

- Fast host checks are compiler-first. `make test` runs payload audit,
  source-hook metadata, rendering contract, compiler verification, generated
  ES-DE settings entry, Cemu/Ryujinx input config, and RetroArch N64 config
  checks.
- The retained E2E specs under `tests/e2e/specs/` describe compiler and Bazzite
  parity states only. They are not the production runtime path while Deck
  verification is active.
- Deck verification remains the required full-system path. Bazzite VM and
  installed-SSH harnesses are retained as parity infrastructure, but they run
  only after the physical Deck path is complete and verified.

## Current Non-Negotiable Direction

1. Do not restore deleted nested runtime trees.
2. Delete legacy compatibility code instead of renaming it.
3. The build must be compiler/generator driven:
   `src/main.btrc`, `src/compiler/`, `src/generators/`, `src/lib/`.
4. Declarative definitions are the source of truth:
   `config/targets`, `config/systems`, `config/emulators`, `config/input`,
   `config/assets`, `config/settings`.
5. Generated files belong under project/generated locations or packaging
   output. Do not keep generated `.semu`, emulator profile, ES-DE XML, or
   native emulator config files as hand-maintained source.
6. Settings and sync policy are Semu-owned config under `config/settings/`;
   project-specific user edits go through `$SEMU_PROJECT/semu.json` and
   `$SEMU_PROJECT/overrides/**`.
7. AppImage/launcher/settings behavior must be generated from declarative
   config. Avoid one-off wrappers unless they are generated output.
8. ES-DE Semu Settings must be an actual Start/Main Menu/Utilities settings
   entry, not a pseudo-system, fake ROM folder, or game entry.
9. Rendering must be one Semu model compiled to target mechanisms. Production
   rendering uses emulator source hooks at the game framebuffer/final present
   layer. External compositor/window injection is fallback/prototype only and
   must not apply to ES-DE, Semu settings, or emulator settings UI.
10. Steam Deck SD ROM/BIOS content is external and read-only for this work. Do
    not delete, rewrite, resync, move, or otherwise mutate SD-card content.
11. Verification must include real Deck screenshots/loop evidence for each
    emulator route. Bazzite parity harness work comes only after the Deck path
    is complete and screenshot/loop verified.

## Current Immediate Plan

1. Keep `src/main.btrc` compiling against the flattened compiler tree.
2. Keep remaining shared primitives in `src/lib` or `src/generators` with
   strict direct imports.
3. Generate ES-DE systems/find rules/settings integration, AppImage files,
   Steam Input templates, per-emulator config outputs, and rendering hook
   config from compiler definitions.
4. Keep Semu Settings as a generated ES-DE Start/Main Menu/Utilities entry.
5. Complete source-hook global visual integration with content viewport
   resolution and screenshot proof for every required emulator route.
6. Verify 3DS ROM availability without mutating SD-card content or touching
   saves.
7. Rebuild/install AppImage only after the compiler-only build is green.
8. Run local tests and Deck per-emulator screenshot evidence.
9. Do Bazzite parity only after every Steam Deck item is implemented and
   screenshot/loop verified.

## Latest User Direction

- Keep this refactor production-clean: no legacy runtime tree, strict BTRC
  imports, no dead test/docs references, and a coherent simple tree.
- Current ownership for this cleanup pass is docs and non-Deck test
  documentation only, plus non-executable stale metadata references to removed
  runtime-era E2E infrastructure.
- Do not touch the active Steam Deck critical path files or work directly on
  Bazzite/Steam Deck.
