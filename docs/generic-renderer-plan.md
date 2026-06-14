# Generic Renderer Plan

Semu's production shader/bezel path is one shared Semu rendering model compiled
into per-emulator render hooks. It is not a RetroArch-only shader plan and not
a vkBasalt/gamescope window-overlay plan. External window/compositor injection
is fallback/prototype only.

## Contract

- Semu owns the rendering declarations under `config/assets/**`.
- Semu compiles those declarations into generated hook config, shader/effect
  files, viewport metadata, and emulator package patches.
- The hook must apply only to emulated game output, not ES-DE, Semu settings,
  emulator settings dialogs, launcher UI, or debug UI.
- The backend must use actual monitor dimensions and Semu's declared content viewport, not a hard-coded Deck panel.
- Emulator-native shader systems are not Semu policy and must not be used as a
  shortcut for the generic renderer. RetroArch is represented through the same
  Semu source-hook contract as other emulators, without generated RetroArch
  shader presets or `video_shader` config.

## Backend Choice

Per-emulator source hooks are the production target. Each hook reads the same
Semu-owned generated render plan and applies the selected shader/bezel at the
emulator's final game framebuffer or content-present layer.

The external compositor experiment is retained only as evidence and fallback
research. It is attractive because it is generic across windows, but Deck proof
showed it is too fragile to be the primary path.

Current Deck proof:

- `/usr/bin/gamescope` `3.16.23.1` runs without ReShade.
- `/usr/bin/gamescope` crashes with status `139` after loading a real pixel
  shader over RetroArch GL output.
- `vkBasalt` process injection needs Vulkan output; RetroArch Vulkan fails on
  the Deck before creating the video context.
- Mesa Zink hangs during `vkCreateInstance` on the Deck test path.
- Bundled Nix `gamescope` `3.16.22` fails Vulkan instance creation on the Deck.
- The custom compositor experiment was removed. It was not BTRC-owned and did not satisfy the architecture.
- RetroArch no longer uses generated RetroArch shader config, Slang presets, or
  launch-time shader arguments. Its current Semu integration is a generic
  source-hook proof at the libretro content-frame handoff in `gfx/video_driver.c`.
  Final shader/bezel composition still needs to consume Semu's generated hook
  config at that boundary.
- Standalone proof hooks now exist for Dolphin, PCSX2, PPSSPP, Flycast, Azahar,
  and melonDS. They prove a Semu-owned hook runs at the game-framebuffer/final
  present boundary and writes `SEMU_RENDER_HOOK_PROOF`. They are not yet the
  final shader/bezel composition implementation.

These failures are not acceptable for a production renderer. They also confirm
that a blind window overlay cannot reliably know which pixels are game content
versus settings or emulator UI.

## Hook Architecture

Semu keeps a single declarative rendering model:

```text
config/assets/systems/*.json
config/assets/shaders.json
config/assets/bezels.json
$SEMU_PROJECT/.semu/generated/assets/rendering/**
```

Each emulator declares a render integration:

```json
{
  "semu_renderer": {
    "preferred_backend": "source_hook",
    "hook_scope": "game_framebuffer",
    "hook_config": "${generated}/assets/rendering/hooks/<emulator>.json",
    "external_fallback_backend": "disabled"
  }
}
```

The Semu compiler emits hook config under `.semu/generated/assets/rendering`.
Nix builds emulator packages with small source patches that read that config at
runtime. The patch should be as close as possible to a stable present/final-pass
boundary, and it must fail closed: if Semu config is absent or invalid, the
emulator runs normally without visual effects.

The launcher passes only Semu-owned paths:

```text
SEMU_RENDER_HOOK=1
SEMU_RENDER_HOOK_CONFIG=$SEMU_PROJECT/.semu/generated/assets/rendering/hooks/<emulator>.json
SEMU_SYSTEM=<system>
```

No hook may edit user emulator config directly. Generated emulator-native config
is allowed only under Semu-owned generated paths.

## Emulator Hook Points

These are the current hook contracts from source research. They live in
`config/emulators/<name>/rendering.json` so package patches and verification can
be generated from Semu-owned declarations.

| Emulator | Hook Point | UI Exclusion | Package Status |
|---|---|---|---|
| `retroarch` | `gfx/video_driver.c::video_driver_frame` libretro content-frame handoff before RetroArch menu/widgets/OSD policy. | Excludes RetroArch menus/widgets/OSD by keeping Semu policy scoped to content frames, not generated native shader presets. | Generic source-hook proof patch wired through `config/emulators/retroarch/package.nix`; final shader/bezel composition pending. |
| `dolphin` | `VideoCommon::Presenter::Present` after `RenderXFBToScreen` and before `OnScreenUI::DrawImGui`. | Excludes Dolphin settings, ImGui, and OSD. | Proof patch wired through `config/emulators/dolphin/package.nix`. |
| `pcsx2` | `GSRenderer.cpp` after `g_gs_device->PresentRect(...)` in both present paths and before `EndPresentFrame`. | Excludes fullscreen UI and ImGui OSD drawn in `EndPresentFrame`. | Proof patch wired through `config/emulators/pcsx2/package.nix`. |
| `ppsspp` | Existing final post-shader/output path in `PresentationCommon` around `CopyToOutput`. | Excludes PPSSPP app UI/settings. | Proof patch wired through `config/emulators/ppsspp/package.nix`. |
| `flycast` | Backend final game blit before OSD/UI render: OpenGL before `drawOSD`, Vulkan before `gui_draw_osd`/overlay/ImGui. | Excludes Flycast OSD and ImGui UI when hooked before those calls. | OpenGL proof patch wired; Vulkan composition still pending. |
| `azahar` | OpenGL `RendererOpenGL::TryPresent` final blit; Vulkan `RendererVulkan::RenderToWindow` or `PresentWindow::CopyToSwapchain` before swapchain present. | Excludes Qt settings/game list; touch cursor policy must be explicit. | Linux OpenGL proof patch wired; macOS binary package must become source-built. |
| `melonds` | `ScreenPanelGL::drawScreen` after DS screen draw loop and before `osdUpdate`/`SwapBuffers`. | Excludes Qt settings and OSD. | OpenGL proof patch wired through `config/emulators/melonds/package.nix`. |
| `cemu` | Planned: `LatteRenderTarget_copyToBackbuffer` around `DrawBackbufferQuad` before screenshots, ImGui, software keyboard, applet, and Latte overlay. | Excludes Cemu overlays when inserted before those calls. | Requires source-build packaging before enabling. |
| `ryujinx` | Planned: `Ryujinx.Graphics.GAL.IWindow.Present` implementations after final game scaling/blit and before swap/present. | Excludes Avalonia settings/UI. | Current Semu package is binary Ryubing; source-build package required before enabling. |

## Package Direction

Gamescope is a compositor, not an emulator. Its package entrypoint lives at:

```text
config/compositors/gamescope/package.nix
```

Reusable source/patch machinery lives at:

```text
build/packaging/nix/lib/source-package.nix
build/packaging/nix/patches/gamescope/
```

When Semu patches an emulator or compositor package, the package must:

- pin upstream source with rev and hash;
- apply ordered Semu patch files through Nix `patches`;
- expose patch-only checks through `passthru.tests.semuPatchesApply`;
- assert hook markers in `postPatchAssertions`;
- fail loudly when a patch no longer applies or no longer installs the intended hook.

## Next Proof

1. Research source hook points from primary emulator sources.
2. Add render hook contracts to `config/emulators/*/rendering.json`.
3. Add Nix source-overlay plumbing for hook patches.
4. Replace proof-only hooks with shared shader/bezel composition that consumes
   Semu's generated hook config.
5. Run each affected route with render required on the Deck.
6. Inspect screenshots for real game pixels, shader behavior, bezel geometry,
   fullscreen sizing, and UI exclusion.
7. Promote each emulator hook only after screenshot proof.
