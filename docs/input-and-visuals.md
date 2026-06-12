# Input, Shaders, Bezels, And Settings

This document defines the order of operations for the production Steam Deck
loop. Input must become boring before visual treatment gets clever.

## Implementation Order

1. Prove the Steam Deck input contract in Game Mode.
2. Use the shipped `semu settings list|get|put|ui|apply` CLI/UI as the
   mutation contract.
3. Add an ES-DE Semu Settings entrypoint for configuration actions.
4. Ship RetroArch-native shader and bezel presets for RetroArch-backed systems.
5. Prototype standalone emulator post-processing behind explicit feature flags.
6. Add broader bezel artwork once the wrapper path is proven per emulator.

## RetroArch Versus Semu Input

| Concern | RetroArch Native | Semu Contract |
|---|---|---|
| Source of truth | RetroArch input config, hotkey enable button, core overrides, shader presets, and overlays. | `input/keymaps/steam_deck.skm`, generated Steam Input VDF, emulator profile renderers, and `semu-quit-watch`. |
| Bottom-left trackpad | Can map a radial menu to RetroArch hotkeys, but it only applies to RetroArch cores. | Left trackpad radial emits Semu actions that every launcher receives. |
| Quit | Native `input_exit_emulator`, quit gamepad combo, or menu. | Radial quit emits `Select+Start`, `Esc`, `Ctrl+Q`, and `Alt+F4`; `semu-quit-watch` also observes evdev and terminates the routed child process group. |
| Save state | Native save-state hotkey and slots. | `state.save = Ctrl+S`; rendered into RetroArch plus standalone profiles that support save states. Unsupported emulators must hide or no-op the action explicitly. |
| Load state | Native load-state hotkey and slots. | `state.load = Ctrl+A`; same support rule as save state. |
| Menu | Native RetroArch menu toggle. | `ui.menu = Ctrl+M`; each emulator profile maps this to its own menu path where possible. |
| Screenshot | Native RetroArch screenshot hotkey. | `ui.screenshot = Ctrl+X`; screenshot hooks also capture before spawn, after spawn, and after exit when enabled. |
| Fullscreen | RetroArch video fullscreen config and hotkey. | Launchers force fullscreen/batch arguments and profile defaults for each emulator. |
| Visual filters | First-class Slang shaders, overlays, and per-core presets. | RetroArch systems use the native path; standalone systems need emulator-native options or an experimental wrapper. |
| Failure mode | Excellent for RetroArch systems, but does not cover Dolphin, PCSX2, PPSSPP, Cemu, Azahar, Ryujinx, or other standalone paths. | Covers every routed process, but each standalone emulator still needs a real Game Mode proof for focus, hotkeys, state behavior, and exit. |

The production target is not to imitate RetroArch everywhere. It is to expose a
single Semu controller contract and use the strongest native feature available
behind each emulator route.

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

## Input Hardening Gates

- `doctor` verifies the generated Steam Input VDF exists and contains the
  expected radial actions.
- `keymap validate` verifies the canonical action names and controller combos.
- A Game Mode test opens each emulator through ES-DE, launches one known-good
  ROM, confirms controller movement/buttons, opens the left-trackpad radial, and
  uses Quit to return to ES-DE.
- Save and load are only marked production-ready for an emulator after a real
  state file is created and reloaded from the radial path.
- Unsupported state actions are hidden or rendered as disabled settings entries
  instead of pretending every emulator has equivalent state semantics.
- Fullscreen is verified from screenshots for every emulator after launch and
  again after returning to ES-DE.

## Visual Pipeline

Semu now treats visuals as a station-owned presentation contract rather than an
emulator-owned trick. Global toggles live in `settings/semu-settings.json`;
per-system shader, bezel, layout, and adapter defaults live in
`settings/presentation/<system>.json`.

The command contract is:

```sh
semu presentation defaults
semu presentation list
semu presentation plan --system gb
semu presentation get gb shader_file
semu presentation put gb bezel_file bezels/gb/classic-grey-game-boy.json
```

`presentation plan` emits normalized JSON for launchers, future compositor
wrappers, and settings UIs. Emulators are adapters: they read and/or broadcast
their own config state, while Semu keeps the display policy stable.

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
| `wiiu` | HD TV plus optional gamepad screen | Modern output by default | Clean 16:9 or TV/gamepad layout | Dynamic 16:9 or dual |
| `switch` | Modern 16:9 handheld/docked display | Off by default | Off by default | Modern fullscreen |

For 4:3-era systems that can also run 16:9, Semu should read emulator/game
config through the adapter and broadcast the resulting aspect in the normalized
presentation state. The bezel choice then follows the state: CRT for 4:3, flat
or clean frame for widescreen.

### RetroArch Systems

RetroArch-backed systems should use RetroArch's native shader and overlay stack
first. This gives us real shader support without inventing compositor plumbing.

Production tasks:

- Bundle or fetch `libretro-shaders-slang` through Nix.
- Configure RetroArch for Vulkan where available and GLCore fallback.
- Use `settings/presentation/<system>.json` as the per-system shader/bezel map.
- Keep native integer scaling on by default for classic systems unless a
  specific shader preset requires exact viewport control.
- Enable shaders with `video_shader_enable = true`.
- Write per-system preset references into the generated RetroArch config.
- Keep a performance tier for the Steam Deck OLED panel, starting with lighter
  presets before expensive reflection presets.
- Treat Mega_Bezel presets as one backend. The normalized presentation plan is
  still Semu-owned so standalone emulators can consume the same policy later.

Mega_Bezel is attractive for 4:3 and CRT presentation because it provides CRT,
bezel, reflection, and scaling presets inside RetroArch. Its own setup guidance
requires RetroArch 1.9.8 or later, recommends Vulkan or GLCore, expects full
aspect scaling with integer scale off, and stores presets under
`shaders/shaders_slang/bezel/Mega_Bezel/Presets`.

### Standalone Emulators

Standalone visual treatment should be conservative until input is proven.

Preferred order:

1. Use emulator-native fullscreen, aspect, scaling, and shader/filter settings.
2. Try `gamescope` ReShade support as a per-emulator wrapper experiment.
3. Try `vkBasalt` for Vulkan-only routes where the emulator and driver path are
   compatible.
4. Consider a custom screen-buffer interceptor only if the above routes fail and
   the latency, focus, capture, and teardown costs are acceptable.

The compositor/interceptor idea is intentionally last. It could centralize
post-processing, but it also risks adding latency, focus bugs, Wayland/DRM
capture complexity, and another failure layer around every emulator.

### Gamescope Wrapper Experiment

The experiment should look like a launcher mode, not a new default:

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

It should not be treated as a universal solution. It does not cover OpenGL
emulators, and not every ReShade shader is compatible.

## Bezel Strategy

Semu should model bezels as declarative presentation data:

| Field | Purpose |
|---|---|
| `system` | System id such as `snes`, `psx`, or `gc`. |
| `aspect` | Native content shape: `4:3`, `3:2`, `8:7`, `16:9`, dual-screen, or custom. |
| `shader_backend` | `retroarch_slang`, `native_or_wrapper_crt`, `native_or_wrapper_lcd`, or `off_by_default`. |
| `shader_file` | Hardware-era shader path such as a DMG LCD, GBC LCD, CRT, or PSP LCD preset. |
| `bezel_backend` | `retroarch_mega_bezel`, `universal_crt_bezel`, `universal_dual_screen`, or `off_by_default`. |
| `bezel_file` | Bezel artwork or preset path, editable per system. |
| `runtime_preset` | Combined runtime preset when the backend needs shader plus bezel in one file. |
| `safe_area` | Viewport constraints for OLED and overscan-sensitive presets. |
| `enabled` | Per-system on/off switch. |

RetroArch bezels can become production before standalone bezels. Standalone
bezels require per-emulator screenshot proof that content is not cropped,
stretched, or hidden behind the bezel.

Default policy:

- `visual.integer_scaling=true`
- `visual.crt_shaders=true`
- `visual.bezels=true`
- `visual.bezel_policy=classic`
- Classic visual systems:
  `gb,gbc,gba,nes,snes,genesis,n64,nds,dreamcast,psx,ps2,psp,gc,wii`
- Modern exclusions: `n3ds,wiiu,switch`

## ES-DE Semu Settings Entry

Semu needs a settings entrypoint inside ES-DE so configuration is controller
reachable on the Deck.

The first shipped contract is BTRC UI-backed, CLI-backed, and file-backed:

```sh
semu settings list
semu settings get roms.dir
semu settings put roms.dir /run/media/deck/SD --apply
semu settings put visual.integer_scaling true
semu settings put visual.bezels true
semu settings put sync.roms false
semu settings ui
semu settings apply
```

The CLI and terminal UI write `settings/semu-settings.json` and
`sync/sync.json`; they do not store shadow state. The UI is dependency-free
BTRC: numbered rows edit string settings, toggle boolean settings, and apply
by running lifecycle reconfigure. ES-DE entries should call this CLI/UI and then
run `settings apply` or pass `--apply` for mutations that require generated
files to be rerendered.

Implemented shape:

- A pseudo-system named `Semu Settings` is appended to generated ES-DE systems.
- Settings launch entries are generated under `settings/es-de`, outside the SD
  ROM tree.
- `%EMULATOR_SEMU_SETTINGS%` routes each `.semu` action file to
  `semu settings entry`.
- Keep all mutations declarative: update config files, then run lifecycle
  reconfigure.

Initial settings actions:

| Entry | Behavior |
|---|---|
| Change ROM Location | Opens a picker or preset list, writes the ROM root, and rerenders ES-DE paths. |
| Syncthing Status | Shows service state and declared folders. |
| Open Syncthing UI | Starts/opens the bundled Syncthing web UI. |
| Sync Folders | Enables/disables declared folders such as saves, states, screenshots, BIOS, ROMs, and emulator state. |
| Toggle CRT Shaders | Enables/disables RetroArch shader presets globally or per system. |
| Toggle Bezels | Enables/disables bezel presentation globally or per system. |
| Input Test | Opens a controller test route that records face buttons, trackpads, radial actions, and quit. |
| Doctor | Runs `semu doctor` and shows a readable result. |
| Rebuild ES-DE Config | Runs lifecycle reconfigure after settings changes. |

The first ES-DE version can be an action menu backed by these BTRC commands.
A richer controller-first settings UI can follow once the Deck Game Mode input
path is proven.

## Decisions

- The bottom-left radial semantics stay constant across emulators.
- RetroArch-native shaders and overlays are the first production visual path.
- Mega_Bezel is supported through RetroArch, not by trying to port its shader
  graph into Semu.
- `gamescope` ReShade and `vkBasalt` are opt-in experiments until real Deck
  screenshots prove them per emulator.
- Settings are exposed through ES-DE as Semu-owned actions, not hidden in shell
  scripts or manual config edits.

## References

- [HyperspaceMadness/Mega_Bezel](https://github.com/HyperspaceMadness/Mega_Bezel)
- [ValveSoftware/gamescope](https://github.com/ValveSoftware/gamescope)
- [DadSchoorse/vkBasalt](https://github.com/DadSchoorse/vkBasalt)
