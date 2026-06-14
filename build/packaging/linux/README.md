# Linux / Steam Deck Launcher

This tree is the Steam Deck-first packaged launcher path for Semu.

- `AppRun` finds the cloud-synced Semu project, mirrors the
  project-generated ES-DE config into `~/ES-DE/custom_systems`, and launches
  bundled ES-DE. Setup is explicit: build the appropriate compiler target
  before the first normal launch.
- If the AppImage was built with `--nix-package`, `AppRun` mounts the bundled
  closure at `/nix/store` with bubblewrap and routes ES-DE find rules to the
  Nix-built `semu-*` launchers in `usr/bin`.
- `bin/semu-*` launchers are stable ES-DE entry names. They are thin shims
  into `semu launcher ...`; emulator IDs, Flatpak IDs, X11/Wayland
  policy, ROM/project grants, and RetroArch routing are declared under
  `config/emulators/` and compiled by `src/generators/`.
- `semu-render` is only used for emulator child windows, so global
  visual wrappers do not affect ES-DE or settings.
- Screenshot verification hooks are declared in `semu.json` and editable
  through `config/verification/screenshots.json`. When
  `SEMU_SCREENSHOT_HOOKS=1` is set, emulator launchers capture
  `before_launch`, delayed `after_spawn`, and `after_exit` images under
  `ES-DE/ES-DE/screenshots/verification`.

Typical project setup:

```sh
make btrc-build
build/out/semu build target steam-deck --project "$SEMU_PROJECT" --roms "$SEMU_PROJECT/ES-DE/ES-DE/ROMs"
build/out/semu build configs --project "$SEMU_PROJECT"
build/out/semu verify target steam-deck --project "$SEMU_PROJECT"
```

On Steam Deck, pass an external SD card path to `--roms` if desired, for
example `/run/media/deck/SD`. The choice is stored in
`.semu/semu.json`, which is intentionally flat JSON so a small UI can edit it
without understanding emulator launchers.

Syncthing is integrated through BTRC commands. In an AppImage install, run the
commands through the AppImage so generated systemd units point at the stable
AppImage entrypoint:

```sh
Semu-x86_64.AppImage sync setup --project "$SEMU_PROJECT" --roms /run/media/deck/SD
Semu-x86_64.AppImage sync force all --project "$SEMU_PROJECT"
Semu-x86_64.AppImage sync status --project "$SEMU_PROJECT"
Semu-x86_64.AppImage sync tray --project "$SEMU_PROJECT"
```

Saves, states, screenshots, and gamelists sync by default. ROMs and BIOS are
declared as opt-in folders because they are large and user-owned.
Controller profile and Steam Input expectations live under the compiler input
definitions and generated launch/config outputs so the UI/editor layer can
update declarative config without changing launcher scripts.
