# Semu TODO

## Verified On Steam Deck

- Rebuilt and installed the self-contained AppImage at
  `/home/deck/Applications/Semu/Semu-x86_64.AppImage`.
- Ran the Desktop Mode direct AppImage Deck loop over the required routed
  emulator set. Processes launched and accepted the scripted quit path, but the
  captured PNGs were later found to be desktop captures rather than reliable
  emulator framebuffer proof. Treat `build/deck-loop-*` as process evidence
  only until the foreground/window capture issue is fixed and rerun.
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
- Local verification passed for BTRC presentation smoke, generated-C e2e,
  AppImage/Nix routing smoke, JSON formatting, and whitespace checks.

## Remaining Physical Gates

- Run a real Steam Deck Game Mode pass for Steam Input templates, the physical
  left-trackpad radial quit, and ES-DE return flow.
- Fix and rerun Desktop Mode visual capture so the required emulator loop proves
  actual emulator windows/framebuffers, not only process launch and quit.
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
