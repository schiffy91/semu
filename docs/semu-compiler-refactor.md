# Semu Compiler Architecture

This document describes the current source architecture. It does not assert
that package builds, AppImage deployment, emulator runtime behavior, or physical
Steam Deck acceptance have passed.

## Model

Semu builds a target. BTRC parses declarative product definitions into a typed
model, resolves target and user layers, performs static checks, and emits build
plans and generated product files. Nix builds immutable package slices. The
AppImage builder and Linux installer are bounded shell orchestration around the
compiled CLI and Nix outputs; reducing those wrappers remains cleanup work.

## Ownership Tree

```text
src/
  main.btrc                 production entrypoint
  cli.btrc                  command dispatch
  compiler/                 lexer, parser, model, resolver, checker, generator
  generators/               emulator, ES-DE, settings, input, rendering, package
  lib/                      Semu-specific merge, template, paths, render settings

config/
  targets/                  steam-deck, macos, bazzite
  systems/                  17 system definitions and rendering selections
  emulators/                package, launch/profile, input, render-hook definitions
  input/                    Steam Deck action and Steam Input definitions
  assets/                   pinned shader/bezel manifests and committed assets
  settings/defaults.json    immutable settings schema/defaults/menu model

packaging/
  nix/                      package composition and flake modules
  appimage/                 AppRun, assembly inputs, runtime-root builder
  esde/                     source package and native settings-menu patch
  install/                  digest-addressed Linux installer
  sync/                     Syncthing package/service template

tests/
  compiler/                 typed compiler and generated-output contracts
  core/                     compiler policy and repository tree audit
  fixtures/                 bounded contract inputs
  targets/steamdeck/        physical harness plus local fixtures
  targets/bazzite/          parity harness; gated behind Deck acceptance

docs/                       goal, acceptance, architecture, and current TODO
build/                      ignored generated output only
```

Maintained implementation no longer lives under `src/semu/**`. Product facts
belong in `config/`; generated C, binaries, plans, artifacts, and evidence belong
under ignored `build/` or an installed Semu project root.

## Definitions

Each `config/emulators/<id>/` currently contains:

```text
emulator.json       identity, platform executable/argv, actions, runtime policy
profile.json        emulator-native generated files and structured substitutions
rendering.json      direct render-hook ABI, phases, surfaces, geometry, status
package.json        source/package contract and fallbacks
package.nix         exact package recipe and source-level assertions
<id>.patch          source integration where required
```

The explicit set is RetroArch, Dolphin, PPSSPP, Flycast, Azahar, melonDS,
PCSX2, Cemu, Ryujinx, and ares. The Steam Deck target resolves 17 systems:

```text
GB, GBC, GBA, NES, SNES, Genesis, N64, PSX, NDS,
GameCube, Wii, Dreamcast, PS2, PSP, 3DS, Wii U, Switch
```

NDS declares melonDS as the Linux primary and RetroArch/DeSmuME as a fallback.
Dreamcast and PSP use standalone Linux emulators and RetroArch cores on macOS;
N64 uses RetroArch on Linux and ares on macOS. ES-DE, settings, Steam Input,
rendering, Syncthing, and AppImage assembly are product generators/surfaces, not
emulators.

`src/generators/emulators.btrc` is generic over parsed definitions. Emulator
identity, system bindings, launch arguments, native profile fields, input
encodings, and rendering defaults must not be duplicated there.

## Compiler Flow

1. `src/cli.btrc` parses `build`, `package`, `verify`, `launcher`, `settings`,
   and `config` commands through `std.cli`.
2. `compiler/lexer.btrc` inventories the allowed `config/` definition units.
3. `compiler/parser.btrc` uses `std.json` to load strict schemas into
   `compiler/model.btrc`.
4. `compiler/resolver.btrc` resolves target inheritance, system/emulator
   selection, and settings precedence:

   ```text
   config/settings/defaults.json
   < target settings
   < $SEMU_HOME/semu.json
   < $SEMU_HOME/overrides/**/*.json
   < --settings-json
   ```

5. `compiler/checker.btrc` statically checks target shape, selected platform and
   backend compatibility, package/profile/rendering files, input vocabulary,
   rendering declarations, output ownership, and target-order references. It
   does not prove that physical ROM, BIOS, key, firmware, emulator, or GPU
   runtime inputs work.
6. `compiler/generator.btrc` serializes the checked build plan.
7. A `build configs` operation additionally emits emulator-native profiles,
   launcher shims, and ES-DE system/rule documents from the same model.
8. `settings prepare` initializes Semu-owned settings, emits target config,
   installs generated ES-DE files, and emits/installs the Steam Input template.
9. `package appimage` validates a provenance-bound runtime root and assembles or
   verifies exact AppImage bytes.
10. The physical Deck harness separately verifies real media, processes,
    renderer receipts, screenshots, operator input, and ES-DE return.

## Owned State

The installed AppRun defaults to:

```text
SEMU_PROJECT_DIR=~/.local/share/semu
SEMU_HOME=$SEMU_PROJECT_DIR/settings
```

`settings put` atomically writes `$SEMU_HOME/semu.json`. The resolver also reads
sorted JSON layers under `$SEMU_HOME/overrides/`, but the current settings UI
does not write those override files. Generated profiles and mutable emulator
state stay beneath the project root. External media remains outside this tree
and is mounted/read through bounded launcher policy.

## Rendering Boundary

The current design is an explicit, direct-linked renderer ABI, not window or
swap interception. Each emulator definition names the same ABI-2 symbols:

```text
semu_render_game_gl
semu_render_post_ui_gl
```

Source patches place the game call after final game drawing and before emulator
UI, then place the post-UI call immediately before native presentation. The
renderer receives framebuffer/surface geometry and emulator OpenGL callbacks.
Package recipes contain source assertions intended to reject `LD_PRELOAD`,
GLX/EGL swap interposition, display-server capture, and gamescope integration.

All ten `rendering.json` files currently declare `package_ready: true` and
`runtime_proven: false`, `physical_proven: false`. Compiler tests prove the
definitions and assets resolve; fresh package builds must still prove that each
patch applies and links, and physical runs must prove pixels, layering,
orientation, aspect switching, and quality.

## Settings And ES-DE

`config/settings/defaults.json` defines the typed terminal/UI protocol. The
current configurable fields cover platform ROM roots, shader/bezel/integer-scale
switches, render mode, theme, and basic synchronization enable/start-at-boot
state. Broader per-system asset selection, Wii mode, synchronization-folder
management, and loopback UI access remain product requirements.

The ES-DE source patch defines one native Semu Settings start-menu entry whose
manifest points to an ES-DE-owned regular runner, which delegates to the stable
installed `~/Applications/Semu/bin/semu`. Compiler tests reject AppImage/FUSE
launch text. A fresh ES-DE package build and physical menu/splash interaction
are still required.

## Package And Install Lifecycle

Nix composes the CLI, renderer, ES-DE, source-patched emulator packages,
selected libretro cores, exact visual assets, input runtime, and runtime tools.
The runtime-root builder stages those slices for AppImage assembly. The current
tree has not produced a fresh `build/Semu-x86_64.AppImage`.

`packaging/install/installer.sh` implements digest verification, one-time
AppImage extraction, immutable release-tree hashing, a regular stable launcher,
current/previous identities, rollback, two-release pruning, and transactional
cleanup of the modeled retired layout. Its fixture contract passes. This is not
evidence that a current artifact has been deployed or launched on the Deck.

## Commands And Current Blockers

The intended development interface is:

```sh
make btrc-build
build/semu build target steam-deck --project "$PWD"
build/semu build emulator retroarch --target steam-deck --project "$PWD"
build/semu build configs --target steam-deck --project "$PWD"
build/semu verify target steam-deck --project "$PWD"
make appimage-build
make appimage-verify
```

As of 2026-07-19, a forced strict production compile and the aggregate
`make test` pass. Flake no-build checking still fails while evaluating
visual-asset and emulator package inputs with invalid Nix store source/patch
paths. Until that is fixed and packages are built, AppImage success must not be
claimed.

The root Makefile still performs direct transpiler, Nix, runtime-builder, and
target-harness orchestration. Keeping Make thin and moving durable policy to
BTRC, JSON, and Nix remains open cleanup work.
