# Semu TODO

- Run a real Steam Deck Game Mode pass for controls, Steam Input templates,
  left-trackpad radial quit, and ES-DE return flow.
- Exercise representative games across the routed emulator set on the physical
  Deck: RetroArch, Dolphin, PPSSPP, Flycast, melonDS, PCSX2, Cemu, Azahar,
  Gopher64, and Ryujinx.
- Add ES-DE `Semu Settings` entries for ROM location, Syncthing, sync folders,
  CRT shaders, bezels, input test, doctor, and reconfigure.
- Add RetroArch shader preset configuration, starting with bundled
  `libretro-shaders-slang` and Mega_Bezel-compatible preset paths.
- Prototype standalone visual effects behind feature flags with gamescope
  ReShade and vkBasalt only after the input gates pass.
- Promote `utils/steam-deck-bootstrap.sh` into BTRC Deck commands after the
  full physical Game Mode pass.
- Add a controller-first settings UI for keymaps, sync folders, screenshots,
  ROM paths, BIOS status, shaders, and bezels.
- Pair a second Syncthing device and verify conflict behavior.
