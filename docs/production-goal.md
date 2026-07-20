# Semu Production Goal

This is the normative end state, not an implementation report. A code path may
be marked implemented in `todo.md` only after its focused automated contract
passes. A system is accepted only after the physical evidence required by
`acceptance-matrix.md` is retained from the same verified release.

## User Directives

These directives determine the architecture and order of work:

- "start with one emulator at a time bro, stop boiling the fucking ocean. get one working, then the next, etc."
- "don't use retroarch's built in shader mechanism, use the same system for absolutely everything so it's generic."
- "we are completely agnostic of x11, wayland, gamescope, etc."
- "remember that we should only be editing our owned files. those files should then be \"compiled\" into the files the emulators need."
- "Make hitting start select at roughly the same time quit the emulator immediately"
- "only do bazzite AFTER you've got everyrthing working on steam deck."
- "all the emulators are custom built by us and nix and published"
- "Just don't modify any of that stuff unless needed" (external ROM, BIOS, key, and save media)

## Compiler And Ownership

Semu must build a target from declarative Semu-owned definitions. The compiler
must parse, resolve, check, and generate emulator-native profiles, launchers,
ES-DE integration, Steam Input configuration, package inputs, and verification
plans without duplicating emulator or system facts in BTRC.

User changes belong in `$SEMU_HOME/semu.json` or
`$SEMU_HOME/overrides/**/*.json`. Emulator-native files are generated output,
never user-owned source. External ROM, BIOS, key, firmware, and existing save
trees are read-only. A necessary copy must be bounded, non-destructive, and
retain before/after hashes; it must never silently overwrite or delete media.

Every BTRC file must import its own dependencies and compile under strict-import
mode. The maintained tree must contain no retired runtime tree, display-server
capture implementation, compatibility alias, generated source, or dead package
path.

## Package And Deployment

The required Steam Deck artifact is one self-contained AppImage assembled from
separately reproducible, source-built Nix slices for ES-DE, Semu, RetroArch and
its selected cores, Dolphin, PPSSPP, Flycast, Azahar, melonDS, PCSX2, Cemu,
Ryujinx, the renderer, input runtime, rendering assets, and Syncthing. The ares
slice is required for the declared macOS N64 target. Exact artifacts must be
published with reproducible identities; host binaries and Flatpaks are not
production fallbacks.

Normal Deck launch must not require FUSE or per-launch extraction. Installation
must verify bytes, install an immutable digest-addressed release, expose a
regular stable launcher, retain one previous release, support rollback, and
remove obsolete Semu releases without touching external media. The default ROM
discovery must work with the existing Steam Deck SD-card layout and remain
configurable through Semu-owned settings.

## Rendering

One Semu renderer model must serve every emulator. Each source-built emulator
must call the same stable renderer ABI at two explicit emulator-owned points:

1. after final game pixels and geometry are available but before emulator UI;
2. after emulator UI, immediately before the emulator presents the frame.

The first hook shades and composites only active game surfaces. The second keeps
emulator settings, notifications, and menus above the game treatment. Production
rendering must not use GLX/EGL swap interposition, `LD_PRELOAD`, arbitrary-window
capture, gamescope effects, X11/Wayland discovery, or a separate overlay window.

The ordered game pipeline is: identify semantic game surfaces, preserve an
independent temporal history per surface, run the selected multi-pass Slang
preset, resolve integer/aspect scaling from native and live content geometry,
composite the selected high-resolution bezel/glass, then return control for
native emulator UI. Any shader failure must emit an explicit failing receipt and
passthrough safely rather than crash or claim success.

Required defaults are:

- GB: restrained olive DMG LCD response, ghosting, and a realistic gray DMG.
- GBC: authentic reflective LCD response and a frost/grape-purple GBC.
- GBA: AGB-001 LCD response and an indigo/purple wide GBA.
- NES, SNES, Genesis, N64, PSX: era-appropriate analog CRT treatment and a
  realistic Panasonic/Sony-style CRT.
- Dreamcast, GameCube, Wii, PS2: CRT/TV presentation with live 4:3 versus 16:9
  selection from emulator-reported content state.
- DS and 3DS: independent top/bottom surfaces, correct touch mapping, and
  high-quality layouts that maximize usable screen area.
- PSP: correct 480x272 treatment with a black or red God of War PSP shell.
- Wii U and Switch: modern clean output by default, without a legacy bezel.

The same declarations must adapt to Steam Deck and external display dimensions.
Asset hashes and geometric apertures are automated contracts; realism,
orientation, clipping, and UI layering require screenshot evidence from real
games.

## Input

Every system must accept the Steam Virtual Gamepad in gameplay. Start+Select in
either near-simultaneous order must immediately terminate the emulator process
group and return to ES-DE. The left trackpad must expose Steam Input radial menus
with semantic icons for quit, save, load, menu, and system-specific actions.

Wii must provide Wiimote, Wiimote+Nunchuk, Classic Controller, and GameCube
controller modes with a quick radial switch. Save/load evidence must prove an
actual state change and restoration where the emulator supports states. Desktop
key injection, generated VDF, and acknowledged commands do not replace physical
Game Mode input proof.

## Settings And Synchronization

Semu must provide a dependency-free BTRC settings UI and typed CLI over only
Semu-owned JSON. It must cover ROM roots, per-system rendering and bezel choices,
input behavior, Wii controller mode, appearance, Syncthing state/folders, and a
route to its loopback UI. Applying settings must regenerate outputs rather than
edit emulator-owned files in place.

ES-DE must show a visible nonblack splash and exactly one native Semu Settings
entry in its start menu, not a fake game system. The entry must invoke the
regular installed Semu launcher without FUSE. Syncthing must run as an isolated
user service and receive write access only to explicitly enabled folders.

## Acceptance Order

1. Build a fresh strict CLI, all package slices, and an exact-byte-verified
   AppImage.
2. Accept RetroArch systems one at a time: GB, GBC, GBA, NES, SNES, Genesis,
   N64, PSX, then the NDS fallback.
3. Accept Dolphin: GameCube, then Wii and every controller mode.
4. Accept PPSSPP, Flycast, melonDS, Azahar, PCSX2, Cemu, and Ryujinx one at a
   time; separately verify the declared macOS ares package path.
5. Accept ES-DE splash/settings and Syncthing on the same installed release.
6. Complete the full physical Steam Deck matrix. Run Bazzite parity only after
   every Deck gate passes.

## Evidence

Every physical row requires exact package and AppImage identity, physical
SteamOS/Deck/AMD identity, real read-only ROM and firmware inputs, unchanged
external-media hashes, launch receipts, frame-bound renderer receipts,
screenshots, nonblank and non-inverted game pixels, correct game geometry,
shader/bezel layering, gameplay input, save/load outcome where supported,
Start+Select and radial quit, emulator process exit, and ES-DE foreground return.

Unit tests, package source checks, synthetic frames, dry runs, process launches,
and command acknowledgments are necessary but never sufficient physical proof.
