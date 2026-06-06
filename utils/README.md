# Semu Utilities

Utilities are implemented under `src/semu/utils/` and exposed through the
`semu utils` command group.

```sh
build/semu utils n3ds-nocrypto ROMs/n3ds --check
build/semu utils n3ds-nocrypto ROMs/n3ds -o ROMs/n3ds-fixed
```

`n3ds-nocrypto` lives in `src/semu/utils/n3ds.btrc`. It also keeps the
`decrypt3ds` compatibility subcommand wired to the same implementation. The
tool sets the NoCrypto flag on already-decrypted NCSD/NCCH 3DS dumps. Use
dumped/decrypted input files before running it.

Steam Deck/SSH probes live under `tests/deck/` and are exposed through
`make deck-ssh-smoke`; `utils/steam-deck-bootstrap.sh` is only the thin
first-run shell wrapper that installs/loads Nix and delegates to the BTRC CLI.
