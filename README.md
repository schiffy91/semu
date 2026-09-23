# Semu

One declarative, Nix-built emulation environment. Semu reads its own JSON
definitions under `config/`, resolves a target and the user's settings, and
compiles ES-DE documents, emulator profiles and launch plans from them. Every
emulator is built by Nix. `PLAN.md` is the goal contract and carries the live
status.

Targets:

- `linux-desktop`: a NixOS desktop with ROMs under
  `~/Games/Emulation/ES-DE/ES-DE/ROMs`.
- `steam-deck`: the physical Deck with the SD-card ROM layout.

## Layout

```text
src/          BTRC: model, resolve, check, emit (ES-DE, profiles), launch, cli
config/       targets, systems, emulators, input, bezels (packages), assets, settings defaults
packaging/    Nix packages and the flake helpers
tests/        contracts/ (make test; spec/ holds the ported emulator specs), integration/
              (real emulator runs in flake checks), visual/ (render host, gallery, Xvfb captures)
build/        ignored build output
```

## One flake per emulator and core

Every emulator, every libretro core, RetroArch, ES-DE and the renderer is its
own flake: `config/emulators/<id>/flake.nix`,
`config/emulators/retroarch/cores/<core>/flake.nix`,
`config/emulators/retroarch/flake.nix`, `packaging/esde/flake.nix` and
`src/renderer/flake.nix`. Each pins its upstream source as a non-flake input
(revision plus content hash in its own `flake.lock`), borrows only build
wiring from nixpkgs, sets `allowSubstitutes = false` so the binary is always
compiled by us, and declares `semu.platforms = { linux; macos; windows; }`.
Linux and macOS packages exist wherever the recipe builds there; Windows is
declared `"planned"` everywhere until a port lands. Each flake builds alone
(`nix build ./config/emulators/dolphin`), and the root flake composes them
through relative path inputs with `nixpkgs` shared by `follows`. The
`platform-matrix` flake check evaluates every flake's Linux and macOS
derivations and refuses one that could be substituted or that mislabels a
platform. Bumping an emulator: edit its flake's `source` input, run
`nix flake lock` in that directory, then at the root.

## Build and test

The CLI, the contracts and the bezel tools build and run on Linux and on macOS (a
development host: evdev input is idle there and nothing launches emulators). Nothing needs
`btrcpy` on PATH; the Makefile runs the flake's pinned compiler.

```sh
make build           # transpile src/semu.btrc with btrcpy and compile it
make test            # links the pinned Mega Bezel tree (make bezel-tree), then every contract
make nix-check       # build and run every flake check for this system
make configs         # emit ES-DE documents and profiles into build/targets/<target>
make nix             # build the bundle (CLI, ES-DE, emulators) at build/nix/result
make doctor          # show resolved paths and what is missing
```

## Visual checks

On the Mac the real renderer runs offscreen (`tests/visual/render_host.btrc`, CGL), fed by
`semu render-env` and the bundle's data (`nix build .#asset-root`):

```sh
tests/visual/render.sh out 1280x800 gb gba:arctic nes:black:sharp nds:none:none   # system[:bezel[:shader]]
tests/visual/gallery.sh --quick out/gallery   # every variant at Deck size
tests/visual/gallery.sh out/gallery           # Deck and 4K, flat-card placement verified within 2 px
tests/visual/hot-reload.sh                     # a compositor edit mid-game changes nothing but the program
```

On Linux, real emulators on a private Xvfb display:

```sh
nix shell nixpkgs#xorg.xorgserver nixpkgs#xorg.xwd nixpkgs#imagemagick -c \
  env SEMU=build/semu SEMU_ASSET_ROOT=build/nix/result SEMU_SOURCE_ROOT=config \
  tests/visual/capture.sh out.png retroarch --system snes --rom "Super Mario Kart (USA).zip"
```

`tests/visual/capture.sh` launches through Semu on a private Xvfb display and
captures the composited frame, so bezels, shaders, curvature, corner masks,
bloom, vignette and the halo on the bezel can be inspected while the real
desktop stays untouched. `SEMU_CAPTURE_WAIT` sets the boot wait (NES needs
about 25 s), `SEMU_CAPTURE_DISPLAY` picks the display number so captures can
run in parallel.

## Steam Deck release

```sh
nix build .#release --out-link build/release     # Semu-x86_64.tar.zst, its .sha256, install.sh
sh build/release/install.sh install build/release/Semu-x86_64.tar.zst   # on the Deck (or any Linux host)
~/Applications/Semu/bin/semu-deck                  # ES-DE with every emulator, target steam-deck
~/Applications/Semu/bin/semu-deck-cli doctor       # the semu CLI inside the release
sh build/release/install.sh rollback               # back to the previous release
DECK_HOST=deck tests/deck/deploy.sh install        # copy, install and report over SSH
```

The tarball holds the bundle's whole Nix closure under `nix/store`. The
launcher runs programs through bubblewrap with that tree mounted read-only at
`/nix`, so the Deck needs no Nix installation and no FUSE. The installer
verifies the digest, installs into `~/Applications/Semu/releases/<digest>`,
switches the `current` symlink, keeps one `previous` release, and writes a
desktop entry.

## Commands

```sh
semu build configs --target linux-desktop [--output DIR]
semu prepare --target linux-desktop [--es-de-home DIR]   # default: <esde_home>/ES-DE
semu path esde_home --target linux-desktop
semu launch retroarch --system gb --core gambatte --rom "Tetris (World) (Rev 1).zip"
semu doctor --target linux-desktop
semu settings ui                    # the document ES-DE renders under SEMU SETTINGS
semu sync start | stop | status | device-id   # Syncthing for saves (sync.enabled, sync.peer_device_ids)
semu steam shortcuts [--steam-root DIR] [--force] | status   # the Semu non-Steam shortcut
```

ES-DE's main menu carries a SEMU SETTINGS entry (an ES-DE patch that only
renders the document `semu settings ui` returns and saves through
`semu settings put`). Save sync runs Syncthing from the bundle with a Semu-owned
home under the state root sharing `paths.content_root` as `semu-saves`; enable
it in the menu, read this device's id there, and paste the other device's id
into PEER DEVICE IDS on both sides. The ES-DE launcher starts sync when enabled;
on NixOS the `semu-sync` user service also runs it.

`launch` regenerates the emulator's profile under the state root, starts the
emulator in its own process group, and watches every gamepad for Start+Select
within 250 ms in either order. The chord terminates the whole process group and
returns to ES-DE.

Shaders and bezels come from one renderer, `libsemurenderer`, that hooked
emulators link directly (RetroArch's gl3 driver calls it after the game draw
and before present). Native OpenGL emulators that cannot be patched get the
same renderer through `libsemupreload.so`, an LD_PRELOAD shim that composes
at their swap call (`render_preload` in `config/emulators/<id>/emulator.json`;
Flycast, PPSSPP and Dolphin today). The launcher passes the system's shader preset and the
selected bezel package (`config/bezels/<id>/bezel.json`: plate, background,
layout, frame and per-screen tube, shape, look, glass and shader) through
`SEMU_RENDER_*`, bound by `config/systems/<id>/{shaders,bezels}.json`.
Package geometry comes from the pinned Mega Bezel presets, not from a render:
`semu bezel resolve` follows a package's upstream preset chain the way
RetroArch does, `semu bezel emit` ports Mega Bezel's own placement math to
write each screen's picture, black edge, bezel ring and opening plus the
preset's texture stack as layers (`layers`, `canvas_layer`), and `semu bezel
edit` serves an editor on `http://127.0.0.1:8765/` where every layer can be
hidden, reordered or made the canvas and every rectangle and corner radius
dragged at integer zoom, with Save writing the package back. The upstream
plates are bundled verbatim (`.#bezel-layers`); recolours (`recolor`) and the late-night light
(`ambient`) are drawn by the renderer over them, and the few flattened plates are baked from
recipes when the bundle is built (see `config/assets/NOTICE.md` for the art's licences). Every
system offers its default, an alternate and `none` for both bezel and shader. Switch variants
without a rebuild:

```sh
semu settings put visual.systems.gb.bezel_variant studio
semu settings put visual.systems.gb.shader_variant pocket
semu settings put visual.bezels false
semu settings get visual.integer_scaling
```

Settings precedence:

```text
config/settings/defaults.json
< config/targets/<target>.json settings
< $SEMU_HOME/semu.json
< $SEMU_HOME/overrides/**/*.json
< --settings-json
```

`$SEMU_HOME` defaults to `~/.config/semu`. Generated emulator state lives under
`paths.state_root` (default `~/.local/share/semu/<emulator>`), saves and states
under `paths.content_root` (default `~/Games/Emulation/Semu`).
