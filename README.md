# Semu

One declarative, Nix-built emulation environment. Semu reads its own JSON
definitions under `config/`, resolves a target and the user's settings, and
compiles ES-DE documents, emulator profiles and launch plans from them. Every
emulator is built by Nix. `PLAN.md` is the goal contract and the record of what
was observed. Open work is tracked at https://github.com/schiffy91/semu/issues.

Targets:

- `linux-desktop`: a NixOS desktop.
- `steam-deck`: the physical Deck.
- `macos`: an Apple Silicon Mac, with RetroArch, Dolphin, Azahar and Ryujinx.

Every target keeps its library at `~/Drive/media/Games/Emulation`, a folder of Google Drive. On the Linux targets the Drive client keeps that folder downloaded in place; on the Deck it stores it on the SD card.

## The library

Everything the owner has is one folder, the library root (`paths.root`, or `$SEMU_ROOT`
when the host names it):

```text
roms/<system>/        games
media/<system>/       ES-DE's scraped art
gamelists/  themes/  collections/   ES-DE's lists and looks
bios/                 firmware and keys: RetroArch's system files, plus ps2/ wiiu/ switch/ n3ds/ nds/ dc/
mods/<emulator>/      texture packs and mods
saves/<emulator>/     every save: RetroArch's under saves/retroarch/<core>, each standalone emulator's own folder
states/<emulator>/    save states
screenshots/
settings/<target>/    semu.json, the choices made in SEMU SETTINGS on that kind of machine
```

An emulator's own state (generated profiles, shader caches, logs) and the generated ES-DE
home are per machine and disposable: `paths.state_root`, `~/.cache/semu` on Linux and
`~/Library/Caches/Semu` on macOS. Each emulator's save folders there are links into
`saves/` and `states/` (`state.links` in its `emulator.json`), and the ES-DE home's
`gamelists`, `themes` and `collections` are links into the library, so deleting the state
root loses nothing. Semu writes nowhere else.

`semu migrate [--root DIR] [--state DIR] [--dry-run]` brings a library from the old layout
(`ES-DE/ES-DE/ROMs`, `RetroArch/config/system`, a folder per emulator) to this one, following
`config/migrations/layout.json`. `--state` names the old state root (`~/.local/share/semu`,
or `<root>/Semu/emulators` on the Deck) whose saves are newer than the folders they were
copied from, so they go first; a file never replaces one, and whatever is left of the old
tree ends under `_legacy/` for the owner to look through and delete. Moves are renames; a
copy across filesystems keeps its source. A marker (`.semu-layout`) makes a second run a
no-op.

## Layout

```text
src/          BTRC: model, resolve, check, emit (ES-DE, profiles), launch, cli
config/       targets, systems, emulators, input, bezels (packages), assets, settings defaults
packaging/    Nix packages and the flake helpers
tests/        contracts/ (make test; spec/ holds the ported emulator specs), integration/
              (real emulator runs in flake checks), visual/ (render host, gallery, Xvfb captures),
              deck/ (the Deck's off-screen harness and its case files)
ci/           checks.sh (every flake check), generated.sh, states.sh
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

The CLI, the contracts and the bezel tools build and run on Linux and on macOS (evdev input
is idle there; pads come through Apple's GameController). Nothing needs `btrcpy` on PATH:
without one the Makefile runs the flake's pinned compiler.

```sh
make build           # transpile src/semu.btrc with btrcpy and compile it
make test            # links the pinned Mega Bezel tree (make bezel-tree), then every contract
make nix-check       # build and run every flake check for this system
make configs         # emit ES-DE documents and profiles into build/targets/<target>
make nix             # build the bundle (CLI, ES-DE, emulators) at build/nix/result
make doctor          # show resolved paths and what is missing
```

The emulators take no compiler. What they need of Semu's BTRC is two small programs, the loader they
link and the bridge RetroArch compiles into its gl3 driver, and both are built from their committed C
(`src/renderer/loader/generated`, `src/renderer/retroarch/generated`). So a new btrc rebuilds Semu's own
programs and none of RetroArch, PCSX2 or the other emulators. The check `generated-c` has the compiler
make both afresh and fails if the committed files differ; `ci/generated.sh` writes them again after a
change to either source.

The check `real-cores` boots each RetroArch core with a freely licensed test program, saves a state,
loads it back, and then loads the state the same program left under the lock before this one
(`tests/integration/states`). A core update that no longer takes that state fails the check: that is how
an update that breaks the states already on a machine is seen before it ships. `ci/states.sh` keeps this
lock's states as the next one's, once such a break is accepted or a core is added.

## Visual checks

On the Mac the real renderer runs offscreen (`tests/visual/render_host.btrc`, CGL), fed by
`semu render-env` and the bundle's data (`nix build .#asset-root`):

```sh
tests/visual/render.sh out 1280x800 gb gba:arctic nes:black:sharp psx:tv::bezel_fractional   # system[:bezel[:shader[:fit]]]
tests/visual/gallery.sh --quick out/gallery   # every variant at Deck size
tests/visual/gallery.sh out/gallery           # Deck and 4K, flat-card placement within 2 px in the four Fit states
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
desktop entry. With Steam closed it also adds the Semu shortcut and the
controller profile to Steam; while Steam runs it prints the two commands.

## Cache and releases

`.github/ci/targets.nix` is the single list of what gets built, for which systems, on which
runners, and what a release carries. Adding a system or an attribute there is the whole
change: the Makefile's cache targets and the cache and release workflows read that file.

A machine of your own publishes for the others, with no account anywhere:

```sh
make cache            # this host's .github/ci/targets.nix attrs, signed into ~/.cache/nix-cache
make cache-all        # the composed bundle and the Deck release as well
make cache-serve      # serve ~/.cache/nix-cache over HTTP (PORT=8080)
```

The first run writes a signing key to `~/.config/nix/semu-cache-1.key` and prints its
public half. Keep a copy: it is what lets the other machines trust the cache. On each of
them, where `HOST` is the publisher:

```nix
nix.settings.extra-substituters = [ "http://HOST:8080" ];
nix.settings.extra-trusted-public-keys = [ "semu-cache-1:GAvdDO5fCGcTJsr1yyVLgnVn7TTyFYnfEkb7fd2zUew=" ];
```

In CI, `.github/workflows/cache.yml` builds one job per system and attribute every
Monday and on every push to `main`. With a `CACHIX_AUTH_TOKEN` secret and a cache named
in `.github/ci/targets.nix` it pushes there; with a `NIX_CACHE_SECRET_KEY` secret and no Cachix
it signs the closure and leaves a flat cache as a workflow artifact, which
`nix copy --from file://...` consumes unchanged. Set the `SEMU_CACHE_URL` and
`SEMU_CACHE_PUBLIC_KEY` repository variables and the runners substitute from the cache
they are filling.

The composed bundle is about 4.5 GiB and does not fit a hosted runner's disk or a
GitHub Pages site, which is why `semu` and `release` are in `local` rather than in a CI
job: build those on your own machine and let the other machines pull them. The parts
that do fit — the CLI, the renderer, ES-DE, RetroArch, the bezel and visual assets — are
what CI keeps warm.

`.github/workflows/release.yml` turns a `v*` tag into a GitHub Release carrying
`Semu-x86_64.tar.zst`, its digest, `install.sh` and a `SHA256SUMS`, built from the
`release` entry of `.github/ci/targets.nix`. The tarball measured 1.5 GiB here — the
4.5 GiB closure compresses well enough to stay under GitHub's 2 GiB asset limit, so the
Deck release travels as one asset and `install.sh` verifies the digest beside it.

## Commands

```sh
semu build configs --target linux-desktop [--output DIR]
semu prepare --target linux-desktop [--es-de-home DIR]   # default: <esde_home>/ES-DE
semu path esde_home --target linux-desktop
semu launch retroarch --system gb --core gambatte --rom "Tetris (World) (Rev 1).zip"
semu doctor --target linux-desktop
semu settings ui                    # the document ES-DE renders under SEMU SETTINGS
semu migrate --dry-run             # what the move to the single-root layout would do
semu steam shortcuts [--steam-root DIR] [--force] | status   # the Semu non-Steam shortcut
```

ES-DE's main menu carries a SEMU SETTINGS entry (an ES-DE patch that only
renders the document `semu settings ui` returns and saves through
`semu settings put`).

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
recipes when the bundle is built (see `config/assets/NOTICE.md` for the art's licences). A
system offers its default and `none` for both bezel and shader, and most an alternate. Switch variants
without a rebuild:

```sh
semu settings put visual.systems.gbc.bezel_variant berry
semu settings put visual.systems.gb.shader_variant pocket
semu settings put visual.bezels false
semu settings get visual.systems.psx.placement   # Fit: game, bezel, game_fractional or bezel_fractional
semu settings put visual.systems.n64.render_scale 2x   # a scale the system offers, applied at Restart Game
```

Settings precedence:

```text
config/settings/defaults.json
< config/targets/<target>.json settings
< $SEMU_HOME/semu.json
< $SEMU_HOME/overrides/**/*.json
< --settings-json
```

`$SEMU_HOME` defaults to `<root>/settings/<target>`. `$SEMU_ROOT` replaces the target's
`paths.root`; a path named on its own in settings (`paths.roms`, `paths.bios`, ...) stays
where it is named, and an empty `paths.mods` turns mods off.

## Licensing and distribution

Semu-owned code is [MIT licensed](LICENSE). Third-party code and assets retain
separate licences, including the emulator patches; see
[packaging/LICENSES.md](packaging/LICENSES.md) and
[the artwork notice](config/assets/NOTICE.md).

The composed bundle and `release` outputs are for personal installation. They
contain locally generated artwork that is not cleared for redistribution; do
not upload those outputs to GitHub Releases, Packages, or a public Nix cache.
Horizon remains a selectable preference for a separately installed local theme,
but is not fetched or bundled by Semu while its redistribution permission is
unresolved. Existing local installations are unaffected.

`make test` checks tracked media against the reviewed icon manifest. Before a
push, run `tests/integration/repository-content.sh HEAD` to check reachable
history too. Keep recordings, game screenshots, ROMs, BIOS, firmware and keys
outside the repository. After the October 2026 history cleanup, use a fresh
clone or the rewritten branch; merging an old clone can restore removed files.
