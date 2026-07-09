#!/usr/bin/env bash
# build_btrc.sh — transpile + build the btrc port of the tap.
#
# NOTE: this is the btrc port's build. The SHIPPED artifact is still libsemutap.c
# (the C tap) until this port is Deck-verified at runtime; packages.nix is not
# changed. This script exists to reproduce the transpile+compile+symbol checks.
#
# Requires: a btrc checkout (BTRC_SRC, default /tmp/btrc-src) with `python3 -m
# src.compiler.python.main` runnable, and zig for the Linux cross-build (both are
# in the btrc nix devshell: `nix develop` in the btrc checkout, then run this).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BTRC_SRC="${BTRC_SRC:-/tmp/btrc-src}"
OUT="${OUT:-/tmp/semu-tap-btrc}"
mkdir -p "$OUT"

echo "== transpile (btrc -> C), --no-dce so library exports survive DCE =="
( cd "$BTRC_SRC" && python3 -m src.compiler.python.main \
    "$HERE/libsemutap.btrc" -o "$OUT/libsemutap.c" --no-dce --no-stdlib )

echo "== compile stb_image implementation object =="
cc -c -O2 "$HERE/stb_impl.c" -o "$OUT/stb_impl.o"

# The btrc port is now fully standalone: semutap_glue.h is gone, so the -I"$HERE"
# below only serves stb_impl.c (stb_image.h). stat_mtime_d uses the Linux st_mtim
# member (the .so is Linux-only); on macOS the syntax-gate build maps it to
# st_mtimespec so the verification dylib still compiles (the shipped macOS tap is
# libsemutap.c, which keeps the platform #ifdef).
echo "== macOS shared lib (verification build; undefined dynamic_lookup for the"
echo "   runtime-resolved GL/dlvsym symbols; restrict exports to the tap contract) =="
cat > "$OUT/exports.txt" <<'EOF'
_semu_tap_report
_dlsym
_glXSwapBuffers
_eglSwapBuffers
EOF
MACOS_COMPAT=""
if [ "$(uname)" = "Darwin" ]; then MACOS_COMPAT="-Dst_mtim=st_mtimespec"; fi
cc -shared -fPIC -undefined dynamic_lookup -O2 $MACOS_COMPAT -I"$HERE" \
   -Wl,-exported_symbols_list,"$OUT/exports.txt" \
   "$OUT/libsemutap.c" "$OUT/stb_impl.o" -o "$OUT/libsemutap.dylib" || true

echo "== Linux .so (the real target). Run inside the btrc nix devshell (has zig). =="
if command -v zig >/dev/null 2>&1; then
  cat > "$OUT/exports.map" <<'EOF'
{ global: semu_tap_report; dlsym; glXSwapBuffers; eglSwapBuffers; local: *; };
EOF
  zig cc -target x86_64-linux-gnu -c -O2 "$HERE/stb_impl.c" -o "$OUT/stb_impl_linux.o"
  zig cc -target x86_64-linux-gnu -shared -fPIC -O2 -I"$HERE" \
      "$OUT/libsemutap.c" "$OUT/stb_impl_linux.o" \
      -Wl,--version-script="$OUT/exports.map" \
      -o "$OUT/libsemutap.so"
  echo "built $OUT/libsemutap.so"
  echo "== exported-symbol parity check (must be exactly the 4 tap-contract syms) =="
  nm -D --defined-only "$OUT/libsemutap.so" | grep -viE 'stbi|GLIBC|^_' || true
else
  echo "zig not on PATH; skipping Linux cross-build (run under 'nix develop')."
fi

echo "== done. Artifacts in $OUT =="
