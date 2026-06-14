# E2E Status

Semu is moving to compiler-first verification. Fast host checks should exercise
the declarative `config/**` definitions through the CLI that the root Makefile
uses; VM and physical Deck checks stay as heavier infrastructure around that
compiler surface.

## Fast Host Checks

- `make test`: runs the payload audit and `compiler-verify`.
- `make compiler-build`: runs `semu build target steam-deck` against the repo
  definitions with test-local project state.
- `make compiler-verify`: runs `semu verify target steam-deck` against the repo
  definitions.
- `make verify`: root Makefile compiler verification for the configured
  project.

The host checks intentionally no longer build committed generated C or call
removed compatibility smoke targets. Generated packaging belongs under
`build/**`; Nix and AppImage behavior should be covered by compiler generators
and heavier full-system checks as those replacements land.

## Full-System Layers

- `make deck-ssh-smoke`: probes a physical Deck over SSH and captures baseline
  display artifacts without touching ROM contents.
- `make bazzite-vm-smoke`: boots the Bazzite Deck live ISO under QEMU TCG and
  validates a nonblank framebuffer.
- `make bazzite-desktop-vm-smoke`: uses the Bazzite Desktop ISO for the same
  software-rendered boot smoke.

Physical Deck Game Mode remains the final check for Neptune trackpads, Steam
Input radial menus, save/load/quit hotkeys, and return-to-ES-DE behavior.

## Payload Policy

`tests/vms` is ignored and must stay ignored. VM disks, ISO images, AppImages,
ROMs, BIOS files, keys, saves, screenshots, and similar payload artifacts must
not be tracked under `tests/**` or `utils/**`.
