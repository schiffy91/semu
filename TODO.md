# Semu TODO

## Verified On Steam Deck

- Rebuilt and installed the self-contained AppImage at
  `/home/deck/Applications/Semu/Semu-x86_64.AppImage`.
- Installed AppImage hash
  `4b7dc3f90d603a3c4b2a1b0f1c5dc6bac6dbe7491f3ea63b11523d318aa7d261`.
- Ran the Desktop Mode direct AppImage Deck loop over the required routed
  emulator set with clean process startup, foreground-window wait, screenshots,
  scripted input probes, and unified quit verification. `gb`, `gbc`, `gba`,
  `nes`, `snes`, `genesis`, `n64-retroarch`, `psp`, `dreamcast`, `gc`, `wii`,
  `ps2`, `nds-melonds`, `n3ds`, `wiiu`, and `switch` all passed.
- Visually inspected final 3DS and Wii captures: 3DS shows Super Mario 3D Land
  in-game UI, and Wii is past the strap screen at Kirby's Epic Yarn's save
  prompt.
- Confirmed the RetroArch DeSmuME DS alternate route is not production-ready on
  the Deck loop: the earlier optional route produced a screenshot but exited
  `status=139` before quit. The production DS route remains melonDS.
- Verified `presentation plan` runs from the installed AppImage and resolves
  bundled RetroArch shader paths.
- Verified Steam can launch the installed AppImage via the generated shortcut
  URI `steam://rungameid/16468373422993309696`; the AppImage stayed running and
  painted ES-DE from the Steam-launched process in Desktop Mode.
- Verified PCSX2 runtime config uses the Deck SD-card BIOS path and does not
  bake local Mac paths into committed profiles.

## Verified Locally

- Added `presentation state` and `presentation broadcast` so emulator adapters
  can expose normalized runtime aspect/layout state without owning Semu policy.
- `presentation plan` now reports effective runtime decisions:
  `effective_aspect`, `presentation_mode`, `selected_shader_file`,
  `selected_bezel_file`, and `selected_runtime_preset`.
- `presentation plan` now also reports separate resolved shader, bezel,
  runtime-preset, and launcher-effective shader paths with `ok`, `missing`, or
  `disabled` status, so Semu can detect missing bundled assets without editing
  emulator-native config.
- Dynamic 4:3/16:9 systems now have editable widescreen shader, bezel, and
  runtime preset overrides in `settings/presentation/*.json`.
- Added dependency-free BTRC terminal UIs for presentation, input/keymap, and
  sync settings. These edit only Semu-owned source files:
  `settings/presentation/*.json`, `input/keymaps/steam_deck.skm`, and
  `sync/sync.json`.
- Added smoke coverage that proves input and sync UI edits do not mutate
  generated emulator or service files before an explicit apply.
- Documented the ownership boundary: Semu-owned JSON/keymap files are source,
  generated emulator profiles and ES-DE files are compiled artifacts, and live
  emulator config is adapter state that Semu may read/broadcast but not treat as
  policy source.
- Added `SemuGeneratedFiles` so core generated writes are explicitly classified
  as Semu project artifacts or external install artifacts. Smoke coverage now
  rejects external ROM paths as generated output.
- Extended the ownership boundary so source writes, generated project writes,
  routed adapter-state writes, and documented external install writes use
  separate APIs. Smoke coverage now classifies source, generated, adapter-state,
  and external ROM paths explicitly.
- Added explicit `settings compile` / `lifecycle compile` paths so Semu-owned
  source files can be compiled into emulator and ES-DE artifacts without
  implying that the UI directly edits emulator-native files. Smoke coverage now
  exercises the compile command and ES-DE settings entry.
- Added input capability reporting so save/load/slot state actions are only
  advertised for generated RetroArch, Dolphin, and PCSX2 profiles. Other
  emulator routes are marked disabled until adapter config and physical state
  file proof exist.
- Local verification passed for BTRC presentation smoke, generated-C e2e,
  AppImage/Nix routing smoke, JSON formatting, and whitespace checks.

## Remaining Physical Gates

- Run a real Steam Deck Game Mode pass for Steam Input templates, the physical
  left-trackpad radial quit, and ES-DE return flow.
- Confirm controller movement/buttons inside representative games from Game
  Mode, not only SSH/uinput.
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
- Extend the dependency-free BTRC settings UI to cover screenshot hooks, BIOS
  status, and advanced per-emulator settings.
- Pair a second Syncthing device and verify conflict behavior.
