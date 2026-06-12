# Semu TODO

- Run a real Steam Deck Game Mode pass for controls, Steam Input templates,
  left-trackpad radial quit, and ES-DE return flow.
- Exercise representative games across the routed emulator set on the physical
  Deck: RetroArch, Dolphin, PPSSPP, Flycast, melonDS, PCSX2, Cemu, Azahar, and
  Ryujinx.
- Rebuild/reinstall the AppImage and rerun the broad Deck pass after the
  settings/state-seeding changes: PCSX2 setup wizard, Azahar OpenGL config,
  Cemu keys/state, Ryujinx keys, RetroArch integer scaling, RetroArch shader
  arguments, and ES-DE `Semu Settings` action entries.
- Verify the new `settings/presentation/*.json` station matrix on the physical
  Deck AppImage: resolved shader paths, per-system RetroArch presets, and
  standalone adapter state for dynamic 4:3/16:9 systems.
- Add or package the requested photorealistic bezel art packs: classic grey Game
  Boy, frost purple GBC, purple wide GBA, Panasonic/Sony CRT, maximized DS/3DS,
  and red God of War or black PSP.
- Prototype standalone visual effects behind feature flags with gamescope
  ReShade and vkBasalt only after the input gates pass.
- Promote `utils/steam-deck-bootstrap.sh` into BTRC Deck commands after the
  full physical Game Mode pass.
- Extend the current dependency-free BTRC settings UI to cover keymaps,
  screenshots, BIOS status, and `presentation get|put` per-system visual
  settings.
- Pair a second Syncthing device and verify conflict behavior.
