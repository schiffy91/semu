# Semu Compiler Refactor

Semu is moving to a compiler model: build a target, not a pile of hand-coded
emulator branches. The compiler reads declarative definitions for targets,
systems, emulators, input, and rendering; applies user overrides; resolves a
build plan; then generates ES-DE glue, emulator configs, launcher renderer
handoff, package outputs, AppImage contents, and verification plans.

There is no legacy nested runtime tree. The source tree is compiler code,
generator code, and small shared libraries only.

## Implementation Status

Done:

- Declarative definitions under `config/` exist in the repo.
- The root Makefile exposes thin `steam-deck`, `emulator`, `configs`, and
  `verify` targets shaped around the compiler CLI.
- The top-level `semu build ...`, `semu verify ...`, `semu settings ...`, and
  `semu assets ...` dispatchers compile under strict BTRC imports.
- The source tree is flattened around `src/main.btrc`, `src/cli.btrc`,
  `src/compiler`, `src/generators`, and `src/lib`.
- Generated ES-DE settings entries, launcher inventory, Steam Input selection,
  and AppImage manifest are emitted from declarative config.
- Rendering declarations live in Semu-owned config and are compiled into
  emulator game-frame render hooks. External compositor wrappers are fallback
  experiments, not the production route.

Pending on the Steam Deck:

- Verify the rebuilt AppImage launches ES-DE and settings entries without the
  FUSE/fusermount failure.
- Verify every emulator route with a real ROM, fullscreen screenshot, input
  proof, Start+Select quit evidence, and generic shader/bezel renderer evidence.

## Target Layout

```text
src/
  main.btrc
  cli.btrc

  compiler/
    lexer.btrc
    parser.btrc
    model.btrc
    resolver.btrc
    checker.btrc
    generator.btrc

  generators/
    esde.btrc
    steam_input.btrc
    appimage.btrc
    emulators/
      retroarch.btrc
      dolphin.btrc
      ppsspp.btrc
      flycast.btrc
      azahar.btrc
      melonds.btrc
      pcsx2.btrc
      cemu.btrc
      ryujinx.btrc
      ares.btrc

  lib/
    owned_paths.btrc
    template.btrc
    merge.btrc

config/
  targets/
  systems/
  emulators/
  input/
  assets/
  settings/
    semu-settings.json
    sync.json
  verification/
  esde/
build/
  packaging/
tests/
```

`config/assets/` stores visual asset declarations directly; there is no
separate `rendering/` definition tree.

## Emulator Definitions

Every emulator folder has the same shape:

```text
config/emulators/<name>/
  emulator.json
  package.nix
  paths.json
  launch.json
  input.json
  rendering.json
  templates/
```

The active emulator set is:

- `retroarch`: GB, GBC, GBA, NES, SNES, Genesis, N64, NDS fallback, PSX, and macOS Dreamcast/PSP fallback through cores.
- `dolphin`: GameCube and Wii.
- `ppsspp`: PSP on Linux and Steam Deck.
- `flycast`: Dreamcast on Linux and Steam Deck.
- `azahar`: Nintendo 3DS.
- `melonds`: Nintendo DS primary on Linux and Steam Deck.
- `pcsx2`: PlayStation 2.
- `cemu`: Wii U.
- `ryujinx`: Nintendo Switch.
- `ares`: macOS Nintendo 64.

`esde`, `steam_input`, and `appimage` are generators, not emulators.

ES-DE Semu Settings integration is also generator output. It must produce a
real Start/Main Menu/Utilities settings entry, not a pseudo game system or fake
ROM collection.

## Compiler Flow

`make steam-deck` runs:

```sh
semu build target steam-deck
```

Compiler stages:

1. `cli.btrc` parses `build`, `config`, `verify`, and `run`.
2. `compiler/parser.btrc` loads JSON definitions with the BTRC stdlib JSON APIs.
3. `compiler/resolver.btrc` merges repo defaults, target defaults, user config,
   overrides, and CLI flags.
4. `compiler/checker.btrc` validates BIOS, ROM roots, emulator support,
   rendering assets, content viewport inputs, paths, and feature gaps.
5. `compiler/generator.btrc` creates a target build plan.
6. `generators/*` emit concrete ES-DE, Steam Input, AppImage, renderer handoff,
   and emulator files.
7. Nix builds packages. Make remains a thin CLI wrapper.

## Rendering Architecture

Rendering is a compiler-owned pipeline:

```text
Semu-owned declarations
-> content viewport resolver
-> emulator source hook
-> generated emulator configs
```

The source declarations live under `config/assets/` and
`config/assets/systems/<system>.json`. They describe display class, native
resolution, scale policy, shader/effect assets, bezel assets, safe area, dynamic
aspect behavior, emulator probes, and preferred renderer backend.

The content viewport resolver is the mandatory middle layer. It composes the
system declaration, runtime emulator state, target output size, orientation, and
safe-area policy into a concrete content rectangle plus bezel bounds. Backends
must consume that output so shaders and bezels align to the emulated picture
instead of blindly covering a whole window.

The production renderer backend is an emulator source hook. Each hook is placed
at the game framebuffer or final game-present layer, before emulator settings,
menus, OSD, or frontend UI are drawn. The hook consumes the same generated Semu
render plan, so shader, bezel, aspect, safe-area, and integer-scaling policy
remain unified even though the integration point is emulator-specific.

RetroArch is the first special case: its native content shader preset chain is
already the stable content-layer hook. Semu should drive that chain with
generated presets and launch state instead of post-present injection.

`vkBasalt`, gamescope ReShade, and `semu-render` remain fallback/prototype
wrappers. They are not production-ready defaults because they see whole windows
instead of the emulator content framebuffer and cannot reliably exclude
settings dialogs or frontend UI.

Generated emulator configs are support data, not the rendering policy. They can
set fullscreen mode, graphics API, scaling/aspect defaults, screen layout,
emulator-state export paths, and Semu hook config paths. They must not become
user-owned renderer policy.

ES-DE and Semu settings entries are control surfaces. They must not receive
render hook env or compositor wrappers; only emulator launch commands get the
renderer environment.

## User Overrides

Defaults stay in repo definitions. User-owned state lives under the Semu
home/project:

```text
$SEMU_HOME/
  semu.json
  overrides/
    systems/
    emulators/
    input/
    assets/
  generated/
  state/
  cache/
```

Precedence:

```text
repo defaults
< target defaults
< $SEMU_HOME/semu.json
< $SEMU_HOME/overrides/**
< CLI flags
```

The config UI writes only `semu.json` and `overrides/**`. Emulator config files
are generated outputs only.

Settings and sync defaults are Semu-owned source config in `config/settings/`.
Project-local user edits are stored under `$SEMU_HOME/semu.json` and
`$SEMU_HOME/overrides/**`; native emulator config remains generated.

## Migration Order

1. Checkpoint the current Deck/AppImage/input fixes.
2. Add declarative `config/targets`, `config/systems`, `config/emulators`,
   `config/input`, and `config/assets` definitions.
3. Add the compiler facade and `semu build ...` commands, then delete replaced
   legacy commands once the compiler path owns the behavior.
4. Move manifest/system catalog generation to the compiler model.
5. Move emulator launch/profile/rendering support logic into per-emulator
   generators.
6. Keep visual declarations under `config/assets/systems`.
7. Keep the flattened compiler-oriented source tree.
8. Simplify Make/Nix/AppImage wrappers around `semu build` and `semu verify`.

## Verification

The refactor keeps these gates:

- Existing BTRC smoke tests for launcher, settings, sync, n3DS, AppImage,
  rendering, and Deck evidence.
- Compiler tests for parse, resolve, override precedence, validation, and
  generation.
- Golden-output tests for ES-DE XML, emulator configs, Steam Input templates,
  AppImage launcher files, and generated manifest.
- Per-emulator tests for fullscreen args, input mappings, paths, BIOS/key
  handling, renderer handoff, content viewport resolution, quit, save, and load.
- Deck production checks for ES-DE settings entries, Steam shortcut/Game Mode
  readiness, radial quit evidence, save/load evidence, rendering audit, and
  read-only external ROM handling.
- Steam Deck completion is the prerequisite for any Bazzite parity work.
