# Semu

Semu is a Steam Deck focused emulation environment compiler. The repository
contains source policy, package source, tests, and small native helpers. It does
not contain ROMs, BIOS files, emulator runtime state, Deck screenshots, VM
disks, or rendered package output.

The product contract lives in [docs/PRD.md](docs/PRD.md). The test contract
lives in [docs/testing.md](docs/testing.md).

## Repository Shape

Top-level folders are intentionally scarce:

| Path | Role |
| --- | --- |
| `.github/` | CI workflows. |
| `config/` | Source-of-truth policy: settings, sync profile, system defaults, keymaps, screenshot verification, and asset declarations. |
| `docs/` | Project docs that are still current. |
| `generated/` | The only repo-local output root. Only `generated/semu.c`, `generated/semu.json`, and `generated/.gitignore` are tracked. |
| `packaging/` | Package source for Linux, Nix, and Steam Deck tap/bootstrap work. |
| `src/` | BTRC source for the CLI, generators, launchers, settings, sync, and tests. |
| `tests/` | Host, graph, Steam Deck, and VM test entrypoints. |

Do not create root `build/`, `.semu/`, `ES-DE/`, `deck-shots/`, `result*`,
`input/`, `settings/`, `sync/`, `verification/`, `utils/`, `emulators/`, or
`tests/vms/`. Generated work belongs under `generated/`.

## Build

```sh
make btrc-build
make manifest
make test
```

`make btrc-build` writes `generated/build/semu.c`, compiles
`generated/build/semu`, and refreshes the temporary committed snapshot at
`generated/semu.c`.

`make manifest` writes `generated/semu.json`.

`make test` runs the tree audit, payload audit, generated-C smoke tests, and
AppImage assembly smoke with fakes.

## Bootstrap

```sh
generated/build/semu bootstrap --project "$PWD"
```

Bootstrap writes repo-local output under `generated/runtime/` and
`generated/packaging/`. External installs may still target real user locations,
such as `~/ES-DE/custom_systems` or an SD-card ROM tree, when explicitly
requested.

## Rendering Model

The renderer contract is small:

1. Emulator tap code reports game-frame metadata: active flag, content rectangle,
   native size, output size, orientation, and backend origin.
2. Semu owns shader and bezel policy from shared config/assets.
3. Emulator menus, settings, and OSD stay outside the tap and render untouched.

Shader and bezel logic should not be copied into every emulator patch. If an
integration grows into thousands of lines, the boundary is wrong: reduce the
patch to tap metadata and move rendering logic into shared compositor or asset
compiler code.
