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
  policy, ROM/project grants, and RetroArch routing live under
  `src/semu/emulators/`.
- `sandbox.sh` is a compatibility shim into `semu sandbox launch`.
  Bubblewrap composition and scratch symlink preparation are handled in BTRC.
- Screenshot verification hooks are declared in `semu.json` and editable
  through `verification/screenshots.json`. When
  `SEMU_SCREENSHOT_HOOKS=1` is set, emulator launchers capture
  `before_launch`, delayed `after_spawn`, and `after_exit` images under
  `.semu/content/screenshots/verification` unless an external ROM/content root
  is configured.

Typical project setup:

```sh
make btrc-build
build/semu deck install --project "$PWD" --roms "$PWD/.semu/content/ROMs"
build/semu doctor --project "$PWD"
build/semu screenshot status --project "$PWD"
```

On Steam Deck, pass an external SD card path to `--roms` if desired, for
example `/run/media/deck/SD`. The choice is stored in
`sync/sync.json`, which is intentionally flat JSON so a small UI can edit it
without understanding emulator launchers.

Syncthing is integrated through BTRC commands. In an AppImage install, run the
commands through the AppImage so generated systemd units point at the stable
AppImage entrypoint:

```sh
Semu-x86_64.AppImage sync setup --project "$PWD" --roms /run/media/deck/SD
Semu-x86_64.AppImage sync force all --project "$PWD"
Semu-x86_64.AppImage sync status --project "$PWD"
Semu-x86_64.AppImage sync tray --project "$PWD"
```

Saves, states, screenshots, and gamelists sync by default. ROMs and BIOS are
declared as opt-in folders because they are large and user-owned.
Controller profile and Steam Input expectations live under `src/semu/input/` and
`src/semu/bootstrap/` so
the UI/editor layer can update declarative config without changing launcher
scripts.
