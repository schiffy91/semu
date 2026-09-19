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

## Build and test

```sh
make build           # transpile src/semu.btrc with btrcpy and compile it
make test            # contract tests against the real config tree
make configs         # emit ES-DE documents and profiles into build/targets/<target>
make nix             # build the bundle (CLI, ES-DE, emulators) at build/nix/result
make doctor          # show resolved paths and what is missing
```

## Commands

```sh
semu build configs --target linux-desktop [--output DIR]
semu prepare --target linux-desktop [--es-de-home ~/ES-DE]
semu launch retroarch --system gb --core gambatte --rom "Tetris (World) (Rev 1).zip"
semu doctor --target linux-desktop
```

`launch` regenerates the emulator's profile under the state root, starts the
emulator in its own process group, and watches every gamepad for Start+Select
within 250 ms in either order. The chord terminates the whole process group and
returns to ES-DE.

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
