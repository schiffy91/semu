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
config/       targets, systems, emulators, input, assets, settings defaults
packaging/    Nix packages and the flake helpers
tests/        contracts/ (run by make test), spec/ (reference fixtures),
              integration/ (real emulator runs)
build/        ignored build output
```

## Every emulator is compiled here

Semu never ships a cache binary. Each emulator directory carries a
`package.json` whose `source` block pins one upstream revision or release tag
plus its content hash, and `package.nix` builds that pin with the build wiring
nixpkgs already knows (dependencies, cmake flags, wrappers). `cores.json` pins
every libretro core the same way, and ES-DE and RetroArch are pinned sources
with Semu's patches on top. All of these set `allowSubstitutes = false`, and
`packaging/nix/emulators.nix` refuses at evaluation time any emulator that
could be substituted. Bumping an emulator means editing its `source` block.

## Build and test

```sh
make build           # transpile src/semu.btrc with btrcpy and compile it
make test            # contract tests against the real config tree
make configs         # emit ES-DE documents and profiles into build/targets/<target>
make nix             # build the bundle (CLI, ES-DE, emulators) at build/nix/result
make doctor          # show resolved paths and what is missing
```

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
Flycast, PPSSPP and Dolphin today). The launcher passes the system's shader preset, bezel
art, hole and canvas policy through `SEMU_RENDER_*` from
`config/systems/<id>/{shaders,bezels}.json`. Switch variants without a
rebuild:

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
