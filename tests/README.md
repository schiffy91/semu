# Semu Test Harness

The fast test path is compiler-first:

```sh
make test
```

`make test` runs host payload, compiler validation, source-hook metadata,
rendering, generated settings, and input-contract checks without VM boot.

Steam Deck verification is the first full-system target. Use the Deck helpers
for SSH/screenshot preflight, emulator launch evidence, quit-watch logs, and
Game Mode proof. Bazzite remains useful parity infrastructure, but it runs only
after the physical Deck path is implemented and verified.

Cleanup policy:

- Keep tests that exercise the current `src/main.btrc`, `src/compiler`,
  `src/generators`, `src/lib`, `config/**`, and `build/**` layout.
- Delete specs or helpers that only call removed runtime commands, deleted
  root `generated/**` outputs, removed compatibility smoke targets, or deleted
  legacy test runners.
- Do not track payload artifacts under `tests/**`: VM disks, ISOs, AppImages,
  ROMs, BIOS, keys, saves, screenshots, and similar runtime outputs belong in
  ignored state such as `tests/vms`.
