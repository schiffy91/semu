# Linux / Steam Deck Launcher

This tree is the Steam Deck-first runtime path for Semu.

- `AppRun` finds the cloud-synced Semu project, mirrors the
  project-generated ES-DE config into `~/ES-DE/custom_systems`, and launches
  bundled ES-DE. Setup is explicit: run `deck install`/`lifecycle install`
  before the first normal launch.
- If the AppImage was built with `--nix-package`, `AppRun` mounts the bundled
  closure at `/nix/store` with bubblewrap and routes ES-DE find rules to the
  Nix-built `semu-*` launchers in `usr/bin`.
- `bin/semu-*` launchers are stable ES-DE entry names. They are thin shims
  into `semu launcher ...`; emulator IDs, Flatpak IDs, X11/Wayland
  policy, ROM/project grants, and RetroArch routing live in `src/semu.btrc`.
- `sandbox.sh` is a compatibility shim into `semu sandbox launch`.
  Bubblewrap composition and scratch symlink preparation are handled in BTRC.
- Screenshot verification hooks are declared in `semu.json` and editable
  through `verification/screenshots.json`. When
  `SEMU_SCREENSHOT_HOOKS=1` is set, emulator launchers capture
  `before_launch`, delayed `after_spawn`, and `after_exit` images under
  `ES-DE/ES-DE/screenshots/verification`.

Typical project setup:

```sh
make btrc-build
build/semu deck install --project "$PWD" --roms "$PWD/ES-DE/ES-DE/ROMs"
build/semu doctor --project "$PWD"
build/semu screenshot status --project "$PWD"
```

On Steam Deck, pass an external SD card path to `--roms` if desired, for
example `/run/media/mmcblk0p1/Emulation/ROMs`. The choice is stored in
`sync/sync.json`, which is intentionally flat JSON so a small UI can edit it
without understanding emulator launchers.

Syncthing is integrated through BTRC commands:

```sh
build/semu sync setup --project "$PWD"
build/semu sync force all --project "$PWD"
build/semu sync status --project "$PWD"
build/semu sync tray --project "$PWD"
```

Saves, states, screenshots, and gamelists sync by default. ROMs and BIOS are
declared but disabled by default because they are large and user-owned.
Controller profile and Steam Input expectations live in `src/semu.btrc` so
the UI/editor layer can update declarative config without changing launcher
scripts.
