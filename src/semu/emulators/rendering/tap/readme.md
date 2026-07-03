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
| `tap_geometry_check.c` / `tap_menu_check.c` | host-runnable contract checks |
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
`stdenvNoCC` interpreter for the `sources.json` asset manifest; the tap is
compiled code that needs a real toolchain.

## macOS plan (future work, not built)

On darwin the tap is not built. The equivalent interposition story is:

- `DYLD_INSERT_LIBRARIES` instead of `LD_PRELOAD`, with a
  `__DATA,__interpose` section (or `dlsym(RTLD_NEXT, ...)` wrappers) to hook
  the present call — `CGLFlushDrawable` for OpenGL emulators, or a
  `MTLCommandBuffer presentDrawable:` swizzle for Metal ones.
- SIP constraints: dyld ignores `DYLD_INSERT_LIBRARIES` for platform binaries
  and for hardened-runtime binaries lacking the
  `com.apple.security.cs.allow-dyld-environment-variables` entitlement. Since
  Semu builds its emulators from source, they can be signed ad-hoc without the
  hardened runtime (or with that entitlement), which keeps insertion legal
  without touching SIP itself.
- The reporting side (`semu_tap.h`) is already OS-agnostic: `dlsym(RTLD_DEFAULT,
  "semu_tap_report")` works unchanged under dyld.

Vulkan on the Deck (a `vkQueuePresentKHR` layer) remains a compositor back end
with the same tap input, not a per-emulator renderer.

Runtime proof must show that game frames are affected and emulator UI is not.
