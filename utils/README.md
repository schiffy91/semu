# Semu Utilities

Runtime utilities are implemented under `src/semu/utils/` and exposed through
the `semu utils` command group. The only shell utility left here is
`steam-deck-bootstrap.sh`, which handles SteamOS/Nix prerequisites and then
delegates product setup to the BTRC CLI.

```sh
build/semu utils n3ds-nocrypto ROMs/n3ds --check
build/semu utils n3ds-nocrypto ROMs/n3ds -o ROMs/n3ds-fixed
```

`n3ds-nocrypto` lives in `src/semu/utils/n3ds_*.btrc`, with the
`decrypt3ds` compatibility entrypoint in
`src/semu/utils/decrypt3ds_nocrypto.btrc`. It sets the NoCrypto flag on
already-decrypted NCSD/NCCH 3DS dumps. Use dumped/decrypted input files before
running it.

Steam Deck SSH smoke tests live under `tests/steam-deck/`.
