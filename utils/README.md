# Semu Utilities

Root-level utilities are thin host entrypoints only. Compiler/runtime
implementation lives under `src/**`; generated outputs live under `build/**`
or the configured Semu project directory.

## Steam Deck Bootstrap

`utils/steam-deck-bootstrap.sh` prepares a Deck/Linux host enough to run the
compiler target:

```sh
utils/steam-deck-bootstrap.sh install --yes
utils/steam-deck-bootstrap.sh status
utils/steam-deck-bootstrap.sh update --yes
```

The script clones or updates the source checkout, optionally builds the CLI or
flake package with Nix, then runs:

```sh
semu build target steam-deck --project "$SEMU_PROJECT"
semu verify target steam-deck --project "$SEMU_PROJECT"
```

Steam Deck/SSH probes live under `tests/deck/` and are exposed through
`make deck-ssh-smoke`.
