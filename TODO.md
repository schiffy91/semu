# Semu TODO

## Verified On Steam Deck

- Rebuilt and installed the self-contained AppImage at
  `/home/deck/Applications/Semu/Semu-x86_64.AppImage`.
- Ran the AppImage Deck loop over the routed emulator set with screenshots and
  process quit verification: GB, GBC, GBA, NES, SNES, Genesis, N64,
  DS/RetroArch, PSP, Dreamcast, GameCube, Wii, PS2, DS/melonDS, 3DS, Wii U,
  and Switch all returned `status=0` and `quit=ok`.
- Verified representative screenshots for GB LCD, Genesis CRT/NTSC, PS2
  runtime, and 3DS top/bottom layout.
- Verified `presentation plan` runs from the installed AppImage and resolves
  bundled RetroArch shader paths.
- Verified PCSX2 runtime config uses the Deck SD-card BIOS path and does not
  bake local Mac paths into committed profiles.

## Verified Locally

- Added `presentation state` and `presentation broadcast` so emulator adapters
  can expose normalized runtime aspect/layout state without owning Semu policy.
- `presentation plan` now reports effective runtime decisions:
  `effective_aspect`, `presentation_mode`, `selected_shader_file`,
  `selected_bezel_file`, and `selected_runtime_preset`.
- Dynamic 4:3/16:9 systems now have editable widescreen shader, bezel, and
  runtime preset overrides in `settings/presentation/*.json`.
- Local verification passed for BTRC presentation smoke, generated-C e2e,
  AppImage/Nix routing smoke, JSON formatting, and whitespace checks.

## Remaining Physical Gates

- Run a real Steam Deck Game Mode pass for controls, Steam Input templates,
  left-trackpad radial quit, and ES-DE return flow.
- Confirm controller movement/buttons inside representative games from Game
  Mode, not SSH/uinput.
- Confirm the physical left-trackpad radial Quit action returns to ES-DE for
  RetroArch, Dolphin, PPSSPP, Flycast, melonDS, PCSX2, Cemu, Azahar, and
  Ryujinx.
- Verify save/load radial actions only where the emulator has production-safe
  state support; hide or disable unsupported actions.

## Remaining Visual Work

- Add or package the requested photorealistic bezel art packs: classic grey Game
  Boy, frost purple GBC, purple wide GBA, Panasonic/Sony CRT, maximized DS/3DS,
  and red God of War or black PSP.
- Prototype standalone visual effects behind feature flags with gamescope
  ReShade and vkBasalt only after the input gates pass.

## Remaining Product Work

- Promote `utils/steam-deck-bootstrap.sh` into BTRC Deck commands after the
  full physical Game Mode pass.
- Extend the current dependency-free BTRC settings UI to cover keymaps,
  screenshots, BIOS status, and `presentation get|put` per-system visual
  settings.
- Pair a second Syncthing device and verify conflict behavior.
