# FOR_CLAUDE.md

This file is a handoff for Claude Code or another coding agent continuing work
on Semu.

## User Intent

The user wants this project to become a production-quality, Steam Deck-first
emulation setup that feels as plug-and-play as RetroDeck while staying simple,
declarative, and editable.

The explicit direction from the user:

- Primary target: Steam Deck and SteamOS/Game Mode.
- Also work out of the box on Linux desktop.
- Runtime logic should be BTRC, using BTRC style and stdlib, not C-style code.
- Configuration should be declarative and UI-editable where possible.
- Controller support should be extensible to Steam Controller, Xbox,
  DualShock/DualSense, Nintendo, and future devices.
- Keymaps should compile like a tiny language: tokenizer, parser, code
  generation, and good errors.
- Verification should be test-first. Use programmatic tests where possible and
  visual screenshot checks where programmatic confidence is not enough.
- Avoid overengineering. KISS, DRY, elegant, modular, production-oriented.
- Do not reintroduce the old Python setup/symlink manager.

## Critical Constraints

- Canonical runtime source: `semu.btrc`.
- Generated artifacts:
  - `semu.json`
  - `generated/semu.c`
- Editable declarative user files:
  - `keymaps/steam_deck.skm`
  - `sync/sync.json`
  - `verification/screenshots.json`
- Runtime install/setup/reconfigure/change/uninstall/reinstall/upgrade logic
  belongs in BTRC.
- Linux/Nix/AppImage runtime paths must not depend on Python.
- `setup.py`, `setup.json`, per-emulator `symlinks.json`, and
  `generate_find_rules.py` were intentionally removed.
- Be careful with this worktree. It may already be dirty. Do not revert user
  changes or broad unrelated modifications.
- Semu has a `btrc` flake input and a local `.#btrcpy` wrapper package.
  `make btrc-build` defaults to the flake-pinned compiler. Use
  `BTRC_FLAKE=path:/absolute/path/to/btrc` or `BTRC_USE_FLAKE=0` only when
  testing unpublished local compiler changes. Normal Nix package builds compile
  `generated/semu.c` and do not need the BTRC compiler checkout.

## Current Architecture

`semu.btrc` owns:

- Manifest generation.
- ES-DE system catalog and command rendering.
- BIOS requirement declarations.
- Controller model declarations.
- Steam Deck controller profile generation.
- Steam Input template generation.
- Keymap language parser/compiler/renderers.
- Steam Deck bootstrap/doctor.
- Lifecycle commands.
- Syncthing setup/status/force/autostart/tray/open.
- Screenshot setup/status/capture and launcher hooks.
- Linux sandbox preparation.
- Linux launcher and Flatpak routing.
- BTRC-native E2E smoke tests.

Nix owns:

- Emulator packages.
- `semu` CLI package.
- Routed emulator wrapper packages.
- Full default bundle.
- NixOS module.

Linux/AppImage glue owns:

- Thin launcher scripts in `linux/bin`.
- AppImage entrypoint in `linux/AppRun`.
- AppImage assembly in `linux/build-appimage.sh`.

## Important Files

| File | Role |
|---|---|
| `semu.btrc` | Main runtime implementation. |
| `semu.json` | Generated manifest for UI/runtime/tooling. |
| `generated/semu.c` | Generated C snapshot for Nix builds. |
| `keymaps/steam_deck.skm` | Editable Steam Deck keymap source. |
| `sync/sync.json` | Editable Syncthing policy. |
| `verification/screenshots.json` | Editable screenshot hook policy. |
| `flake.nix` | Nix package/app/check outputs. |
| `nix/semu-cli.nix` | BTRC CLI package. |
| `nix/semu.nix` | Full bundle package. |
| `nix/routed-emulator.nix` | Per-emulator state-routing wrapper. |
| `linux/AppRun` | AppImage entrypoint. |
| `linux/build-appimage.sh` | AppImage assembly script. |
| `test/E2E.md` | Verification matrix and remaining gaps. |
| `README.md` | Detailed user/developer documentation. |

## Recent Cleanup

The legacy Python setup path was removed:

- `setup.py`
- `setup.json`
- `generate_find_rules.py`
- per-emulator `symlinks.json`
- stale `docs/phase*.md`
- old `test/test_setup.py`
- old `test/test_commands.py`

Tests now target the BTRC manifest/bootstrap/doctor/keymap/sync/AppImage paths
instead of the old setup/symlink manager.

Packaging was updated so Nix no longer copies `setup.json` or `symlinks.json`.
`linux/AppRun` now discovers projects by `semu.json` or ES-DE generated
files, not `setup.json`.

## BTRC Repo Status

During this handoff, the BTRC compiler/stdlib import-visibility work was pushed
to `schiffy91/btrc` and Semu was updated to consume BTRC through its
flake input. Semu now has a `btrc` flake input and a `.#btrcpy` package,
and `make btrc-build` uses the flake-pinned compiler by default. Override with
`BTRC_FLAKE=path:/absolute/path/to/btrc` or `BTRC_USE_FLAKE=0` only when testing
unpublished compiler changes.

Submodule recommendation: avoid adding BTRC as a submodule for now. Generated C
keeps normal Semu builds independent from the compiler checkout, and a
submodule would add nested-git friction for Steam Deck users. Prefer the pinned
flake input. Revisit a submodule only if offline compiler development inside
this repo becomes a hard requirement.

## BTRC Refactors Already Applied

- Reused `controllerProfileFiles()` in controller profile manifest generation.
- Removed an unused ES-DE settings helper.
- Consolidated ES-DE settings XML path rendering.
- Added `syncFolderIds()` and reused it across sync setup/status/doctor loops.
- Made launcher doctor checks loop over `linuxLauncherNames()`.
- Added `SEMU_FLATPAK_X11` as the preferred env var while keeping
  `SEMU_FLATPAK_X11` as fallback compatibility.
- Kept `std.map` import because the generated JSON support still needs it even
  though map usage is not obvious at source level.

## Verification Commands

Use these from the repo root:

```sh
make btrc-build
build/semu manifest --output semu.json
build/semu screenshot setup --project .
python3 -m pytest test/ -v
build/semu e2e all
bash test/appimage/smoke.sh
make nix-e2e
nix build .#default --print-build-logs
make verify
git diff --check
```

If time is limited, the minimum useful set is:

```sh
make verify
nix build .#default --print-build-logs
git diff --check
```

For VM/Deck-style validation:

```sh
make deck-vm-verify
make deck-vm-verify-strict
make bazzite-vm-smoke
make bazzite-desktop-vm-smoke
```

## Known Remaining Gaps

Physical Steam Deck:

- Verify Neptune controls in Game Mode.
- Verify Steam Input template visibility.
- Verify left trackpad radial menu.
- Verify right trackpad mouse behavior.
- Verify save/load/quit/menu/pause inside each emulator.
- Verify quit returns to ES-DE.
- Verify screenshot hook images show actual emulator windows under Gamescope.

Real AppImage on SteamOS:

- Build with a real ES-DE AppImage.
- Include `--nix-package result`.
- Launch on Steam Deck.
- Confirm bundled Nix-store mount works.
- Confirm ES-DE opens.
- Confirm ROM location override persists.
- Confirm routed launchers start real emulators.
- Confirm Syncthing commands work from the installed artifact.

Installed Bazzite:

- Complete an installed Bazzite VM pass, not only live ISO/splash/desktop checks.
- Run the Deck verification scripts over SSH against the installed guest.

Syncthing:

- Current tests cover config, units, local service/API, and force rescans.
- Still needed: pair a second real device and test conflict/resolution behavior.

UI:

- No UI editor exists yet for:
  - `keymaps/steam_deck.skm`
  - `sync/sync.json`
  - `verification/screenshots.json`
  - ROM location and BIOS status
- Build UI against `semu.json` rather than duplicating catalog data.

Python:

- `decrypt3ds.py` remains a Python utility for setting NoCrypto flags on
  already-decrypted 3DS dumps.
- If the repo must become strictly zero-Python outside tests, port this utility
  to BTRC or remove it after replacing its behavior.

Shell:

- Shell remains for thin launchers, AppRun, AppImage assembly, and test scripts.
- Do not move complex runtime decisions into shell. Keep shell as glue.

## BTRC Language Gaps Worth Considering

Only address these in the BTRC language if they remove real friction:

- Structured XML builder helpers. ES-DE XML is currently string-generated.
- Better argv-style process APIs. Some code still uses shell strings plus
  `ShellWords.quote`.
- First-class TOML read/write if future emulator configs need TOML editing.
- Binary read/write helpers if `decrypt3ds.py` is ported to BTRC cleanly.
- Pretty JSON write support for generated user-editable config files.
- Native file glob/sorted-directory helpers for deterministic catalog scans.
- Image/screenshot validation helpers, such as nonblank PNG checks, if visual
  tests become BTRC-native.

Do not add language features just to make the code look abstract. The current
project values simple BTRC code over cleverness.

## Style Guidance

For `semu.btrc`:

- Prefer existing helper patterns.
- Keep data declarations close to their renderers.
- Use vectors and explicit structs/classes where the current code does.
- Avoid C-ish manual memory or raw stdio unless BTRC has no higher-level API.
- Validate before rendering or writing.
- Generate all derived artifacts from canonical data.
- Keep author-facing errors concrete and line/column based where possible.

For tests:

- Add tests at the behavior boundary.
- Prefer BTRC `e2e` checks for runtime behavior.
- Use Python tests only as harnesses around the BTRC binary or standalone tools.
- Use fake binaries for deterministic launcher/AppImage checks.
- Use visual screenshots for SteamOS/Gamescope surfaces that cannot be proven
  headlessly.

## Do Not Reintroduce

Do not bring back:

- `setup.py`
- `setup.json`
- per-emulator `symlinks.json`
- `generate_find_rules.py`
- Python as a Linux/Nix/AppImage runtime dependency
- hidden mutable config that is not represented in `semu.btrc`,
  `semu.json`, `keymaps/steam_deck.skm`, `sync/sync.json`, or
  `verification/screenshots.json`

## Suggested Next Tasks

1. Run the full verification matrix after this cleanup and update this file if
   any command fails.
2. Port or retire `decrypt3ds.py` if strict zero-Python outside tests is still a
   hard requirement.
3. Build a small config UI that edits `keymaps/steam_deck.skm`, `sync/sync.json`,
   screenshot hooks, ROM path, and BIOS status using `semu.json`.
4. Complete a real Steam Deck Game Mode pass and attach screenshots under
   `ES-DE/ES-DE/screenshots/verification/`.
5. Complete a real AppImage pass on SteamOS.
6. Add more routed-wrapper checks for real emulator startup once the SteamOS
   environment is available.
