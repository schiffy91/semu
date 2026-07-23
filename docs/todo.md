# Semu Production TODO

This checklist is subordinate to `production-goal.md` and
`acceptance-matrix.md`. `[x]` means the narrowly worded automated contract was
executed successfully against the current tree. It never means physical
acceptance. Every physical requirement remains unchecked until retained evidence
from one verified release proves it.

## Physical Status Ledger

- Accepted systems: **0 / 17**.
- Accepted frontend rows: **0 / 1**.
- Active row: **RetroArch / Game Boy**.
- Verified and deployed artifact for the active row: **none**.
- Accepted aggregate release: **none**.
- Active execution directive: get Game Boy working imperatively on the physical
  Deck first. Build, deploy, direct-launch Tetris, inspect type-3 screenshots,
  exercise input/save/load/quit and visual variants, and fix the product before
  doing more generic harness or compiler architecture work. After it works,
  encode the proven commands and values declaratively and rerun them.
- Durable directive refreshed 2026-07-21: do not advance from the active row
  until its real-ROM Game Mode transaction proves every required control,
  shader/bezel/layout/aspect/integer-scale combination, save/load path, radial
  action and semantic icon, quit order, fullscreen/geometry property, and
  ES-DE return with inspected screenshots and independently checked receipts.
- Live Deck preflight (2026-07-21): SSH reachable in Game Mode, the
  `semu-keep-awake.service` user inhibitor is active, `IdleHint=no`, OLED
  brightness is `1 / 599000`, and Gamescope is running. ES-DE is not running.
  This transient preflight is not release-bound physical acceptance evidence.
- Native Game Mode capture preflight produced a stable 1280x800 RGBA PNG with
  matching remote/local SHA-256, but the idle frame was exactly black. Capture
  transport and stabilization therefore pass while visual admission fails; the
  file is preflight-only and cannot enter a system evidence bundle. A physical
  run must first wake/focus the real Game Mode surface through the admitted
  input path, then require a fresh nonblack renderer-bound screenshot.

- Current build checkpoint (2026-07-23): BTRC is pinned to `964ffe7`. The
  source-patched RetroArch/renderer integration, strict compiler suite, full
  AppImage trust/transaction/assembly contracts, Deck harness/deployment
  contracts, and real Linux installer transaction pass against the current
  tree. The native offline worker now admits only the host-sealed source
  contract whose release inventory, target, platform, Nix system, runtime
  slices, store path, and assembly authority agree; it no longer attempts an
  impossible networked Nix re-evaluation inside the offline worker. No current
  AppImage is deployable. A clean pushed checkpoint, fresh x86_64 RetroArch
  runtime build, exact artifact verification, and the complete physical Game
  Boy transaction remain pending.

Keep these counts synchronized with `acceptance-matrix.md`. A row may advance
only in this order: implemented, focused contracts passed, exact x86_64 artifact
verified, exact digest deployed, real ROM exercised in Game Mode, evidence
inspected, accepted. No earlier state implies a later one.

The required queue is GB, GBC, GBA, NES, SNES, Genesis, N64, PSX, NDS,
GameCube, Wii, Dreamcast, PS2, PSP, 3DS, Wii U, then Switch. The ES-DE frontend
row is a shared prerequisite and is reverified with the final aggregate release.
Do not activate the next row until every active-row checkbox and evidence field
passes. Automation may drive a labelled synthetic-uinput controller through the
real Deck Game Mode and Steam Input path; direct emulator injection does not
satisfy physical input or radial acceptance.

Whenever the Deck is reachable, the next exact runnable active-row artifact is
deployed and tested before unrelated refactoring or another system is started.
A failed physical check is added below as the next owning-layer blocker, then
the same row is rebuilt, redeployed, and rerun. Resume and status requests do
not change this continuation rule.

## Active Physical Slice

- [ ] RetroArch/Game Boy is the only active system slice. Produce its exact
  incremental x86_64 AppImage, deploy it to the physical Steam Deck, launch a
  real read-only ROM from ES-DE in Game Mode, and retain inspected evidence for
  pixels, gameplay input, default and selectable shader/bezel variants,
  geometry, save/load, semantic radial actions, both Start+Select orders,
  process exit, and ES-DE foreground return before advancing to Game Boy Color.
- [ ] Keep non-slice infrastructure work bounded to blockers that directly
  prevent the active Game Boy build, deployment, launch, or safe verification.
- [ ] Build and independently verify the exact x86_64 RetroArch/Game Boy
  incremental AppImage from one frozen revision; record derivations, closure,
  source revision, and AppImage SHA-256.
- [ ] Install that exact digest transactionally on the Deck and prove the stable
  no-FUSE launcher resolves to its immutable release.
- [ ] Before staging, prove the exact artifact, bootstrap, extracted candidate,
  retained rollback release, and transaction overhead fit the Deck filesystems
  with a bounded safety margin. The current Deck has approximately 24 GiB free
  under `/home` and 7.2 GiB in `/tmp`; first-install bootstrap must not blindly
  duplicate and extract the AppImage in tmpfs or delete the prior release first.
- [ ] Launch one deterministic read-only Game Boy ROM through ES-DE in Game Mode
  and prove bounded startup, nonblank upright fullscreen pixels, correct game
  aperture, and emulator UI above the Semu treatment.
- [ ] Exercise every Game Boy gameplay control through the target-declared Steam
  Input virtual gamepad output and prove there is no controller-autoconfiguration
  toast.
- [ ] Apply and inspect the default plus every exposed Game Boy shader, bezel,
  layout, and aspect choice. Store a run-bound Deck screenshot and renderer
  receipt for each visual combination the settings UI exposes.
- [ ] Prove visible-state save/load restoration, then open and inspect the real
  left-trackpad radial and execute each semantic action using its relevant icon.
- [ ] In separate real launches, prove Start-then-Select and Select-then-Start
  terminate the complete RetroArch process group and return ES-DE to foreground.
- [ ] Retain the complete Game Boy evidence bundle under the exact release/run
  identity, inspect it, confirm protected-media before/after hashes match, and
  only then mark Game Boy accepted and activate Game Boy Color.

## Active Game Boy Acceptance Blockers

- [ ] Bind build, independent verification, deployment, installation, and Deck
  launch to one private immutable AppImage snapshot. Verify its expected digest
  before any AppImage execution and never reopen the mutable source pathname.
- [ ] Retain an artifact-digest-addressed proof bundle containing the source
  revision, package attribute, emulator slice, Nix derivations and closure,
  worker diagnostics, independent host hashes, and final AppImage digest.
- [ ] Make the incremental Deck path explicitly select
  `steamdeck-runtime-retroarch`, `Semu-retroarch-x86_64.AppImage`, and
  `--emulator retroarch`; aggregate defaults must not silently substitute.
- [ ] Run Game Boy acceptance as one release/system/run transaction rather than
  three unrelated automated and operator runs. Reject stale or cross-run files.
- [ ] Extend the resolved Game Boy visual choices to disabled, production
  default, and at least one materially distinct high-quality alternative for
  both shader and bezel. Exercise every shader/bezel/integer-scale combination
  the settings UI permits, with explicit variant identity in every receipt.
- [ ] Delay every gameplay screenshot until the renderer reports the declared
  capture frame and nonblank game pixels; prove the 160x144, 10:9 aperture,
  upright orientation, fullscreen framing, and UI-above-game z-order.
- [ ] Treat Game Mode screenshot capture as asynchronous: wait boundedly for a
  fresh regular PNG whose bytes and size have stabilized, bind its final digest
  to the run, and reject stale, partial, replaced, wrong-dimension, or all-black
  captures. Wake the Game Mode session through an admissible input path before
  visual capture; command success alone is not screenshot evidence.
- [ ] Request an explicit Gamescope screenshot type for every test-only capture.
  The Deck's Gamescope 3.16.23.4 defaults `gamescopectl screenshot` to type 1,
  which removes overlay layers and produced an all-black 1280x800 image on the
  idle Steam UI. Radial and frontend evidence must use full-composition type 3,
  bind that type into the run receipt, and prove nonblack overlay pixels while a
  known foreground surface exists; base-plane captures cannot satisfy UI or
  radial evidence.
- [ ] Physically exercise D-pad up/down/left/right, A, B, Start, and Select on the
  exact target-declared Steam Input virtual output. Treat Steam's virtual
  controller as the hardware path;
  injected desktop keys and generated mappings remain nonphysical evidence.
- [ ] Prove each gameplay action with a declared same-state differential probe:
  restore, observe a fixed no-input control trajectory, restore again, inject
  exactly one action through synthetic-uinput and Steam Input, then require its
  action-specific state/frame divergence. Reject generic animation as evidence.
- [ ] Replace the test controller's Valve VID/PID impersonation with a clearly
  identified synthetic source injected before Steam Input, then correlate each
  source event with the Deck's single existing 28de:11ff virtual gamepad
  output. The current Deck names that exact output
  `Microsoft X-Box 360 pad 0`; accepted names and device facets must come from
  target-owned input JSON rather than harness constants. A fabricated 28de:11ff
  output or 28de:1205 command facet is direct
  injection and cannot satisfy gameplay, quit, or radial acceptance.
- [ ] Capture the actual left-trackpad quick and menu radials after they become
  visible, inspect their semantic icons, and execute every GB-applicable action.
  Remove the inapplicable `screen.swap` action instead of exposing dead UI.
- [ ] Prove Start-then-Select and Select-then-Start in separate launches with
  distinct process identities, chord ordering/timing, whole-process-group exit,
  and ES-DE foreground restoration.
- [ ] Make acceptance fail closed: no receipt may waive physical radial or other
  request-side requirements, no invalid transaction may fall back to a legacy
  engine, every evidence-path ancestor must be canonical and nonsymlinked, and
  every post-launch failure must terminate/reap the game process group, stop
  synthetic input, and restore Semu-owned settings.
- [x] Replace the legally risky untracked Duimon-derived Game Boy shell and
  glass inputs with original Semu-owned RGBA assets, pin their exact dimensions
  and hashes, and prove the renderer consumes the nonconstant glass map only
  over game pixels. Physical quality remains part of the visual matrix below.
- [ ] Fix the Game Boy bezel canvas policy before building the active slice. At
  1280x800, the prior minimum-aperture policy enlarged the portrait canvas past
  the viewport and clipped the shell. The
  resolved declarative layout must preserve the realistic gray handheld while
  producing at least a 2x integer game surface on Deck, adapt at the declared
  external-display viewport, and pass an inspected composition screenshot.
  The strict compiler/compositor contract now keeps the complete 1024x1536 DMG
  canvas inside every viewport and maps its declared 10:9 opening to an exact
  320x288 game surface on the Deck without enlargement or clipping; Studio and
  1080p remain contain-fit. The physical screenshot and renderer receipt remain
  outstanding, so this item stays open.
- [ ] Prove RetroArch continues the game treatment while its native menu is
  alive, draws the native menu above that treatment, suppresses game-pointer
  publication during the menu, and resumes without a stale map. The pinned
  source-built x86_64 integration now passes with a menu-time post-UI capture
  (`menu_difference=26.13`, `renderer_menu_difference=26.14`); the physical
  Game Boy screenshot and controller observation remain required.
- [ ] Keep the deterministic Game Boy ROM read-only, hash it and the protected
  media sentinel before and after the transaction, and retain both results.
- [x] Preselect the existing read-only Game Boy baseline
  `gb/Tetris (World) (Rev 1).zip`, observed on the Deck with SHA-256
  `deb006f1890d93489e7470a2e7973fd286b84672510c9e1923d690a9315cc84a`.
  Repeat the hash inside the final acceptance transaction; this preselection is
  not physical acceptance evidence.
- [x] Preflight the optional read-only
  `gb/Pokemon - Blue Version (USA, Europe) (SGB Enhanced).zip` fallback. Tetris
  consumes all eight declared controls, so the active row should remain a
  single-ROM transaction unless calibration disproves that path. The Mac and
  Deck Pokemon copies both hash to
  `874a05731292370604f52f1367a99d3f89c5bf201e6321a935667d60e4d8839f`.
  Repeat the identity inside the final transaction; this hash preflight is not
  gameplay or physical acceptance evidence.

## Immediate Build Blockers

- [x] Make `nix flake check . --no-build --all-systems` evaluate every declared
  package, app, shell, and check without invalid source or patch paths.
- [x] Make the compiler and Deck acceptance planner consume RetroArch's
  declarative conditional `surface_modes` without weakening strict rendering
  schema validation or duplicating system facts in BTRC.
- [x] Replace RetroArch's optional context-driver-data identity with an owned GL
  context generation that invalidates Semu resources before context destruction;
  prove context A -> destroy -> context B while driver userdata is retained.
- [x] Gate RetroArch's dual-screen path on a generated declarative layout value,
  using 256x384 geometry only as validation; prove an identical non-DS frame
  remains a single game surface.
- [x] Make RetroArch pointer sampling independent of the libretro core's X/Y/
  PRESSED query order, invoke renderer mapping outside `VIDEO_DRIVER_LOCK`, and
  cover capture, drag, release, outside presses, and menu transitions.
- [x] Execute a headless llvmpipe RetroArch test with a synthetic libretro core
  that renders asymmetric top/bottom pixels, opens and closes the native menu,
  recreates the GL context, and scripts pointer query orders.
- [x] Make RetroArch's declared GL backend match the renderer's actual minimum
  GL/GLSL capabilities; reject unsupported contexts before mutation and prove
  shader/program allocation is released on every initialization failure.
- [x] Make final renderer composition atomic: a failure after any intermediate
  draw must leave the emulator backbuffer's original game frame intact.
- [x] Keep one immutable pointer tuple for a complete libretro poll epoch under
  threaded video, so X/Y/PRESSED cannot mix mapped, cleared, or raw states.
- [x] Close the context-retirement publication race: a map validated for retired
  generation A must never be committed or sampled after A is invalidated.
- [x] Decouple renderer evidence/readback status from gameplay touch-map
  publication; evidence I/O failure must not disable otherwise valid input.
- [x] Move dual-surface width and live-geometry validation policy into the
  declarative RetroArch rendering contract and generated launch environment.
- [x] Run `nix flake check path:. --no-build --all-systems` against the current
  tree; every declared app, package, shell, architecture contract, and check
  evaluates.
- [x] Make the outer AppImage sandbox advertise its exact read-only `/nix/store`
  bind to the inner emulator sandbox; the generated RetroArch launch no longer
  fails its own `SEMU_NIX_STORE_MOUNTED=1` admission check.
- [x] Compile RetroArch's exact packaged Steam Virtual Gamepad autoconfig root
  into `retroarch.cfg`, retain both autoconfig-toast suppressions, and reject a
  generated GB profile that omits either behavior.
- [x] Give the shared `ui.menu` chord one owner for RetroArch: the Semu renderer
  menu. The generated native RetroArch profile must not also bind that chord to
  `input_menu_toggle`.
- [x] Compile each system's declared evidence capture frame into the renderer
  launch environment and make the Deck verifier consume the renderer's exact
  schema-3 protocol with scenario-appropriate capture requirements.
- [x] Restrict settings choices and override validation to the selected
  system's declared shader, bezel, layout, and aspect variants; a globally
  known asset from another system must not be offered or accepted.
- [x] Derive gameplay and save/load evidence from independently observed frame
  changes and exact state bytes. No command acknowledgement or generated
  configuration may synthesize `semu-emulator-evidence.log` success.
- [x] Require run-bound radial screenshots to prove the actual Steam radial and
  semantic icons; an ordinary gameplay frame must not satisfy radial evidence.
- [x] Scope Nix's documented `--no-check-sigs` exception to copying the locally
  built closure into Semu's private chroot export store; retain recursive store
  verification, independent closure hashing, and the prohibition elsewhere.
- [x] Reject owner-, group-, or world-writable files and directories when an
  installed release is admitted as immutable; retain an adversarial mode test.
- [x] Bind the Linux worker to the pinned image's observed `linux/amd64`
  architecture and `x86_64-linux` package set; reject host-VM architecture or
  receipt mismatches before any build phase.
- [ ] Give only the private Nix store import/build phases the minimum capability
  and writable storage they require. The independent trust review found that
  `exportRuntime` still restores `--privileged`; replace it with the tested
  bounded capability set and prove a real derivation plus Nix copy while final
  AppImage execution and verification remain read-only and unprivileged.
- [x] Remove excluded build output from every frozen source snapshot, assert
  forbidden roots are absent, and use one source-inventory definition for
  refresh, identity, and the Git-backed Nix archive.
- [x] Transactionally retain the worker receipt and independent release proof in
  an artifact-digest-addressed bundle instead of deleting them after publishing
  only the AppImage.
- [x] Extend no-FUSE verification through a fresh temporary install and bounded
  stable-launcher probe, not structural extraction checks alone.
- [ ] Run every focused package check from a clean generated-output root.
- [ ] Produce and independently verify a fresh
  `build/Semu-retroarch-x86_64.AppImage`; no deployable current artifact exists.
- [x] Remove reusable named Nix volumes from both runtime and AppImage-worker
  authority. Each build must use a fresh anonymous store owned only through an
  immutable full container ID, and cleanup must never select containers or
  volumes by reusable name.
- [ ] Replace the runtime builder's self-issued `--no-trust` store proof with an
  independently bound Nix release derivation/closure and direct host hashing;
  a worker-authored receipt may be diagnostic but cannot establish trust.
- [ ] Bind the exported `bin`, `lib`, `share`, and selected Nix closure bytes
  directly to an independently re-evaluated Nix release proof; reject a forged
  runtime plus otherwise valid target closure.
- [ ] Evaluate both independent builds only from the exact archived source store
  path. Reject a source-swap attempt in which mutable `/src` changes after the
  archive and the old source is retained merely as an unrelated derivation input.
- [ ] Hold descriptor, inode, mode, and digest authority for the release proof,
  provenance, and release marker through publication and pruning; reject each
  file's replacement before, during, and after the rename boundary.
- [ ] Bind one immutable Podman executable for inspect/create/start/remove, make
  create stdout and the descriptor-held cidfile agree on one full container ID,
  and prove malformed or replaced CID publication cannot leak the phase, owner,
  or anonymous Nix store.
- [x] Lease the exact runtime verifier and source-derived `mksquashfs` by
  descriptor, inode, mode, and digest; execute or transfer only those held
  descriptors.
- [x] Key AppImage assembly locking to the canonical output path and retain
  parent/name/inode authority through publication and verification.
- [x] Roll back runtime publication if post-publication pruning fails, with an
  adversarial contract proving the prior publication remains active.
- [x] Keep generated AppImage packaging descriptor-authoritative through final
  staging; no pathname-based copy may reopen mutable intermediate inputs.
- [ ] Make every required AppImage source file visible to Git-backed Nix
  evaluation before rerunning compiler and package gates.

## Automated Contracts Present

- [x] A forced `make -B btrc-build` compiles the production entrypoint with
  strict imports, no cache, and no implicit stdlib; aggregate `make test` passes.
- [x] The ownership tree has no maintained `src/semu/**` or `src/runtime/**` and
  passes the tree audit plus 20 rejection fixtures.
- [x] Focused strict-import compiler tests pass for inventory, parse, target
  inheritance, five-layer settings precedence, static checking, deterministic
  plans, emulator profiles, launch plans, and ES-DE output.
- [x] The 17 system definitions resolve exact committed bezel metadata/hashes,
  shader recipe hashes, semantic surface geometry, integer-scaling policy, and
  1280x800 plus 1920x1080 aperture-fit contracts.
- [x] Steam Input generation passes its VDF, semantic icon, destination, and
  bounded install contracts.
- [x] The typed settings store/UI protocol and one native ES-DE Semu Settings
  manifest/runner contract pass.
- [x] The local Deck harness compiles strictly, resolves 17 fixture systems,
  writes an acceptance plan, and prints a mutation-free command dry run.
- [x] The installer fixture proves one-time extraction, stable no-FUSE launch,
  upgrade, rollback, corruption rejection, two-release retention, and
  transactional retired-layout cleanup.

## Compiler And Repository Cleanup

- [x] Keep production source under `src/main.btrc`, `src/cli.btrc`,
  `src/compiler/`, `src/generators/`, and `src/lib/`.
- [x] Keep declarative product inputs under `config/targets`, `config/systems`,
  `config/emulators`, `config/input`, `config/assets`, and `config/settings`.
- [x] Keep one generic emulator generator over parsed definitions rather than
  one-line emulator classes or emulator-named BTRC branches.
- [x] Keep production and test entrypoints separate; tests are not imported by
  `src/main.btrc`.
- [ ] Make every maintained BTRC production, package, and test entrypoint compile
  with strict per-file imports, no cache, and no implicit stdlib.
- [ ] Remove every remaining hard-coded duplicate of launch, input, rendering,
  path, system, and package facts already owned by JSON or Nix.
- [ ] Remove all dead files, generated-source fallbacks, stale shell adapters,
  compatibility aliases, and obsolete local compiler paths.
- [ ] Reduce the root Makefile to thin compiler/Nix/test calls and keep shell
  orchestration bounded; move durable policy into BTRC, JSON, or Nix.
- [ ] Enforce canonical pretty formatting for every maintained and generated JSON
  file with a focused test.
- [x] Audit `README.md` and `docs/**` against the 2026-07-19 tree without claiming
  package, AppImage, or physical success.

## Packages And AppImage

- [x] Fresh-build the shared renderer and execute its ABI/export/install checks.
- [x] Fresh-build source-patched RetroArch plus the exact selected libretro core
  closure; reject host, Flatpak, and prebuilt runtime fallbacks.
- [ ] Fresh-build Dolphin, PPSSPP, Flycast, Azahar, melonDS, PCSX2, Cemu, and
  Ryujinx for x86_64 Linux with their package/linkage/source-hook checks.
- [ ] Fresh-build and execute the declared macOS ares N64 path.
- [ ] Bind every emulator and core to exact source/package provenance and publish
  reproducible package/AppImage identities.
- [ ] Assemble one self-contained AppImage containing ES-DE, Semu, all selected
  emulator/core slices, renderer, assets, input runtime, Syncthing, and runtime
  closure.
- [ ] Verify exact AppImage bytes, provenance, no-FUSE extraction, embedded
  closure completeness, and two consecutive launches from the stable launcher.
- [ ] Deploy only the verified digest to the Deck, migrate/remove obsolete Semu
  releases transactionally, and retain current/previous/rollback evidence.
- [ ] Prove default ROM discovery works with
  `/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs` and remains settings-controlled.

## Rendering

- [x] Every emulator definition declares the same direct-linked game and post-UI
  symbols; RetroArch is migrated to ABI 3 while the remaining slices retain ABI
  2 until their own migration. Source patches exist for RetroArch, Dolphin,
  PPSSPP, Flycast, Azahar, melonDS, PCSX2, Cemu, Ryujinx, and ares.
- [x] Definitions select renderer-owned Slang shaders/bezels for 15 era systems
  and modern clean output for Wii U and Switch.
- [ ] Execute every emulator package's source assertions proving patch placement,
  linkage, hook order, and rejection of `LD_PRELOAD`, GLX/EGL swap interception,
  gamescope effects, display capture, and runtime symbol discovery.
- [ ] Prove every selected multi-pass shader loads all referenced textures,
  parameters, and temporal resources through librashader on the target GPU.
- [ ] Prove independent shader history and correct touch/game rectangles for DS
  and 3DS top/bottom surfaces.
- [ ] Prove 4:3/16:9 switching uses live emulator-reported content state for N64,
  PSX, Dreamcast, GameCube, Wii, and PS2.
- [ ] Prove opaque bezel art cannot cover game pixels and every aperture remains
  correctly framed at 1280x800 and at least one external-display aspect ratio.
- [ ] Inspect real-game screenshots for era accuracy, photorealistic bezel
  quality, correct z-order, nonblank pixels, no clipping, and upright orientation.
- [ ] Expose per-system shader, bezel, layout, and emulator override choices in
  Semu-owned settings and compile them into generated runtime state.

## Input

- [x] Fresh-build and execute the Linux input supervisor contract for both
  Start-then-Select and Select-then-Start process-group termination.
- [ ] Prove the target-declared Steam Input virtual gamepad gameplay path for
  every configured system and
  eliminate the RetroArch Xbox/autoconfig notification.
- [ ] Prove the left-trackpad quick/menu radials in real Game Mode with semantic
  quit, save, load, menu, and system-specific icons rather than Ctrl labels.
- [ ] Prove Start+Select immediately terminates each emulator process group and
  returns ES-DE to the foreground.
- [ ] Prove save/load performs an actual state change and restoration wherever
  the selected emulator supports state operations.
- [ ] Prove Wii Wiimote, Wiimote+Nunchuk, Classic Controller, and GameCube modes,
  including quick radial switching during a real game.

## Settings, ES-DE, And Sync

- [x] The dependency-free BTRC settings store supports typed get/put and a
  terminal/UI JSON protocol while writing only `$SEMU_HOME/semu.json`.
- [x] The compiler reads sorted, higher-precedence user override JSON without
  allowing `settings put` to modify those files.
- [x] ES-DE source and compiler contracts define exactly one native Semu Settings
  start-menu entry over a regular direct runner, with no fake system or FUSE text.
- [ ] Fresh-build the patched ES-DE package and prove a visible nonblack splash,
  bounded startup, one functional Settings entry, and no FUSE dependency on Deck.
- [ ] Extend the BTRC settings UI to cover per-system rendering/layout choices,
  input behavior, Wii mode, synchronization folders/devices/status, and a route
  to Syncthing's loopback UI without external UI dependencies.
- [ ] Compile all settings exclusively into Semu-owned generated files, then into
  emulator-native outputs; prove no emulator-owned file is edited in place.
- [ ] Compile enabled synchronization folders into generated Syncthing config and
  grant write access only to those exact folders.
- [ ] Prove the isolated Syncthing user service, boot persistence, loopback UI,
  status, and disable/re-enable behavior on the Deck.

## Physical Deck Matrix

- [ ] Preflight Game Mode, SSH, readable controller events, no-sudo keep-awake
  inhibitor, minimum safe brightness, charging/power stability, and writable
  Semu-owned state before launching the matrix.
- [ ] Prove the ES-DE frontend row in `acceptance-matrix.md`.
- [ ] Physically accept Game Boy.
- [ ] Physically accept Game Boy Color.
- [ ] Physically accept Game Boy Advance.
- [ ] Physically accept NES.
- [ ] Physically accept SNES.
- [ ] Physically accept Genesis.
- [ ] Physically accept Nintendo 64, including the non-inverted asymmetric frame.
- [ ] Physically accept PlayStation with isolated BIOS hash evidence.
- [ ] Physically accept Nintendo DS through melonDS and the declared RetroArch
  fallback.
- [ ] Physically accept GameCube.
- [ ] Physically accept Wii with all four controller modes.
- [ ] Physically accept Dreamcast.
- [ ] Physically accept PlayStation 2.
- [ ] Physically accept PSP.
- [ ] Prove the selected Deck 3DS ROM is decrypted and matches the declared Mac
  source; if not, perform only a bounded non-overwriting hash-verified copy.
- [ ] Physically accept Nintendo 3DS.
- [ ] Physically accept Wii U.
- [ ] Physically accept Switch.
- [ ] Retain unchanged before/after hashes for external ROM, BIOS, key, firmware,
  and existing save trees across the complete matrix.
- [ ] Run Bazzite parity only after every physical Steam Deck item above passes.
