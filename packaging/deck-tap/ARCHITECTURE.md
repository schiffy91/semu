# Semu compositor "tap" — architecture

We render era-accurate bezels + CRT/LCD shaders **on top of** each emulator's frame, only
during gameplay, resolution-independent. The emulator is built from source and tells us, every
frame, exactly where the game content is (or that it's in a menu). We never inspect pixels.

## The contract — `semu_tap.h`

```
emulator (per frame):  fill SemuTapState{active, content rect, native res, ...}
                       -> semu_tap_report_safe(&s)
                       -> present (glXSwapBuffers / vkQueuePresentKHR)

compositor present hook: read last SemuTapState
                       -> active==0 ? present untouched (menu / no content)
                       -> else: draw bezel art + reproject the game (content rect)
                                into the bezel's screen cutout, apply CRT shader, present
```

`SemuTapState` carries: `active`, framebuffer size, the **content rectangle** (the emulator's real
viewport — no guessing/centering), the **native resolution** (scanline density + integer scale),
rotation, and the coordinate origin (GL bottom-left vs Vulkan top-left).

Reporting is API-agnostic. The compositor has two back ends:
- **OpenGL**: interpose `glXSwapBuffers` (this is `libsemutap.so`, already working as the tap-out).
- **Vulkan**: a Vulkan layer interposing `vkQueuePresentKHR` (to build; structured like vkBasalt).

## Emulators / systems to support, and where each reports

| Systems | Emulator | API | Report point (call `semu_tap_report_safe`) |
|---|---|---|---|
| nes snes n64 genesis sms gg gb gbc gba pce neogeo psx nds … | **RetroArch** (libretro) | GL glcore / Vulkan | `gfx/video_driver.c: video_driver_frame()` — has the viewport (`video_driver_get_viewport_info`), `av_info` geometry (native res), and `menu_is_alive`. **← STEP 2 reference impl.** |
| 3DS | **Azahar** | Vulkan | its swapchain present / `Frontend` present path |
| GameCube, Wii | **Dolphin** | GL / Vulkan | `Renderer::Present` / backend `PresentBackbuffer` |
| PS2 | **PCSX2** | GL / Vulkan | GS backend present (display rect is `GSgetDisplayRect`) |
| Switch | **Ryujinx** | Vulkan | its `Window`/surface present |
| WiiU | **Cemu** | Vulkan | its swapchain present |
| Dreamcast | **Flycast** | GL / Vulkan | its `rend_present` |
| PSP | **PPSSPP** | GL / Vulkan | its `EndFrame`/present |

The content rectangle + active flag come straight from each emulator's own viewport math and
menu/loaded-content state — the data already exists internally; we just forward it.

## RetroArch specifics (STEP 2)

- Build RA + cores from source (nix), apply a small patch that `#include "semu_tap.h"` and, in
  `video_driver_frame()`, fills `SemuTapState` from:
  - `content rect`  ← `video_driver_get_viewport_info(&vp)` → `vp.x/y/width/height`
  - `native_w/h`    ← `av_info->geometry.base_width/base_height`
  - `active`        ← content loaded AND not `menu_st->flags & MENU_ST_FLAG_ALIVE`
  - `origin`        ← `SEMU_TAP_ORIGIN_BOTTOM_LEFT` (glcore)
  then `semu_tap_report_safe(&s)`.
- No RA config hacks (no forced custom viewport, no integer-scale guessing): the compositor takes
  the real viewport RA reports and reprojects the game into the bezel's screen cutout itself.

## Compositor (generic, consumes the contract)

- `libsemutap.so` (GL): exports `semu_tap_report` (stores the last state); `glXSwapBuffers` hook
  uses it — `active==0` → pass through; else capture the frame, reproject `content rect` → the
  bezel art's screen rect, apply CRT/scanline/mask + rounded-corner mask, draw the bezel PNG, present.
- Bezel art + screen rect + shader params are per-asset (from the launcher / the `.slangp` parser),
  independent of the emulator.

## Plan
- **STEP 1** ✅ this contract (`semu_tap.h`) + this map.
- **STEP 2** generic compositor consuming the contract + RA reference impl (patch + build RA from source).
- **STEP 3** scale: implement the report point in each other emulator; add the Vulkan compositor back end.
