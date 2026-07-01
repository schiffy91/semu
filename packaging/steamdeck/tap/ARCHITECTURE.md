# Semu Tap Architecture

The tap is a boundary, not a renderer.

## Contract

Each emulator reports one small frame description at the game-content boundary:

```c
SemuTapState {
  active,
  framebuffer_width,
  framebuffer_height,
  content_x,
  content_y,
  content_width,
  content_height,
  native_width,
  native_height,
  origin,
  rotation
}
```

Then the emulator presents through its normal graphics path.

The Semu compositor reads the latest `SemuTapState`:

- `active == 0`: present untouched.
- `active == 1`: reproject the game content into the selected bezel screen
  rect, apply shader policy, draw bezel layers, and present.

## Boundary

Emulator patches should only:

- include `semu_tap.h`
- populate `SemuTapState`
- call `semu_tap_report_safe`

Emulator patches should not:

- parse shader presets
- load bezel art
- duplicate compositor code
- infer viewport geometry from pixels
- shade menus, settings, OSD, or frontend UI

If an emulator patch grows past a small metadata adapter, move the code into the
shared compositor or asset compiler.

## Report Points

| Emulator | Systems | Report point |
| --- | --- | --- |
| RetroArch | NES, SNES, Genesis, N64, GB, GBC, GBA, PSX, NDS, and other libretro routes | `gfx/video_driver.c:video_driver_frame()` before menu/overlay presentation |
| Dolphin | GameCube, Wii | renderer present boundary with Dolphin's viewport/display rect |
| PCSX2 | PS2 | GS backend present boundary with display rect |
| PPSSPP | PSP | frame/end-present boundary |
| Flycast | Dreamcast | `rend_present`/present boundary |
| Azahar | 3DS | frontend/swapchain present boundary |
| Cemu | Wii U | swapchain present boundary |
| Ryujinx | Switch | window/surface present boundary |

## Current Implementation

- `semu_tap.h` defines the ABI.
- `libsemutap.c` is the OpenGL `glXSwapBuffers` compositor prototype.
- Vulkan should be a compositor backend with the same tap input, not a new
  per-emulator rendering implementation.

Runtime proof must show that game frames are affected and emulator UI is not.
