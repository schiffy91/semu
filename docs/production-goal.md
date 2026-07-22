# Semu Production Goal

This is the normative end state, not an implementation report. A code path may
be marked implemented in `todo.md` only after its focused automated contract
passes. A system is accepted only after the physical evidence required by
`acceptance-matrix.md` is retained from the same verified release.

> **Durable goal:** Iteratively make every emulator and every served system
> production-ready on the user's physical Steam Deck. Keep one system active
> until a real ROM, all gameplay input, every exposed shader/bezel/layout/
> aspect/integer-scale/controller variation, save/load, the actual semantic
> left-trackpad radial, both Start+Select quit orders, fullscreen geometry, and
> return to ES-DE are proven on one exact release. Visual and radial behavior
> require inspected physical screenshots; state, process, input, geometry,
> artifact, ROM, and protected-media behavior require programmatic receipts.
> Continue through every row and then rerun the entire matrix on one clean
> pushed aggregate release.

## Authoritative Outcome

Iteratively make every declared emulator and every served system work on the
user's physical Steam Deck, then prove it. "Work" means a real ROM launches from
ES-DE in Game Mode; gameplay input, save/load, fullscreen and adaptive geometry,
the actual left-trackpad radial, both immediate-quit chord orders, and return to
ES-DE behave correctly; and the default, disabled, and every exposed alternate
shader, bezel, layout, aspect, integer-scale, and controller mode are exercised.
Every visual combination requires a run-bound physical screenshot plus renderer
and geometry receipts, and every screenshot must be inspected. Programmatic
checks strengthen this physical proof but never replace it.

The unit of progress is one accepted physical system row, not code volume,
elapsed time, local tests, package evaluation, or deployment alone. Keep the
earliest unaccepted row active until its complete evidence bundle passes. Then
advance to the next row and repeat. After all rows pass incrementally, rebuild
one exact clean pushed aggregate release and rerun the complete physical matrix
against that same artifact.

## Current Execution State

This block is the live physical status and must be updated whenever an
acceptance row changes state:

- Physically accepted systems: **0 of 17**.
- Accepted frontend rows: **0 of 1**.
- Active system: **Game Boy through RetroArch/Gambatte**.
- Exact accepted aggregate release: **none**.
- Current admissible milestone: build, verify, deploy, and physically accept the
  complete Game Boy row. Game Boy Color must not start first.
- Next user-visible result: a verified RetroArch/Game Boy AppImage slice running
  Tetris from ES-DE in Game Mode with the complete input, rendering-variant,
  radial, save/load, quit, screenshot, and return-to-frontend record.

This state is intentionally fail-closed. No emulator or system may be described
as working until its physical row is accepted. For every row, Semu must resolve
the complete ROM/mode and settings matrix from owned declarations, exercise each
required real ROM and every exposed input, shader, bezel, layout, aspect,
integer-scale, radial, controller, save/load, and quit choice on the Deck, retain
the corresponding screenshots and programmatic receipts, inspect the visual
evidence, and fix the same row until it passes. The next row does not begin while
any one of those checks remains open.

## Non-Negotiable Iteration Contract

The deliverable is not infrastructure or a plausible configuration. It is a
fully working product, proven one system at a time on the user's physical Steam
Deck. The main agent must keep executing the active row until it is accepted;
it must not stop after analysis, local tests, a package build, deployment, or a
status report.

The physical acceptance order is fixed:

1. Game Boy (RetroArch/Gambatte)
2. Game Boy Color (RetroArch/Gambatte)
3. Game Boy Advance (RetroArch/mGBA)
4. NES (RetroArch/Mesen)
5. SNES (RetroArch/Snes9x)
6. Genesis (RetroArch/Genesis Plus GX)
7. Nintendo 64 (RetroArch/Mupen64Plus-Next)
8. PlayStation (RetroArch/Beetle PSX)
9. Nintendo DS (melonDS primary and configured RetroArch fallback)
10. GameCube (Dolphin)
11. Wii (Dolphin)
12. Dreamcast (Flycast)
13. PlayStation 2 (PCSX2)
14. PSP (PPSSPP)
15. Nintendo 3DS (Azahar)
16. Wii U (Cemu)
17. Switch (Ryujinx)

For each row, the agent must implement, test, build, independently verify,
deploy, launch through ES-DE in Game Mode, exercise, capture, inspect, and
retain evidence before changing the active row. Shared emulator work may be
implemented once, but every served system remains a separate real-ROM
acceptance transaction. A failure restarts the same row from the earliest
invalidated stage; it never permits skipping forward.

When the Deck is reachable, deploy and exercise the next runnable exact artifact
as soon as the active row's blocking checks pass. Do not defer that physical run
to complete unrelated architecture, generic harness, another emulator, or
Bazzite work. Each failed physical attempt must be recorded as a concrete active
row blocker in `todo.md`, fixed at the owning layer, rebuilt, redeployed, and
retested. Status requests and resume messages report or restore this loop; they
do not pause or complete it.

Every row must prove all applicable behavior below:

- one deterministic real ROM reaches stable fullscreen gameplay on the exact
  deployed release without mutating protected media;
- every required gameplay control is consumed by the running game, not merely
  present in a generated mapping;
- default, disabled, and every exposed alternate shader, bezel, layout, aspect,
  and controller mode work, including every combination the settings UI allows;
- each visual combination has a run-bound physical Deck screenshot, renderer
  receipt, geometry/orientation/clipping assertions, and an inspection verdict;
- the left-trackpad radial visibly opens with semantic icons and every applicable
  action works, including Wii controller-mode switching;
- save/load restores a visibly and byte-distinct state where supported;
- Start+Select works in both event orders, terminates the whole emulator process
  group, and returns ES-DE to the foreground; and
- artifact, ROM, process, input, renderer, screenshot, and protected-media
  identities are bound into one non-stale evidence manifest.

Unattended automation is encouraged, but it must traverse production behavior.
A programmatic controller may count only when it is a truthfully labelled
synthetic-uinput source injected before Steam Input in the physical Deck's Game
Mode session, observed through the exact target-declared Steam Input virtual
output identity, and correlated with game frame/state changes. Direct emulator
input injection, desktop keystrokes, and
generated mappings do not count. Rendering and radial behavior always require
actual Deck screenshots; programmatic pixel checks supplement rather than
replace main-agent inspection.

Gameplay correlation must distinguish the requested action from ordinary game
animation. For each action, restore one declared real-game probe state, observe
a fixed no-input control trajectory, restore the same state, inject exactly that
action through the admitted Deck input path, and require the declared
action-specific state or frame divergence over the same interval. Probe state,
timing, expected observation, and any additional title needed for a hardware
mode belong in the resolved system definition. Generic nonblank pixels, any
frame change, emulator input logs, or a generated mapping cannot issue
`consumed=1` by themselves.

No receipt may waive request-side requirements. Automated evidence is never
eligible for the actual left-trackpad radial, and a physical operator addendum
must remain bound to the same release, system, ROM, Game Mode session, and
acceptance transaction. Every failure after launch must terminate and reap the
whole emulator process group, stop synthetic input, restore Semu-owned settings,
and leave protected media unchanged before the same row is retried.

After all 17 incremental rows and the ES-DE frontend row pass, assemble one
clean pushed aggregate release and rerun the entire physical matrix against
that exact digest. The goal remains incomplete until that aggregate rerun passes
and all evidence is retained. Bazzite remains forbidden until then.

Implementation, automated verification, artifact verification, deployment, and
physical acceptance are separate states. Never collapse them into one status or
describe generated configuration, a passing fixture, a package build, or an
uninspected screenshot as a working system.

## Goal Mode Operating Contract

This document is the durable completion contract for a Codex goal. The active
Goal-mode prompt should remain short and point here, to `todo.md`, and to
`acceptance-matrix.md`. On every start, resume, interruption, or context
compaction, the agent must reread those three files before deciding what remains.
The newest explicit user instruction wins; when it changes scope or acceptance,
update these documents before continuing implementation.

The main agent owns the critical path: architecture, integration, the aggregate
AppImage, the physical Steam Deck session, deployment, acceptance decisions,
and final review. It may delegate bounded independent work, but must keep these
coordination rules:

- Use subagents for disjoint package slices, focused tests, read-only source
  review, log analysis, or screenshot/evidence review.
- Give every writing agent an isolated worktree and a non-overlapping file set.
- Never let two agents edit the same integration surface or control/deploy to
  the physical Deck concurrently.
- Do not assign one writing agent per system when systems share an emulator.
  Build the common emulator path once, then validate its systems sequentially.
- Independently red-team each emulator slice before accepting its package or
  physical evidence. Findings become blocking TODO items, not prose to ignore.
- Close completed agents after their result is reviewed and incorporated.

Work is strictly iterative. Select the earliest unaccepted emulator slice in
the acceptance order and execute this complete loop before advancing:

1. Reproduce and pin the current failure with a focused contract.
2. Implement the smallest coherent compiler, configuration, package, or runtime
   change that owns the behavior.
3. Run strict-import tests and the focused adversarial contracts.
4. Build and install-check the exact x86_64 Nix package from a frozen revision.
5. Red-team the source hook, input path, package provenance, and tests.
6. Assemble an incremental exact-provenance AppImage and verify its bytes.
7. Deploy that exact artifact transactionally to the physical Deck.
8. Launch real ROMs through ES-DE in Game Mode and collect run-bound evidence.
9. Inspect screenshots and logs rather than accepting file existence or command
   acknowledgements as proof.
10. Fix every failed gate and repeat the same slice. Commit and push only a
    coherent passing checkpoint, then advance.

### Physical-First Execution Invariant

Progress is measured by accepted behavior on the physical Steam Deck, not by
infrastructure, generated files, package evaluation, or elapsed work. Before
the first physical system is accepted, generalized refactoring or hardening is
allowed only when it directly blocks building, deploying, launching, or safely
testing that current system. The active slice is RetroArch/Game Boy until its
entire row passes; work must not broaden to another emulator or system first.

Every system iteration must use one exact built and deployed release and prove,
through a real read-only ROM launched from ES-DE in Game Mode:

1. bounded startup reaches nonblank, upright, correctly framed game pixels;
2. every required Steam Deck gameplay control produces the intended game input;
3. the system default shader and bezel are active only over game content, adapt
   to live output geometry, and match the declared hardware presentation;
4. every declared user-selectable shader, bezel, layout, and aspect variant can
   be applied and is verified rather than merely listed in configuration; each
   non-modern presentation must offer disabled, its production default, and at
   least one materially distinct high-quality shader and bezel alternative
   wherever that hardware has a declared display or shell treatment;
5. save and load restore a demonstrably changed state where supported;
6. the semantic left-trackpad radial exposes working quit, save, load, menu,
   and system-specific actions with relevant icons;
7. Start+Select in either near-simultaneous order immediately terminates the
   complete emulator process group and returns ES-DE to the foreground; and
8. screenshots, renderer/input/process receipts, logs, artifact identity, and
   before/after protected-media hashes are retained and inspected.

ROM coverage means one deterministic real baseline ROM for every system, plus
additional real titles whenever a declared aspect, screen layout, accessory, or
controller mode cannot be exercised by the baseline. Synthetic cores and ROMs
remain regression fixtures and cannot satisfy physical acceptance.

Programmatic assertions may prove control flow, geometry, hashes, and process
state, but they never replace inspected screenshots for visual behavior or real
Game Mode controller evidence. A failed item keeps the current system active;
the agent fixes and redeploys that slice before advancing. Shared emulator work
is implemented once, then each served system is physically accepted in order.

A status request does not end or satisfy the goal. Report exact implemented,
built, deployed, and physically verified counts, name the current failing gate,
then continue unless the user explicitly pauses the goal. Device unavailability
does not justify idle waiting: continue bounded package, test, review, and
artifact work that does not fabricate physical evidence.

The goal is complete only when every required item in `todo.md` and every row in
`acceptance-matrix.md` is supported by retained evidence from one exact pushed
release. A package build, static contract, synthetic frame, generated plan, or
older deployment cannot substitute for that physical matrix. Bazzite remains
out of scope until the complete Deck matrix passes.

### Per-System Acceptance Transaction

Every system is an independent acceptance transaction, even when several
systems share one emulator package. The active system remains the only physical
work item until all of the following steps pass on the Deck:

1. Freeze the source revision, Nix derivations, AppImage digest, resolved system
   definition, ROM identity, and before-hashes for protected external media.
2. Install that exact AppImage through the production installer and prove the
   stable regular launcher resolves to the expected immutable release.
3. Launch one deterministic real ROM from ES-DE in Game Mode, with the ROM and
   supporting BIOS, key, and firmware inputs mounted read-only wherever the
   emulator permits it.
4. Prove bounded startup, nonblank upright game pixels, fullscreen behavior,
   correct game rectangles, UI-above-game layering, and return-to-frontend
   process state with both programmatic receipts and inspected screenshots.
5. Exercise every required gameplay action through the physical Deck's exact
   target-declared Steam Input virtual gamepad output. A generated mapping,
   injected desktop key, or command acknowledgement is not gameplay evidence.
6. Enumerate the system's selectable shader, bezel, layout, aspect, and
   controller variants from the resolved Semu definition. Apply every exposed
   choice and every combination the UI permits through the owned settings
   interface, regenerate owned outputs, launch it, and retain an inspected
   screenshot or real controller observation as appropriate. Choices that
   cannot work must not be exposed.
7. Prove save/load by changing visible game state, saving, changing it again,
   loading, and observing restoration. Where the emulator has no supported
   state operation, retain an explicit capability receipt instead of inventing
   success.
8. Open the actual left-trackpad radial in Game Mode, capture its semantic icons,
   and execute quit, save, load, menu, and every system-specific action. For Wii,
   this includes switching among Wiimote, Wiimote+Nunchuk, Classic Controller,
   and GameCube Controller during real gameplay.
9. Execute Start-then-Select and Select-then-Start within the declared chord
   window in separate launches. Each order must terminate the whole emulator
   process group immediately and restore ES-DE to the foreground.
10. Rehash protected media, inspect every screenshot and receipt, reject stale
    or cross-run evidence, and store the accepted bundle under
    `build/verification/steamdeck/<release>/<system>/<run>/`.

Visual acceptance always requires screenshots captured from the physical Deck.
Programmatic checks additionally prove artifact identity, timing, geometry,
pixel orientation/nonblank bounds, process ownership, state-byte changes, and
media immutability. Neither evidence class substitutes for the other where both
are required. Synthetic cores and fixtures are regression tests only.

Use one existing ROM per system as the deterministic baseline and add another
only when a declared mode cannot be exercised otherwise. Never rewrite the ROM
library. The sole anticipated copy is the already requested 3DS replacement;
it must be non-overwriting, bounded to the selected decrypted ROM, and verified
byte-for-byte before the destination is used.

Only after the active row is accepted may work advance to the next system in
the order below. After all incremental rows pass, compose one aggregate release
and rerun the complete physical matrix against that exact pushed artifact; an
incremental pass from an older artifact does not satisfy final acceptance.

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

For each non-modern system, the curated settings surface must include disabled,
the production default, and at least one materially distinct high-quality
alternative for each applicable shader and bezel dimension. These choices and
their compatibility constraints are compiler inputs, not harness constants.
The physical matrix exercises every choice and every combination the settings
UI permits; options that cannot be rendered correctly are removed rather than
left as unverified decoration.

The same declarations must adapt to Steam Deck and external display dimensions.
Asset hashes and geometric apertures are automated contracts; realism,
orientation, clipping, and UI layering require screenshot evidence from real
games.

## Input

Every system must accept the target-declared Steam Input virtual gamepad output
in gameplay. Start+Select in
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

1. Build a fresh strict CLI and the smallest exact-byte-verified incremental
   RetroArch AppImage needed for the first physical slice.
2. Accept RetroArch systems one at a time without leaving a failed row: GB,
   GBC, GBA, NES, SNES, Genesis,
   N64, PSX, then the NDS fallback.
3. Build and accept Dolphin: GameCube, then Wii and every controller mode.
4. Build and accept PPSSPP, Flycast, melonDS, Azahar, PCSX2, Cemu, and Ryujinx
   one emulator slice at a time; separately verify the macOS ares package path.
5. Compose the accepted slices into the aggregate AppImage, then accept ES-DE
   splash/settings and Syncthing on that installed release.
6. Rerun and retain every physical row from that one exact pushed aggregate
   release. Run Bazzite parity only after every Steam Deck gate passes.

## Evidence

Every physical row requires exact package and AppImage identity, physical
SteamOS/Deck/AMD identity, real read-only ROM and firmware inputs, unchanged
external-media hashes, launch receipts, frame-bound renderer receipts,
screenshots, nonblank and non-inverted game pixels, correct game geometry,
shader/bezel layering, gameplay input, save/load outcome where supported,
Start+Select and radial quit, emulator process exit, and ES-DE foreground return.

Unit tests, package source checks, synthetic frames, dry runs, process launches,
and command acknowledgments are necessary but never sufficient physical proof.
