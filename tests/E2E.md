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
  launcher routing, declarative screenshot launch hooks, the dependency-free
  settings UI, the per-system presentation matrix, and lifecycle state
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
  checksum verification, Basic Graphics boot, and nonblank framebuffer
  validation of the live boot path.
- `make bazzite-vm-smoke`: Bazzite Deck ISO boot path under QEMU TCG. This
  validates firmware/GRUB/live-image bootability through the software-rendered
  Deck splash path.
- `build/semu e2e graph tests/e2e/graph.json run bazzite-installed-ssh`:
  graph-owned Bazzite Deck VM installation, installed-disk boot, SSH sync, and
  Deck verification inside the guest.

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
  E2E_GRAPH_ARGS="--arg bazziteSshPort=2224"
```

The graph intentionally delegates to the existing Make targets instead of
reimplementing QEMU or SteamOS behavior inside BTRC. It adds dependency ordering,
state hashes, skip/resume behavior, operation coverage, and a payload audit that
fails if tracked ROMs, BIOS, emulator runtime directories, VM disks, or similar
licensed artifacts would be upstreamed.

The Bazzite install node remasters the downloaded Deck ISO with a test
kickstart, creates a fresh qcow2 disk, runs the installer, and accepts the node
only after the VM powers off cleanly and the installed disk has real allocated
content. The SSH node boots that disk and runs the Deck provision/emulator/sync/
input checks through the same BTRC CLI used on a physical Deck.

## Next Verification Passes

- Physical Steam Deck Game Mode pass: Neptune trackpads, Steam Input template
  visibility, left-trackpad radial menu, unified save/load/quit hotkeys inside
  each emulator, screenshot contents from real Gamescope emulator windows, and
  structured quit-watch evidence plus return-to-ES-DE in Game Mode.
- Steam launch metadata pass: run `build/semu steam-input status --project
  "$PWD"` on the Deck and verify both the reported `steam://rungameid/...` URI
  and the Semu Neptune template selection before starting the physical Game Mode
  pass.
- Broad real-emulator Game Mode pass: repeat the representative routed-emulator
  loop from Game Mode so Steam Input, Gamescope, and return-to-ES-DE behavior
  are proven outside Desktop Mode.
- Input and settings pass: compare Semu's bottom-left radial behavior against
  RetroArch-native save/load/quit/menu handling, then verify the generated
  Semu Settings ES-DE entries for ROM location, Syncthing, shader, bezel, and
  reconfigure actions.
- Game Mode readiness pass: run `semu deck game-mode-ready --prepare` before
  the physical pass, then `semu deck game-mode-ready --require-evidence` after
  the pass so session, Steam shortcut, selected Steam Input template, AppImage,
  checklist, and quit evidence are checked together.
- Visual pass after input is solid: verify `settings/presentation/*.json`
  resolves the intended shader/bezel for every system, prove RetroArch native
  presets first, then test standalone emulator wrapper experiments with
  gamescope ReShade or vkBasalt behind feature flags and screenshot evidence.
- True multi-device Syncthing: current tests cover config, systemd units, local
  service/API, and force-rescan commands. A second real device pass should
  verify conflict/resolution behavior.
- User-owned BIOS coverage: doctor declares required BIOS/firmware and reports
  missing files. The PSX, PS2, and Switch BIOS/key checks use user-provided
  files.

## Physical Deck Evidence

The SteamOS/AppImage path has been smoke-tested on a physical Steam Deck in
Desktop Mode with the AppImage built from the Nix package closure:

- ES-DE launched fullscreen at the Deck's 1280x800 display resolution.
- `/run/media/deck/SD` was auto-normalized to the ES-DE ROM directory.
- AppImage-owned `sync setup`, `sync serve`, and `sync force all` worked, with
  Syncthing running from the mounted AppImage payload rather than a host package.
- `doctor` reported Steam Deck defaults, screenshot tooling, Syncthing, and
  Linux launchers as present.
- A Game Boy title launched through the bundled RetroArch/Gambatte route and
  returned to ES-DE after a window-close command.
- SSH access, passwordless sudo, Desktop Mode display environment discovery,
  and Spectacle screenshots were verified from `deck@steamdeck.local`.
- Deck panel brightness was lowered programmatically through
  `/sys/class/backlight/amdgpu_bl0/brightness` for unattended OLED safety.
- A broad Desktop Mode required-route pass launched representative SD-card ROMs
  through RetroArch GB/GBC/GBA/NES/SNES/Genesis/N64, PPSSPP, Flycast, Dolphin,
  PCSX2, melonDS, Azahar, Cemu, and Ryujinx. Every required route returned
  `status=0`, `quit=ok`, and a 1280x800 nonblank screenshot. New passes keep
  per-system `$SEMU_TEST_OUT/<system>.quit-watch.log` files and require a
  quit-watch `reason=<chord>` event for Semu launcher-layer quit evidence.
- The current broad pass used the installed `04a4b79` AppImage and produced
  real visible frames for the required systems, with evidence under
  `/home/deck/semu-evidence/emulator-loop-04a4b79` and a local contact sheet at
  `/tmp/semu-deck-evidence/emulator-loop-contact.jpg`.
- The installed `04a4b79` AppImage strict visual audit resolved every required
  shader, bezel, runtime preset, launcher shader, and transitive sidecar
  dependency from the mounted AppImage payload: 15 OK, 0 missing assets, 0
  missing dependencies, Wii U/Switch disabled by default.
- Commit `43c070b` added structured quit-watch evidence. On the physical Deck,
  the Nix result watcher observed an injected `/dev/uinput` Select+Start event,
  logged `reason=select+start`, terminated the child process, and
  exited cleanly. The current installed AppImage was rebuilt from commit
  `04a4b79` with hash
  `d00df401164f38172cabbe9657cecb779967b3fce4f06e1d8b9ef59b391c86cf`; its
  packaged `e2e appimage` smoke passed inside the AppImage runtime, GB
  presentation asset resolution reported shader/bezel/runtime preset `ok`, and
  installed-path `e2e deck-evidence`, `deck game-mode-ready --prepare`,
  `deck game-mode-ready --prepare --allow-desktop`,
  `deck game-mode-evidence --allow-pending`, and
  `keymap capabilities app.quit` all ran from
  `/home/deck/Applications/Semu/Semu-x86_64.AppImage`.
- The optional RetroArch DeSmuME DS route is not production-ready on the Deck
  loop; an earlier optional pass produced a screenshot but exited `status=139`.
  melonDS is the required DS route.
- Earlier passes exposed production gaps around PCSX2 and Cemu setup prompts,
  Azahar Vulkan defaults, Ryujinx SD-card keys, and the removed Gopher64 route.

This evidence does not prove the physical left-trackpad radial menu because SSH
and X11 key injection cannot generate the Steam Input/evdev events consumed by
the quit watcher. That remains a physical Game Mode check, now audited with
`semu deck game-mode-ready --require-evidence` and
`semu deck game-mode-evidence`.

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
- `.#semu-melonds`
- `.#semu-pcsx2`
- `.#semu-cemu`
- `.#semu-azahar`
- `.#semu-ryujinx`
- `.#semu-es-de`
- `.#semu-routed-emulators`

These wrappers avoid host symlinks by routing emulator state through
`HOME`/`XDG_*` into `.semu/appimage-state`. AppRun can mount a bundled
Nix closure at `/nix/store` with bubblewrap. Local smoke tests cover assembly
and routing; physical Deck smoke covers ES-DE, Syncthing, SD-card detection,
the broad Desktop Mode required-route emulator loop, and the installed AppImage
runtime at commit `ece63b0`. Steam shortcut discovery and Semu Neptune Steam
Input template selection are verified from the installed AppImage. The remaining
gap is the broad physical Game Mode emulator pass using the left-trackpad radial
menu.
