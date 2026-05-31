#!/usr/bin/env bash
# Build Semu.AppImage.
#
# Inputs (from --arg, or defaults):
#   --esde-appimage PATH   Path to an existing ES-DE AppImage (we'll extract it)
#   --nix-package PATH     Optional Nix package output whose closure is bundled
#   --output PATH          Where to write the resulting Semu-*.AppImage
#   --arch aarch64|x86_64  Target arch (appimagetool needs to know)
#
# Requires `appimagetool` in PATH or alongside this script.

set -euo pipefail

ARCH="$(uname -m)"
OUTPUT=""
ESDE_APPIMAGE=""
NIX_PACKAGE="${SEMU_NIX_PACKAGE:-}"

while [ $# -gt 0 ]; do
  case "$1" in
    --esde-appimage) ESDE_APPIMAGE="$2"; shift 2 ;;
    --nix-package)   NIX_PACKAGE="$2"; shift 2 ;;
    --output)        OUTPUT="$2"; shift 2 ;;
    --arch)          ARCH="$2"; shift 2 ;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
[ -z "$ESDE_APPIMAGE" ] && ESDE_APPIMAGE="$HERE/../ES-DE.AppImage"
[ -z "$OUTPUT" ] && OUTPUT="$HERE/../Semu-${ARCH}.AppImage"

if [ ! -f "$ESDE_APPIMAGE" ]; then
  echo "ES-DE AppImage not found at $ESDE_APPIMAGE" >&2
  echo "Pass --esde-appimage <path> or place one alongside the semu dir." >&2
  exit 2
fi

APPIMAGETOOL="${APPIMAGETOOL:-$(command -v appimagetool || true)}"
[ -z "$APPIMAGETOOL" ] && APPIMAGETOOL="$HERE/../bin/appimagetool"
[ -x "$APPIMAGETOOL" ] || { echo "appimagetool not found (try APPIMAGETOOL=...)" >&2; exit 3; }

WORK="$(mktemp -d -t semu-appimage.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
APPDIR="$WORK/Semu.AppDir"
mkdir -p "$APPDIR"

SEMU_NIX_BINS=(
  semu
  bwrap
  semu-retroarch
  semu-dolphin
  semu-ppsspp
  semu-flycast
  semu-gopher64
  semu-melonds
  semu-pcsx2
  semu-cemu
  semu-azahar
  semu-ryujinx
  semu-es-de
)

SEMU_SHIM_BINS=(
  semu-btrc
  semu-flatpak
  semu-retroarch
  semu-dolphin
  semu-ppsspp
  semu-flycast
  semu-gopher64
  semu-melonds
  semu-pcsx2
  semu-cemu
  semu-azahar
  semu-ryujinx
)

# Extract ES-DE's squashfs payload into the AppDir.
echo "Extracting ES-DE AppImage..."
( cd "$WORK" && "$ESDE_APPIMAGE" --appimage-extract >/dev/null )
# ES-DE puts everything under squashfs-root/. We pull the bits we need.
SQ="$WORK/squashfs-root"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share" "$APPDIR/usr/lib"
cp -r "$SQ/usr/bin/." "$APPDIR/usr/bin/"
cp -r "$SQ/usr/share/." "$APPDIR/usr/share/" 2>/dev/null || true
cp -r "$SQ/usr/lib/." "$APPDIR/usr/lib/" 2>/dev/null || true

if [ -z "$NIX_PACKAGE" ] && [ -e "$HERE/../result" ]; then
  NIX_PACKAGE="$HERE/../result"
fi

if [ -n "$NIX_PACKAGE" ]; then
  [ -e "$NIX_PACKAGE" ] || { echo "Nix package not found: $NIX_PACKAGE" >&2; exit 4; }
  command -v nix >/dev/null 2>&1 || {
    echo "nix is required when --nix-package is used" >&2
    exit 4
  }

  echo "Copying Nix closure into AppDir..."
  nix copy --no-check-sigs --to "local?root=$APPDIR" "$NIX_PACKAGE"

  # Copy the routed launchers into usr/bin as real files. Their interpreters
  # and referenced emulator binaries live in the bundled /nix/store closure.
	  for bin in "${SEMU_NIX_BINS[@]}"; do
	    if [ -x "$NIX_PACKAGE/bin/$bin" ]; then
	      cp "$NIX_PACKAGE/bin/$bin" "$APPDIR/usr/bin/$bin"
	      chmod +x "$APPDIR/usr/bin/$bin"
	    fi
	  done
	  if [ ! -x "$APPDIR/usr/bin/bwrap" ]; then
	    echo "Nix package did not provide bwrap; AppImage sandbox mounting would fail" >&2
	    exit 4
	  fi
	fi

# Semu linux/ tree.
mkdir -p "$APPDIR/linux"
cp -r "$HERE/." "$APPDIR/linux/"
# Don't ship the build script itself inside.
rm -f "$APPDIR/linux/build-appimage.sh"

for bin in "${SEMU_SHIM_BINS[@]}"; do
  if [ ! -x "$APPDIR/usr/bin/$bin" ] && [ -x "$HERE/bin/$bin" ]; then
    cp "$HERE/bin/$bin" "$APPDIR/usr/bin/$bin"
    chmod +x "$APPDIR/usr/bin/$bin"
  fi
done

# BTRC CLI. Source of truth is semu.btrc; this is the compiled runtime
# entry used by AppRun for deck/sync/config commands.
if [ ! -x "$APPDIR/usr/bin/semu" ] && [ -x "$HERE/../build/semu" ]; then
  cp "$HERE/../build/semu" "$APPDIR/usr/bin/semu"
  chmod +x "$APPDIR/usr/bin/semu"
elif [ ! -x "$APPDIR/usr/bin/semu" ]; then
  echo "compiled BTRC CLI not found; run 'make btrc-build' or pass --nix-package" >&2
  exit 5
fi

# AppRun, .desktop, icon at the AppDir root (appimagetool requirements).
cp "$HERE/AppRun" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"
cp "$HERE/semu.desktop" "$APPDIR/semu.desktop"
# Icon: borrow ES-DE's icon (a controller silhouette) as a stand-in.
ESDE_ICON="$(find "$SQ" -maxdepth 2 \( -name '*.png' -o -name '*.svg' \) | head -1)"
if [ -n "$ESDE_ICON" ]; then
  # Use ES-DE's icon as a stand-in; named to match the .desktop file.
  ext="${ESDE_ICON##*.}"
  cp "$ESDE_ICON" "$APPDIR/semu.$ext"
else
  # Synthesize a tiny solid-color PNG fallback so appimagetool stops complaining.
  printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f\x15\xc4\x89\x00\x00\x00\rIDATx\x9cc\xfc\xff\xff?\x03\x00\x06\xfd\x02\xfe\x00\x00\x00\x00IEND\xaeB`\x82' \
    > "$APPDIR/semu.png"
fi

# Pack.
echo "Packing AppImage..."
ARCH_FOR_TOOL="${ARCH}"
case "$ARCH" in
  aarch64) ARCH_FOR_TOOL=aarch64 ;;
  x86_64)  ARCH_FOR_TOOL=x86_64 ;;
esac
ARCH="$ARCH_FOR_TOOL" "$APPIMAGETOOL" --no-appstream "$APPDIR" "$OUTPUT"
echo "Built: $OUTPUT"
ls -la "$OUTPUT"
