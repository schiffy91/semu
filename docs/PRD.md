# Semu PRD

## Goal

Semu should make a Steam Deck behave like a clean, console-style emulation
appliance:

- Launch ES-DE from Steam/Game Mode.
- Route every supported system to the right emulator.
- Keep ROMs, BIOS, saves, states, screenshots, and scraped media outside Git.
- Compile source policy into emulator profiles, ES-DE config, Steam Input
  templates, sync units, and package assets.
- Apply Semu-owned shaders and bezels to game content only, never emulator UI.
- Provide a controller-reachable Semu Settings entry in ES-DE.
- Quit any emulator immediately and return to ES-DE.

## Non-Goals

- Do not store copyrighted content, BIOS, keys, saves, VM disks, screenshots, or
  generated Deck captures in the repository.
- Do not make each emulator patch a renderer.
- Do not use ES-DE's root directory as Semu's source or generated-output tree.
- Do not claim production readiness from static config alone.

## Architecture

Semu is a compiler plus runtime launcher.

Source inputs:

- BTRC code in `src/semu/**`.
- Editable settings under `settings/**`, `sync/sync.json`,
  `input/keymaps/**`, and `verification/screenshots.json`.
- Packaging and native helper source under `packaging/**`.

Generated outputs:

- `generated/semu.c` and `semu.json` are committed build snapshots for current
  Nix/package compatibility.
- Project-local generated ES-DE files go under `build/packaging/es-de/**`.
- Project-local generated emulator profiles go under
  `build/packaging/emulators/profiles/**`.
- Local fallback content goes under `.semu/content/**`.
- Runtime ES-DE install files may be written to the user's ES-DE home during
  install, but that is an external install target, not repository source.

## Rendering Contract

The elegant end state is a small tap ABI plus one renderer:

- Tap-in: the emulator reports game-frame metadata at the game-content boundary.
- Tap-out: Semu's compositor uses that metadata to apply shader and bezel policy.
- Proof: runtime output must prove the tap excludes menu/settings UI and uses the
  generated Semu config.

The tap payload should contain:

- active/content-loaded flag
- content rectangle
- output size
- native content size
- backend/API origin
- orientation/surface role where needed

The tap payload should not contain:

- shader code
- bezel layout policy
- asset parsing
- emulator-specific duplicate compositor logic

If an emulator integration needs hundreds or thousands of lines to draw shaders
and bezels, the code belongs in the shared compositor or asset compiler instead.

## Product Requirements

1. Bootstrap
   - `build/semu bootstrap --project "$PWD"` creates `.semu/content/**` and
     `build/packaging/es-de/**`.
   - It must not create a top-level `ES-DE/` tree.

2. Build
   - `make btrc-build` compiles the BTRC CLI.
   - `make manifest` refreshes `semu.json`.
   - `make test` passes on the host without ROMs, BIOS, or Deck hardware.
   - Bootstrap-generated emulator profiles must not dirty root source
     directories.

3. ES-DE
   - Generated ES-DE custom systems and settings entries live in
     `build/packaging/es-de/**`.
   - External runtime install may still write to `~/ES-DE/custom_systems` and
     `~/ES-DE/settings`.

4. Runtime Data
   - Default local content root is `.semu/content`.
   - External SD-card layouts remain supported.
   - Semu must not mutate external ROM roots except through explicit sync or
     settings operations.

5. Verification
   - Host tests prove source/generator contracts.
   - AppImage smoke proves package assembly with fakes.
   - Deck/VM/runtime tests prove launch, input, visual output, quit, and return
     to ES-DE.
   - Visual production claims require screenshots or runtime proof from current
     artifacts.

## Current Risks

- `generated/semu.c` is still committed because Nix packages compile it
  directly. A future cleanup should make Nix run the BTRC compiler instead.
- The old production-refactor branch should be mined only in small pieces after
  current `main` contracts exist for each piece.
