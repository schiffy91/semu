#!/usr/bin/env bash
# Offline Steam-Deck stand-in proof for the Linux GL tap (libsemutap.so).
# Builds the tap + a headless GLX "fake emulator" and asserts the tap composites
# the bezel + CRT tube shader around a GL-presented frame — and does NOT when the
# bezel art is absent. Runs under Xvfb + Mesa llvmpipe (no GPU needed).
#
#   podman build -t semu-tap-proof -f Containerfile .
#   podman run --rm -v "$PWD":/work:Z -v "$REPO":/repo:ro semu-tap-proof bash /work/run.sh
set -e
cd /work
TAP=/repo/src/semu/emulators/rendering/tap
cc -shared -fPIC -O2 -I"$TAP" -o libsemutap.so "$TAP/libsemutap.c" -ldl -lm
cc -O2 -I"$TAP" -o glx_emu glx_emu.c -lGL -lX11 -ldl -lm
ART=${SEMU_PROOF_ART:-/repo/src/semu/assets/bezels/ps2/tv.png}
base="SEMU_TAP_STATE_DIR=/work SEMU_TAP_PRIORITY=b SEMU_TAP_SCREEN=0.125,0.11,0.75,0.78 SEMU_TAP_NATIVE_W=640 SEMU_TAP_NATIVE_H=448 EMU_FRAMES=12 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe"
echo "== POSITIVE: bezel art + CRT shader =="
xvfb-run -a -s "-screen 0 1280x800x24" env LD_PRELOAD=/work/libsemutap.so $base SEMU_TAP_MASK=0.5 SEMU_TAP_SCAN=0.4 SEMU_TAP_CURVE=0.03 SEMU_TAP_ART="$ART" ./glx_emu /work/out_bezel.ppm 2>&1 | grep -E "OUTER|PASS|FAIL"; POS=${PIPESTATUS[0]}
echo "== NEGATIVE: bare (no art, no shader) =="
xvfb-run -a -s "-screen 0 1280x800x24" env LD_PRELOAD=/work/libsemutap.so $base ./glx_emu /work/out_bare.ppm 2>&1 | grep -E "OUTER|PASS|FAIL"; NEG=${PIPESTATUS[0]}
echo "VERDICT: positive=$POS(want 0) negative=$NEG(want 1)"
[ "$POS" = 0 ] && [ "$NEG" = 1 ] && { echo "GL-TAP PROOF: PASS"; exit 0; } || { echo "GL-TAP PROOF: FAIL"; exit 1; }
