# SteamOS / Deck Verification

`tests/deck` contains Deck-facing test helpers. These checks sit above the
compiler: build the Semu CLI first, then run Deck or VM verification with the
same binary that `make steam-deck` and `make verify` use.

## Host Prep

```sh
make btrc-build
make compiler-verify
```

For a quick physical Deck SSH/screenshot preflight:

```sh
make deck-ssh-smoke DECK_HOST=steamdeck.local DECK_ROMS=/run/media/deck/SD
```

For the focused quit-watch path without launching emulator routes:

```sh
bash tests/deck/quit-watch-smoke.sh
```

On Linux, this uses a temporary synthetic input directory and verifies that
Select+Start terminates the launched process group. On non-Linux hosts it exits
as a skip.

## Emulator Loop

For the Desktop Mode direct emulator loop on a physical Deck, use an installed
AppImage plus a Nix-built `result` tree:

```sh
SEMU_APPIMAGE=/home/deck/Applications/Semu/Semu-x86_64.AppImage \
SEMU_PROJECT=/home/deck/.local/share/semu \
SEMU_ROMS=/run/media/deck/SD \
SEMU_RESULT=/home/deck/semu/result \
SEMU_TEST_OUT=/home/deck/.cache/semu-emulator-loop \
bash tests/deck/emulator-loop.sh
```

The loop compiles `tests/deck/uinput-send.c` when `cc` is available. Required
routes fail if launch, screenshot capture, input-induced visual change, or
structured quit-watch evidence is missing. Optional routes run only with
`SEMU_OPTIONAL_ROUTES=1`.

Renderer-required emulator routes also fail unless the first gameplay
screenshot differs from the pre-launch baseline and the route has explicit
Semu source-hook application evidence. Generated hook JSON is not enough.
Whole-window vkBasalt/gamescope/ReShade logs are not accepted as production
render proof. ES-DE and Semu Settings entries are intentionally excluded from
render verification.

When `SEMU_QUIT_WATCH_LOG` is not set, routed emulator launches write Semu-owned
evidence under `.semu/verification/quit-watch/<emulator>.log`. Use those files
for the physical Game Mode pass, where the AppImage is launched through Steam
and the left-trackpad radial menu is operated on the Deck itself.

## Guest Checks

Inside a Deck-like guest:

```sh
cd ~/semu
make compiler-build compiler-verify TEST_PROJECT="$PWD"
```

The VM target proves the compiler can build and verify the target definitions
on Linux after repo sync. A physical Steam Deck remains the required final check
for launcher routing, Syncthing behavior, Neptune trackpads, Steam Input radial
menus, and Game Mode return-to-ES-DE.

## Bazzite

For a heavier SteamOS-like VM:

```sh
make bazzite-vm-smoke
```

For a software-rendered installer/runtime smoke on Apple Silicon:

```sh
make bazzite-desktop-vm-smoke
```

The installed Bazzite E2E path is owned by the tests Makefile. It remasters the
Deck ISO with the test kickstart, installs into a fresh qcow2 disk, boots that
disk, syncs this repo over SSH, and runs the Deck checks inside the guest:

```sh
make bazzite-e2e-verify BAZZITE_E2E_SSH_PORT=2224
```

Set `BAZZITE_ISO_SHA256=<sha256>` to pin an exact ISO. When it is unset, the
harness verifies the downloaded ISO against Bazzite's checksum URL.
