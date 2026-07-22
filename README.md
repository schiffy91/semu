# Semu

Semu is a BTRC compiler for a reproducible emulation target. It reads
Semu-owned JSON definitions, resolves target and user settings, checks the
model, and emits build plans, emulator profiles and launchers, ES-DE files,
Steam Input assets, and AppImage packaging inputs. Nix owns package builds.

The production target is not accepted yet. Automated contracts are not physical
Steam Deck evidence; the required device behavior is tracked in
[`docs/production-goal.md`](docs/production-goal.md) and
[`docs/acceptance-matrix.md`](docs/acceptance-matrix.md).

## Current Status

Audited 2026-07-21:

- A forced `make -B btrc-build` passes strict production compilation, and
  `make test` passes the aggregate host contracts.
- The local 17-system Deck harness and installer lifecycle fixture pass. The
  generated Game Boy plan contains the exact 18-case shader/bezel/integer-scale
  matrix derived from owned rendering definitions.
- `nix flake check . --no-build --all-systems` evaluates every declared package,
  app, shell, and check. This is evaluation evidence, not package realization.
- `build/Semu-x86_64.AppImage` is absent, and no physical acceptance receipts
  are retained. Do not deploy or claim a system accepted from this tree yet.

## Repository

```text
src/          BTRC entrypoint, compiler, generators, and Semu libraries
config/       declarative targets, systems, emulators, input, assets, settings
packaging/    Nix packages, AppImage assembly, installer, ES-DE, and Syncthing
tests/        compiler contracts, fixtures, and target harnesses
docs/         production contract, architecture, acceptance matrix, and TODO
build/        ignored generated binaries, plans, artifacts, and evidence
```

`src/main.btrc` is the production entrypoint. Each
`config/emulators/<id>/` owns its emulator definition, generated-profile data,
render-hook contract, source/package metadata, Nix recipe, and source patch.
System bindings and rendering choices live under `config/systems/`.

Settings precedence is:

```text
config/settings/defaults.json
< target settings
< $SEMU_HOME/semu.json
< $SEMU_HOME/overrides/**/*.json
< --settings-json
```

`settings put` writes only `$SEMU_HOME/semu.json`. Override files are separate
user-owned, higher-precedence inputs. Generated emulator-native files are
outputs and must not be hand-edited. External ROM, BIOS, key, firmware, and
existing save trees remain read-only unless an explicit bounded copy operation
records source and destination hashes.

## Local Contracts

These commands currently pass and do not contact the Deck:

```sh
make -B btrc-build
make test
make tree-audit
make compiler-tests
make -f tests/targets/steamdeck/Makefile test
make -f tests/targets/steamdeck/Makefile installer-contract
```

The Deck harness `test` target compiles strict BTRC entrypoints, exercises 17
fixture descriptors, writes an acceptance plan, and prints commands. It does
not run SSH, launch a ROM, inspect pixels, or prove controls.

The remaining production build gates are:

```sh
nix flake check path:.
make appimage-build
make appimage-verify
make verify-production
```

## Install And Settings

Once a fresh AppImage passes exact-byte verification, install it with:

```sh
packaging/install/installer.sh install \
  --artifact build/Semu-x86_64.AppImage \
  --roms /run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
```

The passing installer fixture covers digest-bound one-time extraction, a regular
no-FUSE launcher, current/previous releases, rollback, corruption rejection,
and transactional cleanup. Those are automated installer contracts, not proof
that the current artifact has been installed on the Deck.

The intended installed command surface is:

```sh
~/Applications/Semu/bin/semu settings get visual.integer_scaling
~/Applications/Semu/bin/semu settings put visual.integer_scaling false --apply \
  --target steam-deck
~/Applications/Semu/bin/semu settings terminal
~/Applications/Semu/bin/semu settings prepare --target steam-deck
```

Compiler tests cover the typed settings protocol and one native ES-DE Semu
Settings entry. The actual ES-DE menu, splash, and no-FUSE launch path remain
physical acceptance items.

## Steam Deck Acceptance

After a fresh artifact is built and verified:

```sh
make -f tests/targets/steamdeck/Makefile preflight
make -f tests/targets/steamdeck/Makefile deploy
make -f tests/targets/steamdeck/Makefile prepare
make -f tests/targets/steamdeck/Makefile accept-system SYSTEM=gb
```

`accept-system` requires unattended runtime evidence plus real Game Mode
Start+Select and radial-menu operator actions. No physical row can pass from
synthetic input or a dry run. Bazzite parity runs only after every physical Deck
row passes.
