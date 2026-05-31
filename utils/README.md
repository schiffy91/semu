# Semu Utilities

Utilities are implemented in `semu.btrc` and exposed through the `semu utils`
command group.

```sh
build/semu utils n3ds-nocrypto ROMs/n3ds --check
build/semu utils n3ds-nocrypto ROMs/n3ds -o ROMs/n3ds-fixed
```

`n3ds-nocrypto` only sets the NoCrypto flag on already-decrypted NCSD/NCCH
3DS dumps. It does not decrypt encrypted ROMs.
