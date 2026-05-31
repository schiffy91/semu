# SteamOS / Deck Verification

`test/deck` is the Deck-like full-system verification layer. It is intentionally
shell-thin; the state and behavior are declared in `schemulator.btrc`.

Generate the BTRC runtime on the host first, then run the thin guest scripts
inside SteamOS, Bazzite, or the Arch VM. The guest path compiles
`generated/schemulator.c`; it does not need the sibling `../btrc` checkout.

```sh
make btrc-build
make deck-vm-sync
```

Inside the guest:

```sh
cd ~/schemulator
test/deck/provision.sh "$PWD"
test/deck/verify-emulators.sh "$PWD"
test/deck/verify-sync.sh "$PWD"
test/deck/verify-input.sh "$PWD"
```

VMs can prove Linux packaging, Flatpak routing, Syncthing, BTRC bootstrap,
emulator launchers, routed sandbox preparation, and generated keymaps.
`make deck-vm-verify-strict` additionally fails when `/dev/uinput` or
`inputplumber` is missing. A physical Steam Deck is still the only trustworthy
final check for Neptune trackpads, Steam Input radial menus, and Game Mode
return-to-ES-DE.

## Bazzite Deck VM

For a heavier SteamOS-like VM, use the Bazzite Deck live ISO target:

```sh
make bazzite-vm-smoke
```

This downloads the current Bazzite Deck live ISO, boots it under QEMU TCG
software emulation, selects the ISO's Basic Graphics Mode, exposes VNC on
`127.0.0.1:5905`, and writes a framebuffer smoke screenshot to
`test/vms/bazzite-screen.ppm`. On macOS, inspect it with:

```sh
sips -s format png test/vms/bazzite-screen.ppm --out test/vms/bazzite-screen.png
```

Keep `VM_DIR` as a relative no-space path such as `test/vms`; GNU Make splits
target names on spaces, so an absolute path under `My Drive` is not a valid VM
artifact directory override.

After installing Bazzite, boot the installed disk without the ISO attached and
add this repo's SSH key to the guest user:

```sh
make bazzite-vm-start-installed
```

Then run:

```sh
make bazzite-vm-verify-ssh BAZZITE_SSH_USER=<guest-user>
```

For a more reliable no-GPU installer/runtime smoke on Apple Silicon, use the
Bazzite Desktop ISO variant:

```sh
make bazzite-desktop-vm-smoke
```

It uses the same QEMU/TCG harness, but exposes VNC on `127.0.0.1:5906` and
writes `test/vms/bazzite-desktop-screen.ppm`. SSH is forwarded to `2234` for
this variant to avoid colliding with the default Deck/Arch VM ports.

Set `BAZZITE_ISO_SHA256=<sha256>` to pin an exact ISO. When it is unset, the
harness verifies the downloaded ISO against Bazzite's current checksum URL.

Bazzite Deck is much closer to SteamOS than the fast Arch VM, but on Apple
Silicon it still cannot prove hardware-specific Neptune trackpad behavior or
GPU-accelerated Gamescope. Rosetta can translate x86_64 Linux userland inside
an arm64 Linux VM, but it cannot hardware-virtualize a full x86_64 SteamOS-like
guest; this target uses QEMU TCG/software rendering for that reason. It is
useful for Steam Gaming Mode boot behavior, Fedora Atomic/Bazzite runtime
assumptions, Syncthing/systemd user services, and our Linux launcher/profile
install path.
