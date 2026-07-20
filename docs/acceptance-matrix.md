# Semu Physical Acceptance Matrix

This matrix is physical Steam Deck acceptance, not package or fixture status.
Every row is open. The repository currently retains only a generated acceptance
plan: there is no production AppImage and no per-system runtime, screenshot, or
operator evidence under `build/verification/steamdeck`.

## Frontend

| Status | Surface | User-observed failure | Required physical evidence |
| --- | --- | --- | --- |
| [ ] | ES-DE | Loading splash disappeared and startup was black. Earlier Semu entries did nothing or failed through FUSE. | Visible nonblack splash, bounded startup, exactly one native Semu Settings start-menu entry, settings UI opens through the regular no-FUSE launcher, a game starts, and ES-DE returns to the foreground after quit. |

## Systems

| Status | System | Emulator path | User-observed failure | Required physical evidence |
| --- | --- | --- | --- | --- |
| [ ] | Game Boy | RetroArch / Gambatte | Gameplay input failed; an Xbox-controller toast appeared; shader resembled a scarf; bezel quality was poor. | Steam Virtual Gamepad gameplay with no autoconfig toast; restrained DMG LCD/ghosting; realistic gray DMG shell; fullscreen; save/load restoration; Start+Select and radial quit; ES-DE return. |
| [ ] | Game Boy Color | RetroArch / Gambatte | Same input, notification, shader, and bezel failures as Game Boy. | Working gameplay input; authentic reflective GBC LCD; realistic frost/grape-purple GBC shell; fullscreen; save/load restoration; both quit paths; ES-DE return. |
| [ ] | Game Boy Advance | RetroArch / mGBA | Same input, notification, shader, and bezel failures as Game Boy. | Working gameplay input; authentic AGB-001 LCD; realistic indigo/purple wide GBA shell; fullscreen; save/load restoration; both quit paths; ES-DE return. |
| [ ] | NES | RetroArch / Mesen | RetroArch input, shader, and bezel failures. | Gameplay input; composite 240p CRT treatment; measured high-quality 4:3 CRT bezel; fullscreen; save/load restoration; both quit paths; ES-DE return. |
| [ ] | SNES | RetroArch / Snes9x | RetroArch input, shader, and bezel failures. | Gameplay input; appropriate S-Video/composite CRT treatment; measured high-quality 4:3 CRT bezel; fullscreen; save/load restoration; both quit paths; ES-DE return. |
| [ ] | Genesis | RetroArch / Genesis Plus GX | RetroArch input, shader, and bezel failures. | Gameplay input; composite-aware CRT treatment that preserves dithering; measured high-quality 4:3 CRT bezel; fullscreen; save/load restoration; both quit paths; ES-DE return. |
| [ ] | Nintendo 64 | RetroArch / Mupen64Plus-Next | Output was upside down, with the same RetroArch input/rendering failures. | Asymmetric real frame proves upright orientation; gameplay input; VI-soft CRT treatment; correct 4:3 or reported widescreen bezel; save/load restoration; both quit paths; ES-DE return. |
| [ ] | PlayStation | RetroArch / Beetle PSX | RetroArch input, shader, and bezel failures. | Declared BIOS copied only into isolated Semu state with equal source/copy hashes and unchanged media; gameplay input; era-accurate CRT and bezel; fullscreen; save/load restoration; both quit paths; ES-DE return. |
| [ ] | Nintendo DS | melonDS primary; RetroArch / DeSmuME fallback | melonDS worked but its bezel was not photorealistic; fallback lacked shader/bezel treatment. | Both configured launch paths reach gameplay; independent top/bottom shader history; high-quality measured shell and maximized layout; correct touch mapping and controls; fullscreen; save/load restoration; both quit paths; ES-DE return. |
| [ ] | GameCube | Dolphin | Shader looked plausible and controls worked, but no bezel appeared. | Game-only renderer hook; live 4:3/16:9 selection; high-quality measured TV bezel; controls; fullscreen; save/load where supported; both quit paths; ES-DE return. |
| [ ] | Wii | Dolphin | Shader looked plausible, no TV bezel appeared, and games needed different controller/accessory modes. | Live 4:3/16:9 rendering; Wiimote, Wiimote+Nunchuk, Classic, and GameCube modes; quick semantic-icon radial switch; gameplay controls; save/load where supported; both quit paths; ES-DE return. |
| [ ] | Dreamcast | Flycast | ROM launch crashed immediately back to ES-DE. | Real ROM reaches nonblank fullscreen gameplay within a bounded timeout; controller input; correct adaptive rendering; save/load restoration; both quit paths; clean ES-DE return. |
| [ ] | PlayStation 2 | PCSX2 | Gameplay input failed. | Steam Deck mapping; real ROM reaches fullscreen gameplay; correct 4:3/16:9 CRT rendering; save/load restoration; both quit paths; ES-DE return. |
| [ ] | PSP | PPSSPP | Gameplay was good, but no bezel appeared. | Correct 480x272 game rectangle; selected realistic black or red God of War PSP shell; controls; fullscreen; save/load restoration; both quit paths; ES-DE return. |
| [ ] | Nintendo 3DS | Azahar | Fullscreen window showed black game content; Deck ROMs were reported encrypted. | Selected Deck ROM is proven decrypted and byte-identical to the declared Mac source, or a bounded non-overwriting copy is performed with hashes; real ROM reaches nonblack fullscreen gameplay; independent top/bottom rendering and measured 3DS shell; touch/controls; save/load restoration; both quit paths; ES-DE return. |
| [ ] | Wii U | Cemu | Black screen for at least one minute; earlier builds also lacked input. | Real ROM reaches nonblank gameplay within a bounded timeout; Steam Deck mapping; modern clean rendering from the game-only hook; fullscreen; save/load where supported; both quit paths; ES-DE return. |
| [ ] | Switch | Ryujinx | Long black screen led to a loading screen that hung. | Declared keys/firmware read from bounded media without mutation; real ROM reaches nonblank gameplay within a bounded timeout; controller input; modern clean output without a legacy bezel; fullscreen; save/load where supported; both quit paths; ES-DE return. |

The macOS ares N64 package is a separate target contract and is not a Steam Deck
matrix row.

## Automated Evidence Available

The following passed during the 2026-07-19 documentation audit:

- `make tree-audit`: approved ownership tree plus 20 rejection fixtures.
- `make compiler-tests`: compiler stages/precedence/emission, settings and
  ES-DE documents, emulator launch profiles, 17-system rendering declarations,
  exact committed asset metadata/hashes, geometry at 1280x800 and 1920x1080,
  and Steam Input VDF/icon/install contracts.
- `make -f tests/targets/steamdeck/Makefile test`: strict local harness compile,
  17 fixture descriptors, acceptance-plan generation, and command dry run.
- `make -f tests/targets/steamdeck/Makefile installer-contract`: fixture proof
  for one-time extraction, stable no-FUSE launch, upgrade, rollback, corruption
  rejection, and transactional legacy-layout cleanup.

These checks do not build a current production artifact, execute emulator hooks,
load shader resources, launch a real ROM, inspect a Deck screenshot, or observe
physical controls. Forced strict production compilation and `make test` pass,
but flake evaluation still fails as recorded in `README.md`; package and
AppImage verification are therefore open.

## Evidence Required Per System

Each system must retain the exact Nix derivations and AppImage digest, physical
device identity, ROM/core/BIOS/key/firmware receipts, before/after external-media
hashes, a frame-bound screenshot and game rectangle, renderer layer receipts,
gameplay-input evidence, save/load outcome where supported, immediate
process-group quit, physical radial action, and post-quit ES-DE foreground
receipt.

Acceptance proceeds through RetroArch systems, Dolphin, then each standalone
emulator. Bazzite must not run until every row above passes on the physical Deck.
