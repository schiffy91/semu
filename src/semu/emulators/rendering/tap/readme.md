# Semu GL tap

The tap is a boundary, not a renderer.

## Contract

Each emulator (built from source) reports one small frame description at its
present point — "tap-in" — by filling a `SemuTapState` (`semu_tap.h`) and calling
`semu_tap_report_safe`. The Semu compositor (`libsemutap.c`, loaded into the
emulator process) reads only that report at the swap boundary — "tap-out":

- `active == 0`: present untouched (menus, no content).
- `active == 1`: integer-scale the game from its reported content rect, map the
  bezel art's screen hole onto it, apply the shader policy (`tube.frag` is the
  source of truth for the embedded fragment shader), draw the radial menu when
  open, and present.

No pixel inspection, no black-detection, no centering guesses — geometry comes
exclusively from the report plus the `SEMU_TAP_*` environment.

## Environment contract

The `SEMU_TAP_*` variables (native size, aspect, style, priority, bezel art,
screen hole, shell tint, ...) are produced by `RenderPlanner.tapEnvironment` in
`src/semu/emulators/rendering/semu_rendering.btrc` and consumed by
`libsemutap.c`. The btrc side is the single source of truth for key names and
wire formats.

## Files

| File | Role |
| --- | --- |
| `semu_tap.h` | the ABI: emulator-side reporting contract (tap-in) |
| `libsemutap.c` | the OpenGL compositor: glXSwapBuffers/EGL hook (tap-out) |
| `tap_geometry.h` | pure geometry contract (integer scale, hole mapping) |
| `tap_menu.h` | radial menu contract: states, navigation, font-atlas text |
| `tap_geometry_check.c` / `tap_menu_check.c` | host-runnable contract checks (`build.sh`) |
| `macos_overlay.m` | the macOS compositor: overlay window above the emulator (tap-out, injection-free) |
| `build.sh` | host build path: run the contract checks, compile `semu-overlay` on darwin |
| `tube.frag` | readable source of the fragment shader embedded in `libsemutap.c` |
| `mbparse.c` | offline Mega Bezel `.slangp` resolver (learn art + geometry params) |
| `gen_bezel_manifest.sh` | offline: auto-measure bezel screen holes into a manifest |
| `gen_handheld_shells.sh` | offline: build handheld device shells + measure holes |
| `stb_image.h` | vendored PNG loader (stb, public domain / MIT) |

The emulator-side report patch for RetroArch lives with its contract:
`src/semu/emulators/retroarch/retroarch.patch` includes `semu_tap.h` and reports
at `video_driver_frame()`.

## Build

`libsemutap.so` is built by the `semu-tap` package stanza in
`src/semu/packaging/nix/flake/packages.nix`, on x86_64-linux only (the Steam
Deck target):

    cc -shared -fPIC -O2 -o libsemutap.so libsemutap.c -ldl -lm

It went there rather than into `semu_shaders.nix` because that derivation is a
`stdenvNoCC` interpreter for the `shaders.json` asset manifest; the tap is
compiled code that needs a real toolchain.

## macOS: the overlay window (built, no injection)

On darwin the emulators present via Metal, so the GL tap does not apply and
nothing is injected. The macOS tap is `macos_overlay.m` (`build.sh` compiles
it to `src/generated/build/macos/tap/semu-overlay`): a standalone process
holding a transparent, click-through, non-activating window one level above
the emulator's, drawing the bezel art around the shared `tap_geometry.h`
content rect with the screen hole cleared back to transparency. It reads the
same `SEMU_TAP_*` environment plus `SEMU_TAP_TARGET_PID` (the emulator pid to
shadow, minted by the launch wrapper in
`src/semu/platforms/macos/macos_tap.btrc`), tracks the pid's largest window
via `CGWindowListCopyWindowInfo`, and exits when the pid dies.

The dyld interposition route (`DYLD_INSERT_LIBRARIES` + ad-hoc signing to
stay legal under SIP; `semu_tap.h` is already OS-agnostic) remains on the
table if per-frame compositing is ever needed on macOS — the overlay draws
around the content, it never touches the frames themselves.

Vulkan on the Deck (a `vkQueuePresentKHR` layer) remains a compositor back end
with the same tap input, not a per-emulator renderer.

Runtime proof must show that game frames are affected and emulator UI is not.
