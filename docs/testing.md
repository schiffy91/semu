# Testing

## Host

Run the normal local path:

```sh
make test
```

This runs:

- `tree-audit`: fails if stale root output directories or generated tracked
  files outside approved snapshots exist.
- `payload-audit`: fails if licensed payloads or VM artifacts would be
  upstreamed.
- `tap-geometry-smoke`: compiles the Steam Deck tap geometry contract and
  checks game-priority, bezel-priority, and fill-hole cutout alignment.
- `tap-menu-smoke`: compiles the Steam Deck tap radial menu contract and
  checks Rendering, Save/Load, dual-screen, and Wii/Wii U controller options.
- `generated-smoke`: builds from committed `generated/semu.c` and runs BTRC
  smoke tests, including the render-options contract.
- `appimage-smoke`: assembles an AppImage with fake ES-DE, fake Nix, and fake
  appimagetool.

Run the fuller deterministic verification path:

```sh
make verify
```

To run only the compositor option contract check:

```sh
generated/build/semu e2e render-options --project "$PWD"
```

To prepare or verify the visual proof matrix:

```sh
generated/build/semu deck visual-evidence --project "$PWD" --prepare
generated/build/semu deck visual-evidence --project "$PWD"
```

To capture the current live emulator surface for one system, launch that system
with the same `SEMU_TAP_STATE_DIR`, then run:

```sh
SEMU_TAP_STATE_DIR="$PWD/generated/test/tap-state" \
  generated/build/semu deck visual-evidence gb --capture --allow-pending \
  --project "$PWD" --tap-state-dir "$PWD/generated/test/tap-state"
```

The capture command can also own the launch lifecycle when given an explicit
command template:

```sh
generated/build/semu deck visual-evidence gb --capture --allow-pending \
  --launch-command 'generated/build/semu launcher retroarch -L "$CORE" "$ROM"' \
  --launch-wait-seconds 10 --project "$PWD"
```

When `--rom` is supplied, Semu can derive the launch command from the system
catalog. Use `--print-launch-command` to inspect the resolved command without
starting the emulator:

```sh
generated/build/semu deck visual-evidence gb \
  --rom "$PWD/generated/runtime/content/ROMs/gb/game.gb" \
  --print-launch-command --project "$PWD"
```

To capture every declared system in one run, put representative ROMs under the
normal ES-DE system folders and use `--capture-all`:

```sh
SEMU_TAP_STATE_DIR="$PWD/generated/test/tap-state" \
  generated/build/semu deck visual-evidence --capture-all --allow-pending \
  --rom-root "$PWD/generated/runtime/content/ROMs" \
  --project "$PWD" --tap-state-dir "$PWD/generated/test/tap-state"
```

Each system's `start-of-gameplay-analysis.txt` must include the required
tokens emitted in `generated/test/visual-evidence/VISUAL_EVIDENCE.md`,
including emulator, alignment, cutout, input, radial-menu, bezel, shader, and
system-specific checks.

To cross-compile the Steam Deck GL tap preload:

```sh
make tap-preload-build
```

The tap compositor reads and writes its live override files from `/home/deck`
on the device. Local and VM evidence runs can redirect those files into the
generated test tree. Set `SEMU_SCREENSHOT_CMD` or `SEMU_VISUAL_CLIP_CMD` when
the host lacks `grim`/`spectacle`/`wf-recorder`/`ffmpeg` or needs a specific
capture tool:

```sh
SEMU_TAP_STATE_DIR="$PWD/generated/test/tap-state" generated/build/semu deck visual-evidence --project "$PWD"
```

## Graph E2E

```sh
make e2e-graph-list
make e2e-graph-status
make e2e-graph-coverage
make e2e-graph-run
```

Graph specs live in `tests/e2e/`. Graph state and VM/test artifacts belong
under `generated/test/`.

## Steam Deck

Physical Deck helpers live in `tests/steamdeck/`:

```sh
tests/steamdeck/ssh-smoke.sh
generated/build/semu deck provision --project "$PWD"
generated/build/semu deck verify-emulators --project "$PWD"
generated/build/semu deck verify-sync --project "$PWD"
generated/build/semu deck verify-input --project "$PWD"
```

The Steam Deck bootstrap script is `packaging/steamdeck/bootstrap.sh`.

## VM

Linux VM targets use `generated/test/vms/`:

```sh
make deck-vm-start
make deck-vm-verify
make bazzite-vm-smoke
make bazzite-e2e-verify
```

VM disks, screenshots, serial logs, keys, and transient state must stay under
`generated/test/`.
