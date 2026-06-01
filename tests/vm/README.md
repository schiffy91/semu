# SteamOS / Deck Verification

`tests/vm` documents the Deck-like VM verification layer. The runtime commands
live in BTRC under `src/semu/`.

Generate the BTRC runtime on the host first, then run the Deck commands inside
SteamOS, Bazzite, or the Arch VM. The guest path compiles `generated/semu.c`
from this repo.

```sh
make btrc-build
make deck-vm-sync
```

Inside the guest:

```sh
cd ~/semu
build/semu deck provision --project "$PWD"
build/semu deck verify-emulators --project "$PWD"
build/semu deck verify-sync --project "$PWD"
build/semu deck verify-input --project "$PWD"
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
`tests/vms/bazzite-screen.ppm`. On macOS, inspect it with:

```sh
sips -s format png tests/vms/bazzite-screen.ppm --out tests/vms/bazzite-screen.png
```

Keep `VM_DIR` as a relative path such as `tests/vms`; GNU Make target parsing
works cleanly with path names that avoid spaces.

After installing Bazzite, boot the installed disk and add this repo's SSH key to
the guest user:

```sh
make bazzite-vm-start-installed
```

Then run:

```sh
make bazzite-vm-verify-ssh BAZZITE_SSH_USER=<guest-user>
```

For a more reliable software-rendered installer/runtime smoke on Apple Silicon,
use the Bazzite Desktop ISO variant:

```sh
make bazzite-desktop-vm-smoke
```

It uses the same QEMU/TCG harness, but exposes VNC on `127.0.0.1:5906` and
writes `tests/vms/bazzite-desktop-screen.ppm`. SSH is forwarded to `2234` for
this variant to avoid colliding with the default Deck/Arch VM ports.

Set `BAZZITE_ISO_SHA256=<sha256>` to pin an exact ISO. When it is unset, the
harness verifies the downloaded ISO against Bazzite's current checksum URL.

Bazzite Deck is much closer to SteamOS than the fast Arch VM. On Apple Silicon,
this target uses QEMU TCG/software rendering, which is useful for Steam Gaming
Mode boot behavior, Fedora Atomic/Bazzite runtime assumptions,
Syncthing/systemd user services, and the Linux launcher/profile install path.
A physical Deck remains the validation target for Neptune trackpads and
GPU-accelerated Gamescope.
