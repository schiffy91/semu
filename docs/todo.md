# Semu Production TODO

This checklist is subordinate to `production-goal.md` and
`acceptance-matrix.md`. `[x]` means the narrowly worded automated contract was
executed successfully against the current tree. It never means physical
acceptance. Every physical requirement remains unchecked until retained evidence
from one verified release proves it.

## Immediate Build Blockers

- [ ] Make `nix flake check path:. --no-build` evaluate every package and app;
  visual-asset and emulator inputs currently fail with invalid Nix store
  source/patch paths.
- [ ] Run `nix flake check path:.` and every focused package check from a clean
  generated-output root.
- [ ] Produce a fresh `build/Semu-x86_64.AppImage`; no current artifact exists.

## Automated Contracts Present

- [x] A forced `make -B btrc-build` compiles the production entrypoint with
  strict imports, no cache, and no implicit stdlib; aggregate `make test` passes.
- [x] The ownership tree has no maintained `src/semu/**` or `src/runtime/**` and
  passes the tree audit plus 20 rejection fixtures.
- [x] Focused strict-import compiler tests pass for inventory, parse, target
  inheritance, five-layer settings precedence, static checking, deterministic
  plans, emulator profiles, launch plans, and ES-DE output.
- [x] The 17 system definitions resolve exact committed bezel metadata/hashes,
  shader recipe hashes, semantic surface geometry, integer-scaling policy, and
  1280x800 plus 1920x1080 aperture-fit contracts.
- [x] Steam Input generation passes its VDF, semantic icon, destination, and
  bounded install contracts.
- [x] The typed settings store/UI protocol and one native ES-DE Semu Settings
  manifest/runner contract pass.
- [x] The local Deck harness compiles strictly, resolves 17 fixture systems,
  writes an acceptance plan, and prints a mutation-free command dry run.
- [x] The installer fixture proves one-time extraction, stable no-FUSE launch,
  upgrade, rollback, corruption rejection, two-release retention, and
  transactional retired-layout cleanup.

## Compiler And Repository Cleanup

- [x] Keep production source under `src/main.btrc`, `src/cli.btrc`,
  `src/compiler/`, `src/generators/`, and `src/lib/`.
- [x] Keep declarative product inputs under `config/targets`, `config/systems`,
  `config/emulators`, `config/input`, `config/assets`, and `config/settings`.
- [x] Keep one generic emulator generator over parsed definitions rather than
  one-line emulator classes or emulator-named BTRC branches.
- [x] Keep production and test entrypoints separate; tests are not imported by
  `src/main.btrc`.
- [ ] Make every maintained BTRC production, package, and test entrypoint compile
  with strict per-file imports, no cache, and no implicit stdlib.
- [ ] Remove every remaining hard-coded duplicate of launch, input, rendering,
  path, system, and package facts already owned by JSON or Nix.
- [ ] Remove all dead files, generated-source fallbacks, stale shell adapters,
  compatibility aliases, and obsolete local compiler paths.
- [ ] Reduce the root Makefile to thin compiler/Nix/test calls and keep shell
  orchestration bounded; move durable policy into BTRC, JSON, or Nix.
- [ ] Enforce canonical pretty formatting for every maintained and generated JSON
  file with a focused test.
- [x] Audit `README.md` and `docs/**` against the 2026-07-19 tree without claiming
  package, AppImage, or physical success.

## Packages And AppImage

- [ ] Fresh-build the shared renderer and execute its ABI/export/install checks.
- [ ] Fresh-build source-patched RetroArch plus the exact selected libretro core
  closure; reject host, Flatpak, and prebuilt runtime fallbacks.
- [ ] Fresh-build Dolphin, PPSSPP, Flycast, Azahar, melonDS, PCSX2, Cemu, and
  Ryujinx for x86_64 Linux with their package/linkage/source-hook checks.
- [ ] Fresh-build and execute the declared macOS ares N64 path.
- [ ] Bind every emulator and core to exact source/package provenance and publish
  reproducible package/AppImage identities.
- [ ] Assemble one self-contained AppImage containing ES-DE, Semu, all selected
  emulator/core slices, renderer, assets, input runtime, Syncthing, and runtime
  closure.
- [ ] Verify exact AppImage bytes, provenance, no-FUSE extraction, embedded
  closure completeness, and two consecutive launches from the stable launcher.
- [ ] Deploy only the verified digest to the Deck, migrate/remove obsolete Semu
  releases transactionally, and retain current/previous/rollback evidence.
- [ ] Prove default ROM discovery works with
  `/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs` and remains settings-controlled.

## Rendering

- [x] Every emulator definition declares the same direct-linked ABI-2 game and
  post-UI hooks; source patches exist for RetroArch, Dolphin, PPSSPP, Flycast,
  Azahar, melonDS, PCSX2, Cemu, Ryujinx, and ares.
- [x] Definitions select renderer-owned Slang shaders/bezels for 15 era systems
  and modern clean output for Wii U and Switch.
- [ ] Execute every emulator package's source assertions proving patch placement,
  linkage, hook order, and rejection of `LD_PRELOAD`, GLX/EGL swap interception,
  gamescope effects, display capture, and runtime symbol discovery.
- [ ] Prove every selected multi-pass shader loads all referenced textures,
  parameters, and temporal resources through librashader on the target GPU.
- [ ] Prove independent shader history and correct touch/game rectangles for DS
  and 3DS top/bottom surfaces.
- [ ] Prove 4:3/16:9 switching uses live emulator-reported content state for N64,
  PSX, Dreamcast, GameCube, Wii, and PS2.
- [ ] Prove opaque bezel art cannot cover game pixels and every aperture remains
  correctly framed at 1280x800 and at least one external-display aspect ratio.
- [ ] Inspect real-game screenshots for era accuracy, photorealistic bezel
  quality, correct z-order, nonblank pixels, no clipping, and upright orientation.
- [ ] Expose per-system shader, bezel, layout, and emulator override choices in
  Semu-owned settings and compile them into generated runtime state.

## Input

- [ ] Fresh-build and execute the Linux input supervisor contract for both
  Start-then-Select and Select-then-Start process-group termination.
- [ ] Prove Steam Virtual Gamepad gameplay input for every configured system and
  eliminate the RetroArch Xbox/autoconfig notification.
- [ ] Prove the left-trackpad quick/menu radials in real Game Mode with semantic
  quit, save, load, menu, and system-specific icons rather than Ctrl labels.
- [ ] Prove Start+Select immediately terminates each emulator process group and
  returns ES-DE to the foreground.
- [ ] Prove save/load performs an actual state change and restoration wherever
  the selected emulator supports state operations.
- [ ] Prove Wii Wiimote, Wiimote+Nunchuk, Classic Controller, and GameCube modes,
  including quick radial switching during a real game.

## Settings, ES-DE, And Sync

- [x] The dependency-free BTRC settings store supports typed get/put and a
  terminal/UI JSON protocol while writing only `$SEMU_HOME/semu.json`.
- [x] The compiler reads sorted, higher-precedence user override JSON without
  allowing `settings put` to modify those files.
- [x] ES-DE source and compiler contracts define exactly one native Semu Settings
  start-menu entry over a regular direct runner, with no fake system or FUSE text.
- [ ] Fresh-build the patched ES-DE package and prove a visible nonblack splash,
  bounded startup, one functional Settings entry, and no FUSE dependency on Deck.
- [ ] Extend the BTRC settings UI to cover per-system rendering/layout choices,
  input behavior, Wii mode, synchronization folders/devices/status, and a route
  to Syncthing's loopback UI without external UI dependencies.
- [ ] Compile all settings exclusively into Semu-owned generated files, then into
  emulator-native outputs; prove no emulator-owned file is edited in place.
- [ ] Compile enabled synchronization folders into generated Syncthing config and
  grant write access only to those exact folders.
- [ ] Prove the isolated Syncthing user service, boot persistence, loopback UI,
  status, and disable/re-enable behavior on the Deck.

## Physical Deck Matrix

- [ ] Preflight Game Mode, SSH, readable controller events, no-sudo keep-awake
  inhibitor, minimum safe brightness, charging/power stability, and writable
  Semu-owned state before launching the matrix.
- [ ] Prove the ES-DE frontend row in `acceptance-matrix.md`.
- [ ] Physically accept Game Boy.
- [ ] Physically accept Game Boy Color.
- [ ] Physically accept Game Boy Advance.
- [ ] Physically accept NES.
- [ ] Physically accept SNES.
- [ ] Physically accept Genesis.
- [ ] Physically accept Nintendo 64, including the non-inverted asymmetric frame.
- [ ] Physically accept PlayStation with isolated BIOS hash evidence.
- [ ] Physically accept Nintendo DS through melonDS and the declared RetroArch
  fallback.
- [ ] Physically accept GameCube.
- [ ] Physically accept Wii with all four controller modes.
- [ ] Physically accept Dreamcast.
- [ ] Physically accept PlayStation 2.
- [ ] Physically accept PSP.
- [ ] Prove the selected Deck 3DS ROM is decrypted and matches the declared Mac
  source; if not, perform only a bounded non-overwriting hash-verified copy.
- [ ] Physically accept Nintendo 3DS.
- [ ] Physically accept Wii U.
- [ ] Physically accept Switch.
- [ ] Retain unchanged before/after hashes for external ROM, BIOS, key, firmware,
  and existing save trees across the complete matrix.
- [ ] Run Bazzite parity only after every physical Steam Deck item above passes.
