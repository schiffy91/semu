# SteamOS / Deck Verification

`tests/deck` documents the Deck-like full-system verification layer. The
runtime commands live in BTRC under `src/semu/`.

Generate the BTRC runtime on the host first, then run the Deck commands inside
SteamOS, Bazzite, or the Arch VM. The guest path compiles `generated/semu.c`
from this repo.

```sh
make btrc-build
make deck-vm-sync
```

For a quick physical Deck SSH/screenshot preflight, run:

```sh
make deck-ssh-smoke DECK_HOST=steamdeck.local DECK_ROMS=/run/media/deck/SD
```

For the Desktop Mode direct emulator loop on a physical Deck, use the installed
AppImage and a Nix-built `result` tree:

```sh
SEMU_APPIMAGE=/home/deck/Applications/Semu/Semu-x86_64.AppImage \
SEMU_PROJECT=/home/deck/semu-latest \
SEMU_ROMS=/run/media/deck/SD \
SEMU_RESULT=/home/deck/semu-latest/result \
SEMU_TEST_OUT=/home/deck/.cache/semu-emulator-loop-cad8b59 \
bash tests/deck/emulator-loop.sh
```

The loop compiles `tests/deck/uinput-send.c` when `cc` is available and uses it
to send a small gameplay probe plus Select+Start quit. Required routes fail the
script if launch, screenshot capture, or structured quit-watch evidence fails.
Each case writes `$SEMU_TEST_OUT/<system>.quit-watch.log`; a successful Semu
quit adapter observation contains a quit-watch line with `reason=<chord>`, such
as `terminate child=... reason=select+start`. Alternate routes, such as
RetroArch DeSmuME for DS when melonDS is already the required DS route, run only with
`SEMU_OPTIONAL_ROUTES=1`.

When `SEMU_QUIT_WATCH_LOG` is not set, routed emulator launches write Semu-owned
evidence under `.semu/verification/quit-watch/<emulator>.log`. Use those files
for the physical Game Mode pass, where the AppImage is launched through Steam
and the left-trackpad radial menu is operated on the Deck itself. Run the
readiness gate before switching to Game Mode:

```sh
semu deck game-mode-ready --project /home/deck/semu-latest --prepare
```

That command writes `.semu/verification/game-mode-readiness.json` and fails if
the session is only Desktop Mode, Steam is not running, the Steam shortcut is
missing, the installed AppImage is missing/non-executable, or the physical
checklist has not been prepared. Use `--allow-desktop` only as an SSH preflight
escape hatch for Desktop Mode or mixed Desktop/Gamescope sessions; the final
proof should run in Game Mode without it. After the physical pass, require both
readiness and evidence:

```sh
semu deck game-mode-ready --project /home/deck/semu-latest --require-evidence
semu deck game-mode-evidence --project /home/deck/semu-latest
```

`game-mode-evidence` fails any route that lacks a `reason=` event.

The current installed AppImage smoke on the physical Deck is:

- commit: `3dd0c0a`
- path: `/home/deck/Applications/Semu/Semu-x86_64.AppImage`
- sha256:
  `8763f1bb950c908ca8a0687c18c5c961be36f7f3423f07291afcf4372da0d8ac`
- backup:
  `/home/deck/Applications/Semu/Semu-x86_64.AppImage.prev-3dd0c0a-20260612-035730`
- verified: installed-path `e2e deck-evidence`, `e2e appimage`,
  `deck game-mode-ready --prepare`, `deck game-mode-ready --prepare
  --allow-desktop`, `deck game-mode-evidence --allow-pending`, GB presentation
  asset resolution, app.quit capability report, and bundled `semu-quit-watch`
  start/exit evidence
- pending: physical Game Mode left-trackpad radial quit/save/load proof

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
return-to-ES-DE. The Game Mode pass should preserve the same quit-watch
evidence files so physical radial behavior is compared against the Desktop Mode
adapter contract instead of inferred from screenshots alone.

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

The installed Bazzite E2E path is owned by the graph. It remasters the Deck ISO
with the test kickstart, installs into a fresh qcow2 disk, boots that disk, syncs
this repo over SSH, and runs the Deck checks inside the guest:

```sh
make e2e-graph-run E2E_GRAPH_NODES="bazzite-installed-ssh" \
  E2E_GRAPH_ARGS="--arg bazziteSshPort=2224"
```

The graph-created guest user is `bazzite`. To inspect the installed VM after a
run, start it and open SSH:

```sh
make bazzite-vm-start-installed \
  BAZZITE_DISK=tests/vms/bazzite-e2e.qcow2 \
  BAZZITE_PID=tests/vms/bazzite-e2e.pid \
  BAZZITE_MONITOR=tests/vms/bazzite-e2e-monitor.sock \
  BAZZITE_SERIAL=tests/vms/bazzite-e2e-serial.log \
  BAZZITE_SSH_PORT=2224 \
  BAZZITE_VNC_DISPLAY=7
make bazzite-vm-ssh BAZZITE_SSH_USER=bazzite BAZZITE_SSH_PORT=2224
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
