# GL tap — offline Steam-Deck stand-in proof

The Deck is the only real target for `libsemutap.so` (LD_PRELOAD GL interposition,
x86_64-linux). This proof reproduces the tap's runtime behaviour **without a Deck and
without a GPU**, inside a container using Xvfb + Mesa `llvmpipe` software GL. It is the
regression that answers "does the GL tap actually composite the bezel + CRT shader over
a GL-presenting standalone emulator?".

## What it does

`glx_emu.c` is a headless "fake emulator": it opens a GLX window, renders a bright game
frame into a reported content rect, fills a `SemuTapState` (`semu_tap.h`) exactly as a
real tap-patched emulator does, and calls `glXSwapBuffers` — which `libsemutap.so`
(LD_PRELOADed) intercepts to integer-scale the frame, map the bezel screen-hole, apply
its embedded CRT "tube" shader, and present. It reads the presented X window back
(`XGetImage`) and asserts:

- **positive** (bezel art + shader env): the outer frame band is ~100% bezel art;
- **negative** (no art): the outer frame is not bezel — proving the effect is the bezel,
  not an artifact of the harness.

This exercises the real GLX present path the Deck's standalone emulators use once they
are OpenGL-forced (pcsx2 `Renderer=12`, dolphin, cemu `GraphicAPI=0`, ryujinx GL).

## Run

    cd src/semu/emulators/rendering/tap/gl_tap_proof
    podman build -t semu-tap-proof -f Containerfile .
    mkdir -p /tmp/w && cp glx_emu.c /tmp/w/
    podman run --rm -v /tmp/w:/work:Z -v "$(git rev-parse --show-toplevel)":/repo:ro \
      semu-tap-proof bash /repo/src/semu/emulators/rendering/tap/gl_tap_proof/run.sh

Prints `GL-TAP PROOF: PASS` and writes `out_bezel.ppm` (the composited TV bezel with the
game in the tube) + `out_bare.ppm` to the work dir. No GPU, no Deck, no X server on the
host required — everything runs in the container.
