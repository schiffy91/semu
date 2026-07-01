# Testing

## Host

Run the normal local path:

```sh
make test
```

This runs:

- `tree-audit`: fails if stale root output directories or generated tracked
  files outside approved snapshots exist.
- `payload-audit`: fails if licensed payloads or VM artifacts would be
  upstreamed.
- `generated-smoke`: builds from committed `generated/semu.c` and runs BTRC
  smoke tests.
- `appimage-smoke`: assembles an AppImage with fake ES-DE, fake Nix, and fake
  appimagetool.

Run the fuller deterministic verification path:

```sh
make verify
```

## Graph E2E

```sh
make e2e-graph-list
make e2e-graph-status
make e2e-graph-coverage
make e2e-graph-run
```

Graph specs live in `tests/e2e/`. Graph state and VM/test artifacts belong
under `generated/test/`.

## Steam Deck

Physical Deck helpers live in `tests/steamdeck/`:

```sh
tests/steamdeck/ssh-smoke.sh
generated/build/semu deck provision --project "$PWD"
generated/build/semu deck verify-emulators --project "$PWD"
generated/build/semu deck verify-sync --project "$PWD"
generated/build/semu deck verify-input --project "$PWD"
```

The Steam Deck bootstrap script is `packaging/steamdeck/bootstrap.sh`.

## VM

Linux VM targets use `generated/test/vms/`:

```sh
make deck-vm-start
make deck-vm-verify
make bazzite-vm-smoke
make bazzite-e2e-verify
```

VM disks, screenshots, serial logs, keys, and transient state must stay under
`generated/test/`.
