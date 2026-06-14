# Input, Shaders, Bezels, And Settings

This document defines the order of operations for the production Steam Deck
loop. Input must become boring before visual treatment gets clever.

## Implementation Order

1. Prove the Steam Deck input contract in Game Mode.
2. Use the shipped `semu settings get|put|toggle|ui|apply` CLI/UI as the
   mutation contract.
3. Add a real ES-DE Start/Main Menu/Utilities Semu Settings entrypoint for
   configuration actions.
4. Ship one Semu-owned shader/bezel layer for every launched emulator process.
5. Resolve the emulator content viewport before applying any effect or bezel.
6. Add broader bezel artwork once the generic renderer is proven per emulator.

## RetroArch Versus Semu Input

| Concern | RetroArch Native | Semu Contract |
|---|---|---|
| Source of truth | RetroArch input config, hotkey enable button, and core overrides. | `config/input/keymaps/steam_deck.skm`, generated Steam Input VDF, emulator profile renderers, and `semu-quit-watch`. |
| Bottom-left trackpad | Can map a radial menu to RetroArch hotkeys, but it only applies to RetroArch cores. | Left trackpad radial emits Semu actions that every launcher receives. |
| Quit | Native `input_exit_emulator`, quit gamepad combo, or menu. | Radial quit emits `Select+Start`, `Esc`, `Ctrl+Q`, and `Alt+F4`; `semu-quit-watch` also observes evdev and terminates the routed child process group. |
| Save state | Native save-state hotkey and slots. | `state.save = Ctrl+S`; rendered into RetroArch plus standalone profiles that support save states. Unsupported emulators must hide or no-op the action explicitly. |
| Load state | Native load-state hotkey and slots. | `state.load = Ctrl+A`; same support rule as save state. |
| Menu | Native RetroArch menu toggle. | `ui.menu = Ctrl+M`; each emulator profile maps this to its own menu path where possible. |
| Screenshot | Native RetroArch screenshot hotkey. | `ui.screenshot = Ctrl+X`; screenshot hooks also capture before spawn, after spawn, and after exit when enabled. |
| Fullscreen | RetroArch video fullscreen config and hotkey. | Launchers force fullscreen/batch arguments and profile defaults for each emulator. |
| Visual filters | Built-in emulator visual systems are route-specific. | Semu keeps one visual policy and compiles it into each emulator's game-frame render hook. |
| Failure mode | Excellent for RetroArch systems, but does not cover Dolphin, PCSX2, PPSSPP, Cemu, Azahar, Ryujinx, or other standalone paths. | Covers every routed process, but each standalone emulator still needs a real Game Mode proof for focus, hotkeys, state behavior, and exit. |

The production target is not to imitate RetroArch everywhere. It is to expose a
single Semu controller contract and use the strongest native feature available
behind each emulator route.

## Ownership Boundary

Semu policy is edited only through Semu-owned files and commands:
`config/settings/semu-settings.json`, `config/assets/systems/*.json`,
`config/input/keymaps/steam_deck.skm`, `config/settings/sync.json`, and the
matching `settings`, `assets`, `keymap`, and `sync` CLI/UI commands.

Generated emulator profiles, Steam Input templates, ES-DE systems, desktop
entries, systemd units, and routed launcher config are compiled artifacts. They
are rewritten by `settings apply`, `config apply`, or commands that explicitly
pass `--apply`.

Steam's selected controller layout is also treated as external state. Semu owns
the desired keymap and generated templates; installing or selecting those
templates in Steam is an explicit Deck-side step outside the flattened compiler
CLI. The rest of Steam userdata remains Steam-owned.

Live emulator config is emulator state. Semu can read it to infer runtime aspect,
screen layout, or capability state, and helpers can broadcast normalized state
back into `settings/visual-state/*.json`; Semu policy still changes by
editing the owned files above, then compiling into emulator-readable files.
Direct emulator-native config edits are not a valid production workflow unless a
documented emulator migration explicitly owns the one-time import/export step.
Semu verification files under `.semu/verification` are also emulator state: they
record launcher evidence for audits, not emulator-owned policy.

## Current Left Trackpad Radial Contract

The generated Steam Input template currently uses the left trackpad radial menu
for these actions:

| Radial Item | Semu Action | Emitted Input |
|---|---|---|
| Save State | `state.save` | `Ctrl+S` |
| Load State | `state.load` | `Ctrl+A` |
| Quit | `app.quit` | `Select+Start`, `Esc`, `Ctrl+Q`, `Alt+F4` |
| Menu | `ui.menu` | `Ctrl+M` |
| Screenshot | `ui.screenshot` | `Ctrl+X` |
| Escape | `ui.escape` | `Esc` |

The physical Game Mode pass must prove all of this from Steam Input, not from
SSH key injection. SSH can test process behavior and screenshots, but it cannot
prove the actual Neptune left-trackpad radial menu.

Semu now exposes a capability report for these actions:

```sh
semu keymap capabilities
semu keymap capabilities state.save
```

State save/load/slot actions are generated only for RetroArch, Dolphin, and
PCSX2 today. Azahar, PPSSPP, Flycast, melonDS, Cemu, and Ryujinx remain marked
disabled for Semu state actions until a generated emulator profile exists and a
physical Game Mode pass proves that a state file is created and reloaded from
the radial path.

## Input Hardening Gates

- `doctor` verifies the generated Steam Input VDF exists and contains the
  expected radial actions.
- `keymap validate` verifies the canonical action names and controller combos.
- `deck game-mode-ready --prepare` writes the Semu-owned physical checklist and
  readiness report. It verifies the current session, Steam process, Steam
  shortcut, Steam Input template selection, installed AppImage, and checklist
  state before the physical pass.
  `--allow-desktop` is for SSH preflight only, including mixed
  Desktop/Gamescope sessions; final proof must run from Steam Game Mode.
- A Game Mode test opens each emulator through ES-DE, launches one known-good
  ROM, confirms controller movement/buttons, opens the left-trackpad radial, and
  uses Quit to return to ES-DE.
- `deck game-mode-evidence --prepare` writes the Semu-owned physical test
  checklist; `deck game-mode-evidence` fails any emulator whose quit-watch log
  is missing or lacks a `reason=` event.
- `deck state-evidence --prepare` writes the Semu-owned save/load checklist for
  emulators with generated state contracts; `deck state-evidence` fails until
  the matching emulator-state log has both `action=state.save result=ok` and
  `action=state.load result=ok`.
- `deck game-mode-ready --require-evidence` combines the readiness gate with
  complete quit-watch evidence after the physical pass.
- `deck production-status` is the SSH-friendly composed report. `deck
  production-ready` is the final gate: owned source config, rendering audit,
  Steam shortcut/template selection, installed AppImage, quit evidence, and
  generated state evidence must all pass.
- Save and load are only marked production-ready for an emulator after a real
  state file is created and reloaded from the radial path.
- Unsupported state actions are hidden or rendered as disabled capability
  entries instead of pretending every emulator has equivalent state semantics.
- Fullscreen is verified from screenshots for every emulator after launch and
  again after returning to ES-DE.

## Visual Pipeline

Semu treats visuals as a Semu-owned rendering contract, not as a RetroArch
feature or an emulator-specific setting. Global toggles live in
`config/settings/semu-settings.json`; per-system shader, bezel, layout,
viewport, and emulator defaults live in `config/assets/systems/<system>.json`.

The architecture is:

1. Semu-owned declarations define each system's display class, source geometry,
   scale policy, shader/effect selection, bezel art, safe area, and emulator
   probes.
2. The content viewport resolver combines those declarations with runtime
   emulator state and the target output surface. It returns the actual content
   rectangle, normalized UVs, matte/bezel bounds, scale factor, and orientation.
3. The generic renderer backend applies the selected shader/effect and bezel to
   that resolved content viewport through an emulator source hook. The hook is
   placed at the game framebuffer or final game-present layer, so emulator
   settings dialogs and frontend UI are not shaded.
4. Generated emulator configs provide only what the resolver and launcher need:
   fullscreen mode, preferred graphics API, aspect/scaling defaults, screen
   layout, emulator-state export paths, hook config paths, and process launch
   metadata. They do not receive user-edited renderer policy.

The production boundary is the emulator render hook. Launchers pass only
Semu-owned generated config paths and `SEMU_SYSTEM`. ES-DE, Semu settings
entries, terminal settings UI, sync helpers, doctor output, and other control
surfaces must never receive render hook env or compositor wrappers.

`semu-render` remains a fallback/prototype process wrapper for experiments. Deck
proof showed that gamescope ReShade and vkBasalt are too fragile as the primary
route for RetroArch, so compositor/window injection must not be treated as
production-ready without per-route screenshot proof.

The command contract is:

```sh
semu assets get gb renderer.shader.source_asset
semu assets put gb renderer.bezel.source_asset bezels/gb/classic-grey-game-boy.json --apply
semu assets put ps2 renderer.bezel.widescreen_source_asset bezels/ps2/clean-wide.json --apply
```

The asset plan combines Semu policy and runtime state for the launcher and
generated hook config. Asset audits check every selected shader/effect file,
bezel, viewport dependency, and backend config, write
`.semu/verification/assets.json`, and fail when required visual assets are
missing. Emulators read and/or broadcast their own config state, while Semu
keeps the display policy stable.

The asset plan reports resolution at separate layers:

| Field | Meaning |
|---|---|
| `selected_shader_file` | Semu-owned shader/effect policy after runtime aspect selection. |
| `selected_bezel_file` | Semu-owned bezel policy after runtime aspect selection. |
| `content_viewport` | Resolved content rectangle, scale, safe area, and orientation. |
| `renderer_backend` | `source_hook`, `disabled`, or an explicitly proven fallback backend. |
| `resolved_shader_file` | Existing Semu-owned shader/effect file, if selected. |
| `resolved_bezel_file` | Existing Semu-owned bezel asset, if selected. |
| `resolved_backend_config` | Generated hook/backend config consumed by the emulator hook or fallback wrapper. |
| `dependency_count` / `missing_dependency_count` | Recursive checks for files referenced by selected Semu shader/effect and bezel assets. |
| `*_status` | `ok`, `missing`, or `disabled`; missing means policy exists but the asset is not bundled or installed yet. |

The audit report is verification emulator state, not policy. It should be used
to decide which Semu-owned assets or Nix payloads need to be added; it should
not be edited by hand and does not directly change emulator-native config.

## Declarative Rendering Contract

Every current system has a Semu-owned rendering declaration under
`config/assets/systems/<system>.json`. The minimum contract is:

| Field | Meaning |
|---|---|
| `display_class` | Coarse hardware class: `handheld_lcd`, `crt_tv`, `dual_screen_lcd`, modern HD, or a dynamic TV/component variant. |
| `render_mode` | Normalized visual route such as `handheld_lcd`, `crt_4x3`, `dual_screen_stacked`, or `crt_4x3_or_widescreen_frame`. |
| `native_resolution` | The source panel or TV signal geometry that scaling decisions are based on. |
| `scale_policy` | Per-system scaling rule consumed by render plans and audits. |
| `integer_scaling_default` | Whether the global integer-scaling preference should apply before emulator/runtime constraints. |
| `renderer_backend` | Preferred generic backend: `source_hook`, proven fallback wrapper, or `disabled`. |
| `renderer_fallback` | The declared fallback when the preferred backend cannot be used. |
| `content_viewport_source` | Emulator probe used to locate the emulated content inside the emulator window. |

The shared policy lives in `config/assets/defaults.json`. Integer scaling is
still globally controlled by `visual_integer_scaling`, but each system declares
whether that means strict native multiples, integer when the layout fits, or
aspect-preserving output. GB/GBC/GBA use strict native LCD integer scaling. NES,
SNES, and Genesis prefer integer 240p scaling. N64 and PSX preserve variable
CRT-era aspect because internal resolution can change. Dreamcast, GameCube,
Wii, and PS2 preserve emulator-reported 4:3 or 16:9 output. DS uses stacked
integer LCD panels. 3DS keeps top/bottom geometry first and uses integer panel
multiples only when the combined layout fits. PSP prefers a 2x native LCD scale
on the Deck panel and falls back to aspect preservation.

The generic renderer backend is hook-scoped. `source_hook` is preferred whenever
Semu builds the emulator package. `vkBasalt` and gamescope ReShade are fallback
experiments only after a route has screenshot proof on the Deck. Wii U and
Switch keep classic treatment off by default unless their Semu declarations opt
in. `SEMU_RENDER_REQUIRED=1` is a verification mode that turns missing hook
evidence or fallback into a hard failure.

## Station Defaults

| System | Display Target | Shader Target | Bezel Target | Layout/Aspect |
|---|---|---|---|---|
| `gb` | DMG-01 reflective STN LCD | Green DMG tint, LCD grid, slow-pixel ghosting | Classic grey Game Boy | Single LCD, integer native, 10:9 |
| `gbc` | Game Boy Color TFT LCD | GBC LCD color and mild persistence | Frost purple Game Boy Color | Single LCD, integer native, 10:9 |
| `gba` | AGB-001 reflective TFT LCD | Original GBA color and LCD persistence | Purple wide Game Boy Advance | Single LCD, integer native, 3:2 |
| `nes` | Consumer CRT over composite | NES composite artifacts, phosphor mask, scanlines | Panasonic/Sony consumer CRT | 4:3 CRT |
| `snes` | Consumer CRT over S-Video/composite | Soft analog CRT scanlines | Panasonic/Sony consumer CRT | 4:3 CRT |
| `genesis` | Consumer CRT over composite/RGB | Composite dithering blend and scanlines | Panasonic/Sony consumer CRT | 4:3 CRT |
| `psx` | Consumer CRT over composite/S-Video | 240p/480i analog softness | Panasonic/Sony consumer CRT | 4:3 CRT |
| `n64` | Consumer CRT over composite/S-Video | CRT scanlines plus N64 VI softness | Panasonic/Sony consumer CRT | 4:3 CRT |
| `dreamcast` | CRT or VGA monitor | 480i/480p CRT/VGA behavior | CRT for 4:3, flat frame for VGA/wide | Dynamic 4:3 or 16:9 |
| `gc` | Consumer/component TV | 480i/480p TV output | CRT for 4:3, flat frame for widescreen | Dynamic 4:3 or 16:9 |
| `wii` | Consumer/component TV | 480i/480p TV output | CRT for 4:3, flat frame for widescreen | Dynamic 4:3 or 16:9 |
| `ps2` | CRT/component/early flat panel | 480i CRT scanlines and deinterlacing | CRT for 4:3, flat frame for widescreen | Dynamic 4:3 or 16:9 |
| `nds` | Dual 256x192 LCDs | Dual LCD grid and persistence | Maximized DS top/bottom bezel | Dual stacked LCD |
| `n3ds` | 400x240 top plus 320x240 bottom LCDs | 3DS LCD geometry | Maximized 3DS top/bottom bezel | Dual asymmetric LCD |
| `psp` | 480x272 PSP LCD | PSP LCD grid and color response | Red God of War PSP or original black PSP | Single 16:9 LCD |
| `wiiu` | HD TV plus optional gamepad screen | Modern output by default | Disabled by default; optional TV/gamepad layout | Dynamic 16:9 or dual |
| `switch` | Modern 16:9 handheld/docked display | Off by default | Off by default | Modern fullscreen |

For 4:3-era systems that can also run 16:9, Semu reads emulator/game config
through the emulator and broadcasts the resulting aspect as normalized render
state. The plan then exposes `effective_aspect`, the resolved content viewport,
the selected shader/effect, the selected bezel, and the backend config consumed
by the emulator's Semu render hook. The bezel choice follows the state: CRT for
4:3, flat or clean frame for widescreen.

### Renderer Backends

The renderer backend should stay inside the emulator content path whenever Semu
builds the emulator package. `source_hook` means a small Semu integration point
at the game framebuffer or final game-present layer. That hook consumes the same
generated rendering plan for every emulator, but it does not shade emulator
settings dialogs, frontend UI, or ES-DE control surfaces.

Preferred order:

1. Launch the emulator fullscreen with stable aspect/scaling and a known content
   viewport.
2. Use an emulator source hook that receives the game framebuffer before
   emulator UI/settings overlays are drawn.
3. Use the emulator's native content shader chain only when that is the stable
   source hook boundary, as with RetroArch shader presets.
4. Keep `vkBasalt`, gamescope ReShade, or a compositor wrapper as a documented
   fallback experiment only after a route has screenshot proof on the Deck.

The compositor/interceptor idea is intentionally a fallback. It can centralize
post-processing, but it sees windows instead of game framebuffers and already
proved fragile on the Deck for RetroArch Vulkan/GL launch paths.

The gamescope fallback is a launcher mode, not a blanket overlay:

```sh
gamescope -W 1280 -H 800 -f -b \
  --reshade-effect "$SEMU_SHADER" \
  -- "$SEMU_EMULATOR" "$SEMU_ROM"
```

This must be tested separately in Desktop Mode and Game Mode because the Deck
session already uses gamescope. Nested behavior, focus, overlays, and quit
handoff must be proven before this can become a default.

### vkBasalt Experiment

`vkBasalt` is useful only for Vulkan routes. The launcher can opt in with:

```sh
ENABLE_VKBASALT=1 VKBASALT_CONFIG_FILE="$SEMU_VKBASALT_CONFIG"
```

Neither backend is a universal solution. Both require the content viewport
resolver so effects and bezels align to the emulated picture instead of covering
the emulator window, ES-DE, or settings UI blindly.

## Bezel Strategy

Semu should model bezels as declarative rendering data:

| Field | Purpose |
|---|---|
| `system` | System id such as `snes`, `psx`, or `gc`. |
| `display_class` | Hardware class used to group LCD, CRT, dual-screen, and modern display behavior. |
| `aspect` | Native content shape: `4:3`, `3:2`, `8:7`, `16:9`, dual-screen, or custom. |
| `renderer.shader.effect_file` | Semu renderer effect for the content shader. |
| `shader_file` | Semu-owned shader/effect path such as a DMG LCD, GBC LCD, CRT, or PSP LCD effect. |
| `renderer.bezel.composition_effect_file` | Semu renderer effect that composes the bezel with the content shader. |
| `bezel_file` | Bezel artwork or layout data, editable per system. |
| `native_resolution` | Physical/source resolution used for viewport and integer-scaling decisions. |
| `scale_policy` | Scaling rule: strict integer, integer-when-fit, layout-aware integer, aspect-preserving, or modern fill/preserve. |
| `integer_scaling_default` | Boolean declaration of whether global integer scaling applies by default for the system. |
| `widescreen_shader_file` | Optional shader override when emulator state reports 16:9. |
| `widescreen_bezel_file` | Optional bezel override when emulator state reports 16:9. |
| `effective_aspect` | Runtime aspect after static policy and emulator probes are composed. |
| `render_mode` | Normalized mode such as `crt_4x3`, `widescreen_frame`, `handheld_lcd`, or `dual_screen`. |
| `renderer_backend` | Generic renderer route: `source_hook`, a proven fallback wrapper, or off by default. |
| `renderer_fallback` | What the launcher/audit should expect when the generic renderer is unavailable. |
| `host_render_mode` | Optional per-system override for the host wrapper, for example `nixgl_only` for modern systems. |
| `safe_area` | Viewport constraints for OLED and overscan-sensitive layouts. |
| `enabled` | Per-system on/off switch. |

No emulator route gets a special rendering path. A route can become
production only after screenshots prove the resolved content viewport is not
cropped, stretched, inverted, or hidden behind the bezel.

Default policy:

- `visual.integer_scaling=true`
- `visual.crt_shaders=true`
- `visual.bezels=true`
- `visual.bezel_policy=classic`
- Classic visual systems:
  `gb,gbc,gba,nes,snes,genesis,n64,nds,dreamcast,psx,ps2,psp,n3ds,gc,wii`
- Modern exclusions: `wiiu,switch`

## ES-DE Semu Settings Entry

Semu needs a real settings entrypoint inside ES-DE so configuration is
controller reachable on the Deck. This entry must live in ES-DE's actual
Start/Main Menu/Utilities settings path, like RetroArch's settings entry. A
pseudo-system, fake ROM folder, or game entry named Semu Settings is not an
acceptable endpoint.

The first shipped contract is BTRC UI-backed, CLI-backed, and file-backed:

```sh
semu settings get roms.dir
semu settings put roms.dir /run/media/deck/SD --apply
semu settings put visual.integer_scaling true
semu settings toggle visual.bezels --apply
semu settings put visual.bezels true
semu settings put sync.roms false
semu settings ui
semu settings apply
```

The CLI and terminal UI write project-local user config; Syncthing policy
defaults stay in Semu-owned `config/settings/sync.json`. They do not store
shadow state. The UI is dependency-free BTRC: it prints the active project and
the command contract for string edits, boolean toggles, and generated-file
apply.
The ES-DE menu entry should call this CLI/UI and then run `settings apply` or
pass `--apply` for mutations that require generated files to be rerendered.

Required generated shape:

- A Start/Main Menu/Utilities settings item named `Semu Settings` opens the Semu
  settings UI.
- The implementation must not add a generated ES-DE game system, fake ROM path,
  or `.semu` action files to the user's ROM tree.
- Keep all mutations declarative: update Semu-owned/project config, then run
  `settings apply`, `config apply`, or a command with `--apply`.

Configured settings action:

| Entry | Behavior |
|---|---|
| Semu Settings | Opens the Semu settings UI from ES-DE's real settings menu. |

The first ES-DE version can be an action menu backed by these BTRC commands.
A richer controller-first settings UI can follow once the Deck Game Mode input
path is proven.

## Decisions

- The bottom-left radial semantics stay constant across emulators.
- Semu uses source/content hooks for production shader/bezel rendering on every
  classic emulator route.
- Emulator-native shaders, overlays, and per-core renderer references are not the
  production rendering model.
- `vkBasalt`, gamescope ReShade, and `semu-render` stay fallback/prototype
  experiments and are not render proof for production readiness.
- Runtime render proof for standalone emulators must come from the hook writing
  `SEMU_RENDER_HOOK_PROOF` and/or the patched source emitting
  `semu-source-hook: applied`. Generated config alone is not proof.
- Settings are exposed through ES-DE as Semu-owned actions, not hidden in shell
  scripts or manual config edits.

## References

- [ValveSoftware/gamescope](https://github.com/ValveSoftware/gamescope)
- [DadSchoorse/vkBasalt](https://github.com/DadSchoorse/vkBasalt)
