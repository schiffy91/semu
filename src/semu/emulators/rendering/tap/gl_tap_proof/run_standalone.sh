#!/usr/bin/env bash
# Standalone-mode proof: the GL tap composites its bezel over a REAL,
# uninstrumented GL program (glxgears from mesa-utils) — no SemuTapState report,
# so the tap synthesizes the content rect from the live framebuffer, exactly as
# it does for the flatpak standalones (Cemu/Dolphin/PCSX2, SEMU_TAP_STANDALONE=1).
# Xvfb + Mesa llvmpipe; no GPU/Deck needed.
#
#   podman run --rm -v "$PWD":/work:Z -v "$REPO":/repo:ro semu-tap-proof bash /work/run_standalone.sh
set -e
cd /work
TAP=/repo/src/semu/emulators/rendering/tap
cc -shared -fPIC -O2 -I"$TAP" -o libsemutap.so "$TAP/libsemutap.c" -ldl -lm
cc -O2 -o xcap "$TAP/gl_tap_proof/xcap.c" -lX11
command -v glxgears >/dev/null || { echo "need mesa-utils (glxgears)"; exit 2; }
ART=${SEMU_PROOF_ART:-/repo/src/semu/assets/bezels/ps2/tv.png}

export DISPLAY=:99
Xvfb :99 -screen 0 1280x800x24 >/tmp/xvfb.log 2>&1 &
XVFB=$!; sleep 1.5
LD_PRELOAD=/work/libsemutap.so SEMU_TAP_STATE_DIR=/work SEMU_TAP_STANDALONE=1 \
  SEMU_TAP_PRIORITY=b SEMU_TAP_ART="$ART" SEMU_TAP_SCREEN=0.125,0.11,0.75,0.78 \
  SEMU_TAP_NATIVE_W=640 SEMU_TAP_NATIVE_H=448 SEMU_TAP_MASK=0.5 SEMU_TAP_SCAN=0.4 \
  glxgears -geometry 1280x800 >/tmp/gears.log 2>&1 &
GEARS=$!; sleep 3
set +e
./xcap /work/out_standalone.ppm; CAP=$?
set -e
kill "$GEARS" "$XVFB" 2>/dev/null || true

echo "== tap self-report (synthesized state + bezel draw) =="
grep -E 'standalone=1' /work/semu-swaptype.log || { echo "tap did not enter standalone mode"; exit 1; }
grep -qE 'effArt=1\.0' /work/semutap.log || { echo "tap never drew the bezel (effArt stayed 0)"; exit 1; }
[ "$CAP" = 0 ] && echo "STANDALONE GL-TAP PROOF: PASS" || { echo "STANDALONE GL-TAP PROOF: FAIL"; exit 1; }
