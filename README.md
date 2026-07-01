# Semu

Semu is a Steam Deck focused emulation environment compiler. The repository
contains source policy, build packaging, tests, and small native helpers. It
does not contain ROMs, BIOS files, emulator runtime state, Deck screenshots, or
generated ES-DE project output.

The product contract lives in [docs/PRD.md](docs/PRD.md).

## Repository Shape

| Path | Role |
| --- | --- |
| `src/semu/**` | BTRC source for the Semu CLI, generators, launchers, settings, sync, tests, and verification. |
| `generated/semu.c` | Generated C snapshot used by Nix/package builds until the package builds BTRC directly. |
| `semu.json` | Generated manifest snapshot. |
| `settings/**`, `sync/sync.json`, `input/keymaps/**`, `verification/screenshots.json` | Editable source defaults. |
| `input/steam-input/**`, `packaging/linux/**` | Current package/input snapshots consumed by builds. |
| `packaging/deck-tap/**` | Small tap ABI and GL compositor prototype. |
| `packaging/standalone-bezel/**` | Standalone vkBasalt/ReShade bezel assets and wrappers. |
| `tests/**` | Host, AppImage, lifecycle, sandbox, Deck, and VM test entrypoints. |

Generated/local paths are ignored:

| Path | Role |
| --- | --- |
| `build/**` | Build outputs and generated packaging, including `build/packaging/es-de/**` and `build/packaging/emulators/profiles/**`. |
| `.semu/**` | Local Semu state and fallback content root, including `.semu/content/**`. |
| `ES-DE/**` | Runtime ES-DE data if ES-DE creates it locally. |
| `deck-shots/**` | Local screenshots and visual review captures. |
| `tests/vms/**` | VM disks, logs, screenshots, keys, and state. |

## Build

```sh
make btrc-build
make manifest
make test
```

`make btrc-build` transpiles `src/semu.btrc` to `build/semu.c`, compiles
`build/semu`, and refreshes `generated/semu.c`.

`make manifest` regenerates `semu.json` from the compiled CLI.

`make test` runs the deterministic host smoke path: payload audit, generated C
smoke tests, and AppImage assembly smoke with fakes.

## Bootstrap

```sh
build/semu bootstrap --project "$PWD"
```

Bootstrap writes local content under `.semu/content` and generated ES-DE project
files under `build/packaging/es-de`. It must not create a top-level `ES-DE`
project tree.

For a real Deck or SD card, pass an external ROM root:

```sh
build/semu lifecycle install --project "$PWD" --roms /run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
```

Semu still recognizes existing ES-DE SD-card layouts such as
`Emulation/ES-DE/ES-DE/ROMs`; it just does not generate that layout at the repo
root.

## Rendering Model

The renderer contract is intentionally small:

1. An emulator tap reports game-frame metadata: active flag, content rectangle,
   native size, output size, orientation, and backend origin.
2. Semu's compositor consumes that metadata and owns shader/bezel policy.
3. Emulator UI, menus, settings, and OSD are outside the tap and must present
   untouched.

Shader and bezel policy should not be copied into every emulator patch. If an
emulator integration grows into thousands of lines, the boundary is wrong:
reduce the patch to tap metadata and move rendering logic into the shared
compositor or generated assets.

## Branch Review Note

`codex/semu-production-deck-refactor` was reviewed as a stale, unmerged
production-readiness branch. It contains useful ideas around generated config
and proof contracts, but it also rewrites the tree, conflicts with current
`main`, and lacks current runtime proof. This cleanup does not merge that
branch wholesale.
