# Semu TODO

## Verified On Steam Deck

- Rebuilt and installed the self-contained AppImage at
  `/home/deck/Applications/Semu/Semu-x86_64.AppImage`.
- Installed AppImage hash
  `8763f1bb950c908ca8a0687c18c5c961be36f7f3423f07291afcf4372da0d8ac`.
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
- Built the clean `/home/deck/semu-latest` checkout at commit `0f7c239` with
  `nix build .#default`; output
  `/nix/store/zs1iwb8ivfdzvilq8dv9kyvfkhhd2p21-semu-full`.
- On that pushed Deck checkout, `result/bin/semu e2e deck-evidence` passed and
  `result/bin/semu deck game-mode-evidence --prepare --allow-pending` wrote the
  physical checklist under `.semu/verification` while correctly reporting every
  physical Game Mode quit log as pending.
- Built the clean `/home/deck/semu-latest` checkout at commit `cad8b59` with
  `nix build .#default`; output
  `/nix/store/3yf0h0sxz32w5khh0afaa02xxlynb08c-semu-full`.
- On that pushed Deck checkout, `result/bin/semu e2e presentation` passed and
  `presentation plan --system gb` resolved shader, bezel, runtime preset, and
  launcher shader paths as `ok` against the bundled shader tree.
- On that pushed Deck checkout, `result/bin/semu e2e appimage` passed against
  the AppRun and AppImage assembly smoke path.
- Built `/home/deck/.cache/semu-verify-cad8b59/Semu-x86_64.AppImage` from
  that Nix result and verified the packaged AppImage itself with
  `e2e launcher`, `e2e appimage`, `presentation plan --system gb`, bundled
  executable extraction, and `semu-quit-watch` start/exit evidence.
- Verified the installed AppImage CLI from `/home/deck/Applications/Semu`:
  `presentation plan --system gb` resolves shader, bezel, runtime preset, and
  launcher shader paths from the mounted AppImage payload, and
  `keymap capabilities app.quit` plus the doctor input section report the
  Desktop-verified quit set with physical Game Mode still pending.
- Verified Linux structured quit evidence on the Deck from the Nix result:
  `semu-quit-watch` observed an injected `/dev/uinput` Select+Start event on
  `/dev/input/event11`, logged `reason=select+start`, terminated the
  child process, and exited cleanly.
- Rebuilt and installed the AppImage from commit `cad8b59`; extracted the
  installed AppImage's bundled `usr/bin/semu-quit-watch` and verified it writes
  durable start/exit evidence.
- Rebuilt and installed the AppImage from commit `0f7c239`; backup of the prior
  install is
  `/home/deck/Applications/Semu/Semu-x86_64.AppImage.prev-0f7c239-20260612-032707`.
- Verified the installed `0f7c239` AppImage runs `e2e deck-evidence`,
  `deck game-mode-evidence --allow-pending`, `keymap capabilities app.quit`,
  and `presentation plan --system gb` with shader, bezel, runtime preset, and
  launcher shader statuses all `ok`.
- Launched the installed AppImage in Deck Desktop Mode and captured a 1280x800
  screenshot showing ES-DE running through the bundled AppRun/bwrap path.
- Rebuilt and installed the AppImage from commit `3dd0c0a`; backup of the prior
  install is
  `/home/deck/Applications/Semu/Semu-x86_64.AppImage.prev-3dd0c0a-20260612-035730`.
- Installed AppImage hash is now
  `8763f1bb950c908ca8a0687c18c5c961be36f7f3423f07291afcf4372da0d8ac`.
- Verified the installed `3dd0c0a` AppImage runs `e2e deck-evidence`,
  `e2e appimage`, `deck game-mode-ready --prepare`, `deck game-mode-ready
  --prepare --allow-desktop`, `deck game-mode-evidence --allow-pending`,
  `keymap capabilities app.quit`, and `presentation plan --system gb` with
  shader, bezel, runtime preset, and launcher shader statuses all `ok`.

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
- Added `presentation audit` plus settings UI access to record per-system
  shader, bezel, runtime preset, and launcher shader readiness under
  `.semu/verification/presentation-assets.json`; strict mode fails when a
  required asset is declared but not bundled.
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
- Added structured `semu-quit-watch` evidence logging. The Deck loop now keeps
  per-system quit-watch logs and treats a quit-watch `reason=<chord>` event as
  the Semu launcher-layer proof that a unified quit action was observed.
- Routed launcher runs now default quit evidence to the Semu-owned
  `.semu/verification/quit-watch/<emulator>.log` path when
  `SEMU_QUIT_WATCH_LOG` is not explicitly set, so Steam/Game Mode launches can
  be audited without mutating emulator-native config.
- Added `deck game-mode-evidence` as the physical Game Mode evidence gate. It
  prepares a Semu-owned checklist under `.semu/verification`, fails on missing
  or lifecycle-only logs, and passes only when each selected emulator has
  quit-watch evidence with a `reason=` value.
- Added `deck game-mode-ready` as the physical pass readiness gate. It writes a
  Semu-owned JSON report under `.semu/verification`, verifies session type,
  Steam process state, Steam shortcut discovery, installed AppImage
  executability, checklist state, and can require complete quit-watch evidence
  after the physical pass.
- Local `e2e deck-evidence` now simulates Desktop Mode, Game Mode, missing Steam,
  Steam shortcut discovery, AppImage checks, readiness JSON formatting, and the
  combined `--require-evidence` gate.
- Added input capability reporting so save/load/slot state actions are only
  advertised for generated RetroArch, Dolphin, and PCSX2 profiles. Other
  emulator routes are marked disabled until adapter config and physical state
  file proof exist.
- Local verification passed for BTRC presentation smoke, generated-C e2e,
  AppImage/Nix routing smoke, JSON formatting, and whitespace checks.
- AppImage assembly smoke now covers linked `result` canonicalization before
  `nix copy`; the builder also avoids SteamOS `/tmp` space failures by using
  `$HOME/.cache/semu-appimage-work` when `TMPDIR` is unset.

## Remaining Physical Gates

- Run a real Steam Deck Game Mode pass for Steam Input templates, the physical
  left-trackpad radial quit, `deck game-mode-evidence`, and ES-DE return flow.
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
  and red God of War or black PSP. `presentation audit --strict` is now the
  gate for proving those declared assets are actually present.
- The new Deck-side `presentation plan --system ps2` check shows the generic
  Mega_Bezel runtime preset resolves, but the requested PS2-specific Soqueroeu
  TV background preset is still missing from the bundled assets.
- Prototype standalone visual effects behind feature flags with gamescope
  ReShade and vkBasalt only after the input gates pass.

## Remaining Product Work

- Promote `utils/steam-deck-bootstrap.sh` into BTRC Deck commands after the
  full physical Game Mode pass.
- Clean up the non-fatal Nix `symlinkJoin` build-log warning for
  `libretro-shaders-slang` (`libretro: is a link instead of a directory`).
- Extend the dependency-free BTRC settings UI to cover screenshot hooks, BIOS
  status, and advanced per-emulator settings.
- Pair a second Syncthing device and verify conflict behavior.
