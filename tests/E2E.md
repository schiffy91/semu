# E2E Status

Semu is Steam Deck-first, but the verification layers have different
confidence levels.

## Automated Today

- `make e2e-graph-run`: declarative JSON graph runner that composes the fast
  host tests, records content-addressed state under `tests/vms/e2e-state`, and
  skips nodes whose source/material hash is already ready. The default graph runs
  `payload-audit`, `generated-smoke`, and `appimage-smoke`.
- `make e2e-graph-status`, `make e2e-graph-list`, and
  `make e2e-graph-coverage`: graph inspection and operation-coverage checks.
- `make verify`: BTRC manifest determinism, keymap compiler/renderers, doctor
  invariants, launcher shim syntax, BTRC runtime source guard, BTRC sandbox
  preparation, BTRC Linux launcher routing, BTRC lifecycle install/reconfigure/
  change/uninstall/reinstall/upgrade smoke, fake AppImage assembly with bundled
  Nix-store mount wiring, routed Nix wrapper behavior, generated-C smoke, and
  the BTRC 3DS NoCrypto utility.
- `make e2e-smoke`: BTRC-native smoke for sandbox symlink specs, emulator
  launcher routing, declarative screenshot launch hooks, and lifecycle state
  transitions.
- `make deck-vm-verify`: Arch Linux VM provisioning, emulator preflight,
  BTRC sandbox/launcher route smokes, Syncthing setup/API force sync,
  screenshot hook status, `/dev/uinput`, and controller-route checks.
- `make deck-vm-verify-strict`: same VM path as `deck-vm-verify`, but fails
  instead of warning when `/dev/uinput` or `inputplumber` is absent.
- `make nix-e2e`: flake eval for Linux routed packages/apps plus a host-native
  Nix check that executes a mock routed RetroArch wrapper and verifies
  `HOME`/`XDG_*`, seed copying, and RetroArch `--config` injection.
- `make appimage-smoke`: fake ES-DE/fake appimagetool AppImage assembly check
  that proves `--nix-package`, copied routed launchers, AppRun bubblewrap
  `/nix/store` mount args, CLI passthrough, stable launch-time filesystem,
  normal AppRun ES-DE launch, and `SEMU_LAUNCHER_BIN` bootstrap output.
- `make bazzite-desktop-vm-smoke`: Bazzite Desktop live ISO under QEMU TCG with
  checksum verification, Basic Graphics boot, and high-threshold framebuffer
  validation of the KDE live environment.
- `make bazzite-vm-smoke`: Bazzite Deck ISO boot path under QEMU TCG. This
  validates firmware/GRUB/live-image bootability through the software-rendered
  Deck splash path.

## Graph Usage

The graph lives at `tests/e2e/graph.json`; specs live under
`tests/e2e/specs/*.json`.

```sh
make e2e-graph-list
make e2e-graph-status
make e2e-graph-coverage
make e2e-graph-run
make e2e-graph-run E2E_GRAPH_NODES="verify"
make e2e-graph-run E2E_GRAPH_NODES="arch-deck-vm"
make e2e-graph-run E2E_GRAPH_NODES="bazzite-installed-ssh" \
  E2E_GRAPH_ARGS="--arg bazziteUser=<guest-user>"
```

The graph intentionally delegates to the existing Make targets instead of
reimplementing QEMU or SteamOS behavior inside BTRC. It adds dependency ordering,
state hashes, skip/resume behavior, operation coverage, and a payload audit that
fails if tracked ROMs, BIOS, emulator runtime directories, VM disks, or similar
licensed artifacts would be upstreamed.

## Next Verification Passes

- Physical Steam Deck pass: Neptune trackpads, Steam Input template visibility,
  left-trackpad radial menu, unified save/load/quit hotkeys inside each emulator,
  screenshot contents from real Gamescope emulator windows, and quit returning
  to ES-DE in Game Mode.
- Installed Bazzite pass: install to `tests/vms/bazzite*.qcow2`, boot with
  `make bazzite-vm-start-installed`, SSH in, and run
  `make bazzite-vm-verify-ssh BAZZITE_SSH_USER=<guest-user>`.
- Real AppImage pass on SteamOS: build `Semu-*.AppImage` with
  `packaging/linux/build-appimage.sh --nix-package result`, launch it on a Deck, verify
  ES-DE opens under Gamescope, ROM location override persists, Syncthing
  commands work, and routed launchers start real emulator binaries.
- True multi-device Syncthing: current tests cover config, systemd units, local
  service/API, and force-rescan commands. A second real device pass should
  verify conflict/resolution behavior.
- User-owned BIOS coverage: doctor declares required BIOS/firmware and reports
  missing files. The PSX, PS2, and Switch BIOS/key checks use user-provided
  files.

## AppImage Scope

The AppImage path bundles ES-DE, AppRun, the compiled BTRC `semu` CLI,
ES-DE find rules, launcher scripts, and optionally a Nix closure copied into
the AppDir with `--nix-package`. ROMs and BIOS remain user-owned. Flatpak
launchers are the host-backed route for systems outside the routed Nix payload.

Linux flake outputs now expose routed Nix emulator wrappers:

- `.#semu-retroarch`
- `.#semu-dolphin`
- `.#semu-ppsspp`
- `.#semu-flycast`
- `.#semu-gopher64`
- `.#semu-melonds`
- `.#semu-pcsx2`
- `.#semu-cemu`
- `.#semu-azahar`
- `.#semu-ryujinx`
- `.#semu-es-de`
- `.#semu-routed-emulators`

These wrappers avoid host symlinks by routing emulator state through
`HOME`/`XDG_*` into `.semu/appimage-state`. AppRun can mount a bundled
Nix closure at `/nix/store` with bubblewrap. Local smoke tests now cover the
assembly and routing design; the remaining gap is executing the final AppImage
against real SteamOS/Gamescope/emulator binaries.
