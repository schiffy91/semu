# Semu PRD

## Goal

Semu should make a Steam Deck behave like a clean console-style emulation
appliance:

- Launch ES-DE from Steam/Game Mode.
- Route each supported system to the right emulator.
- Keep ROMs, BIOS, saves, states, screenshots, scraped media, VM disks, and
  generated package output outside source-controlled roots.
- Compile source policy into emulator profiles, ES-DE config, Steam Input
  templates, sync units, package assets, and runtime settings.
- Apply Semu-owned shaders and bezels to game content only, never emulator UI.
- Provide controller-reachable Semu Settings entries in ES-DE.
- Quit any emulator immediately and return to ES-DE.

## Non-Goals

- Store copyrighted content, BIOS, keys, saves, VM disks, screenshots, or Deck
  captures in Git.
- Let each emulator patch become its own renderer.
- Use ES-DE's root directory as Semu's source or generated-output tree.
- Treat static config as production readiness proof.
- Add top-level directories for narrow implementation details.

## Source Layout

Source inputs:

- BTRC code in `src/semu/**`.
- Editable policy under `config/**`.
- Package source under `packaging/**`.
- Test entrypoints under `tests/**`.

Generated outputs:

- `generated/build/` for local BTRC/C build products.
- `generated/semu.c` and `generated/semu.json` as temporary committed
  snapshots for current Nix/package compatibility.
- `generated/packaging/` for rendered ES-DE XML, emulator profiles, Steam Input
  VDFs, sync scripts, AppImage output, and generated packaging material.
- `generated/runtime/` for local content, launcher state, Syncthing state, logs,
  and fallback ROM tree.
- `generated/test/` for verification artifacts, screenshots, VM disks, and e2e
  state.
- `generated/nix/result` for Nix out-links.

External install targets are still allowed when explicit, for example the
user's ES-DE home or SD card. Repo-local generation is constrained to
`generated/`.

## Rendering Contract

The elegant end state is a small tap ABI plus one renderer:

- Tap-in: the emulator reports game-frame metadata at the game-content boundary.
- Tap-out: Semu's compositor uses that metadata to apply shader and bezel
  policy.
- Proof: runtime output must prove the tap excludes menu/settings UI and uses
  generated Semu config.

The tap payload should contain active/content-loaded flag, content rectangle,
output size, native content size, backend/API origin, and orientation/surface
role where needed. It should not contain shader code, bezel layout policy, asset
parsing, or emulator-specific duplicate compositor logic.

## Product Requirements

1. Bootstrap
   - `generated/build/semu bootstrap --project "$PWD"` creates
     `generated/runtime/**` and `generated/packaging/**`.
   - It must not create root `build/`, `.semu/`, `ES-DE/`, or `result*`.

2. Build
   - `make btrc-build` compiles the BTRC CLI to `generated/build/semu`.
   - `make manifest` refreshes `generated/semu.json`.
   - Nix builds use `--out-link generated/nix/result`.
   - Bootstrap-generated emulator profiles must not dirty source directories.

3. ES-DE
   - Generated ES-DE custom systems and settings entries live under
     `generated/packaging/es-de/**`.
   - Runtime install may still write to `~/ES-DE/custom_systems` and
     `~/ES-DE/settings`.

4. Runtime Data
   - Default local content root is `generated/runtime/content`.
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
  directly. Future cleanup should make Nix run the BTRC compiler instead.
- `generated/semu.json` is still committed as a manifest snapshot. Future
  cleanup should remove it once package/build consumers compile it directly.
- Old production-refactor branches should be mined only in small pieces after
  current `main` has explicit contracts for each piece.
