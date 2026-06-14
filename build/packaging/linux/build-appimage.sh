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
REPO_ROOT="$(cd "$HERE/../../.." && pwd)"
[ -z "$ESDE_APPIMAGE" ] && ESDE_APPIMAGE="$REPO_ROOT/ES-DE.AppImage"
[ -z "$OUTPUT" ] && OUTPUT="$REPO_ROOT/Semu-${ARCH}.AppImage"

if [ ! -f "$ESDE_APPIMAGE" ]; then
  echo "ES-DE AppImage not found at $ESDE_APPIMAGE" >&2
  echo "Pass --esde-appimage <path> or place one at the repository root." >&2
  exit 2
fi

APPIMAGETOOL="${APPIMAGETOOL:-$(command -v appimagetool || true)}"
[ -z "$APPIMAGETOOL" ] && APPIMAGETOOL="$REPO_ROOT/bin/appimagetool"
[ -x "$APPIMAGETOOL" ] || { echo "appimagetool not found (try APPIMAGETOOL=...)" >&2; exit 3; }

if [ -z "${TMPDIR:-}" ] && [ -n "${HOME:-}" ] && [ -d "$HOME/.cache" ]; then
  export TMPDIR="$HOME/.cache/semu-appimage-work"
  mkdir -p "$TMPDIR"
fi

WORK="$(mktemp -d -t semu-appimage.XXXXXX)"
cleanup() {
  chmod -R u+w "$WORK" 2>/dev/null || true
  rm -rf "$WORK" 2>/dev/null || {
    if command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
      sudo rm -rf "$WORK"
    else
      echo "warning: could not remove AppImage workdir: $WORK" >&2
    fi
  }
}
trap cleanup EXIT
APPDIR="$WORK/Semu.AppDir"
mkdir -p "$APPDIR"

SEMU_NIX_BINS=(
  semu
  semu-quit-watch
  bwrap
  semu-retroarch
  semu-dolphin
  semu-ppsspp
  semu-flycast
  semu-melonds
  semu-pcsx2
  semu-cemu
  semu-azahar
  semu-ryujinx
  semu-es-de
  es-de
  semu-render
  gamescope
  syncthing
  syncthingtray
  curl
  nixGL
)

SEMU_SHIM_BINS=(
  semu-btrc
  semu-flatpak
  semu-settings
  semu-retroarch
  semu-dolphin
  semu-ppsspp
  semu-flycast
  semu-melonds
  semu-pcsx2
  semu-cemu
  semu-azahar
  semu-ryujinx
  semu-render
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

if [ -z "$NIX_PACKAGE" ] && [ -e "$REPO_ROOT/result" ]; then
  NIX_PACKAGE="$REPO_ROOT/result"
fi

if [ -n "$NIX_PACKAGE" ]; then
  [ -e "$NIX_PACKAGE" ] || { echo "Nix package not found: $NIX_PACKAGE" >&2; exit 4; }
  NIX_PACKAGE="$(readlink -f "$NIX_PACKAGE")"
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
      chmod u+w "$APPDIR/usr/bin/$bin" 2>/dev/null || true
      rm -f "$APPDIR/usr/bin/$bin"
      cp "$NIX_PACKAGE/bin/$bin" "$APPDIR/usr/bin/$bin"
      chmod +x "$APPDIR/usr/bin/$bin"
    fi
  done
  # The Nix package's bin/semu is a makeWrapper script that sets SEMU_BIN back
  # to its /nix/store output. Inside the AppImage, generated desktop entries
  # and systemd units must point at the stable AppImage executable instead, so
  # ship the raw BTRC CLI as usr/bin/semu and let AppRun provide the env.
  if [ -x "$NIX_PACKAGE/lib/semu/semu-btrc" ]; then
    chmod u+w "$APPDIR/usr/bin/semu" 2>/dev/null || true
    rm -f "$APPDIR/usr/bin/semu"
    cp "$NIX_PACKAGE/lib/semu/semu-btrc" "$APPDIR/usr/bin/semu"
    chmod +x "$APPDIR/usr/bin/semu"
  fi
  if [ ! -x "$APPDIR/usr/bin/bwrap" ]; then
    echo "Nix package did not provide bwrap; AppImage sandbox mounting would fail" >&2
    exit 4
  fi
  if [ -d "$NIX_PACKAGE/lib/retroarch/cores" ]; then
    echo "Copying RetroArch cores into AppDir..."
    mkdir -p "$APPDIR/usr/lib/retroarch"
    chmod -R u+w "$APPDIR/usr/lib/retroarch/cores" 2>/dev/null || true
    rm -rf "$APPDIR/usr/lib/retroarch/cores"
    cp -aL "$NIX_PACKAGE/lib/retroarch/cores" "$APPDIR/usr/lib/retroarch/cores"
  fi
  if [ -d "$NIX_PACKAGE/share/libretro/shaders/shaders_slang" ]; then
    echo "Copying Semu reference shader assets into AppDir..."
    mkdir -p "$APPDIR/usr/share/libretro/shaders"
    chmod -R u+w "$APPDIR/usr/share/libretro/shaders/shaders_slang" 2>/dev/null || true
    rm -rf "$APPDIR/usr/share/libretro/shaders/shaders_slang"
    cp -aL "$NIX_PACKAGE/share/libretro/shaders/shaders_slang" "$APPDIR/usr/share/libretro/shaders/shaders_slang"
  fi
  if [ -d "$NIX_PACKAGE/share/libretro/shaders/Mega_Bezel_Packs" ]; then
    echo "Copying Semu reference bezel assets into AppDir..."
    mkdir -p "$APPDIR/usr/share/libretro/shaders"
    chmod -R u+w "$APPDIR/usr/share/libretro/shaders/Mega_Bezel_Packs" 2>/dev/null || true
    rm -rf "$APPDIR/usr/share/libretro/shaders/Mega_Bezel_Packs"
    cp -aL "$NIX_PACKAGE/share/libretro/shaders/Mega_Bezel_Packs" "$APPDIR/usr/share/libretro/shaders/Mega_Bezel_Packs"
  fi
fi

# Semu packaging/linux tree.
mkdir -p "$APPDIR/packaging/linux"
cp -r "$HERE/." "$APPDIR/packaging/linux/"
# Don't ship the build script itself inside.
rm -f "$APPDIR/packaging/linux/build-appimage.sh"

if [ -d "$REPO_ROOT/config" ]; then
  cp -r "$REPO_ROOT/config" "$APPDIR/config"
fi
if [ -d "$REPO_ROOT/assets" ]; then
  cp -r "$REPO_ROOT/assets" "$APPDIR/assets"
fi

for bin in "${SEMU_SHIM_BINS[@]}"; do
  if [ ! -x "$APPDIR/usr/bin/$bin" ] && [ -x "$HERE/bin/$bin" ]; then
    cp "$HERE/bin/$bin" "$APPDIR/usr/bin/$bin"
    chmod +x "$APPDIR/usr/bin/$bin"
  fi
done

# BTRC CLI. Source of truth is src/main.btrc; this is the compiled entry used
# by AppRun for deck/sync/config commands.
if [ ! -x "$APPDIR/usr/bin/semu" ] && [ -x "$REPO_ROOT/build/out/semu" ]; then
  cp "$REPO_ROOT/build/out/semu" "$APPDIR/usr/bin/semu"
  chmod +x "$APPDIR/usr/bin/semu"
elif [ ! -x "$APPDIR/usr/bin/semu" ]; then
  echo "compiled BTRC CLI not found; run 'make btrc-build' or pass --nix-package" >&2
  exit 5
fi

# AppRun, .desktop, icon at the AppDir root (appimagetool requirements).
cp "$HERE/AppRun" "$APPDIR/AppRun"
# AppRun is the one script the AppImage runtime execs before the bundled Nix
# store is mounted. Nix may patch the source copy's shebang when this builder is
# run from a package output, so force the root entrypoint back to a host-portable
# interpreter before packing.
APPRUN_REWRITE="$APPDIR/AppRun.portable"
{
  printf '%s\n' '#!/usr/bin/env bash'
  tail -n +2 "$APPDIR/AppRun"
} > "$APPRUN_REWRITE"
mv "$APPRUN_REWRITE" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"
cp "$HERE/semu.desktop" "$APPDIR/semu.desktop"
# Icon: borrow ES-DE's icon (a controller silhouette) as a stand-in.
ESDE_ICON="$(find "$SQ" -maxdepth 3 -type f \( -name '*.png' -o -name '*.svg' \) | head -1)"
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
chmod u+w "$OUTPUT" 2>/dev/null || true
rm -f "$OUTPUT"
ARCH="$ARCH_FOR_TOOL" "$APPIMAGETOOL" --no-appstream "$APPDIR" "$OUTPUT"
echo "Built: $OUTPUT"
ls -la "$OUTPUT"
