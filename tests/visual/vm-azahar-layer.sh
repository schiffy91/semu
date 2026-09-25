#!/usr/bin/env bash
# Azahar through VK_LAYER_SEMU_compositor in the Linux VM: lavapipe Vulkan and llvmpipe OpenGL on a
# private Xvfb, so no real display is touched. Captures the composed 3DS shell, then holds a click
# at TOUCH_X,TOUCH_Y on the composed picture and captures again; the layer logs where the click
# was mapped in Azahar's own layout (semu-touch.patch). The ROM is mounted read-only.
# usage: vm-azahar-layer.sh REPOSITORY ROM OUT_DIR   (env: WAIT seconds before the capture, TOUCH_X, TOUCH_Y)
set -euo pipefail
repository="$(cd "$1" && pwd)"; rom="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"; out="$(mkdir -p "$3" && cd "$3" && pwd)"
cat > "$out/inside.sh" <<'INSIDE'
set -u
export NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false"
git config --global --add safe.directory '*'
B() { nix build --no-link --print-out-paths "git+file:///src#packages.x86_64-linux.$1" 2>/dev/null | tail -1; }
P() { nix build --no-link "nixpkgs#$1" >/dev/null 2>&1; nix eval --raw "nixpkgs#$1.outPath"; }
R=$(B semu-renderer); AZ=$(B emulator-azahar); CLI=$(B semu-cli); ASSETS=$(B asset-root)
MESA=$(P mesa); XVFB=$(P xvfb); XWD=$(P xwd); IM=$(P imagemagick); XDO=$(P xdotool)
echo "renderer $R azahar $AZ"
$XVFB/bin/Xvfb :97 -screen 0 1280x720x24 >/dev/null 2>&1 & X=$!
sleep 2
mkdir -p /tmp/home/config/azahar-emu /tmp/home/data
printf '[Renderer]\ngraphics_api\\default=false\ngraphics_api=2\nasync_presentation\\default=false\nasync_presentation=false\n[UI]\nfullscreen\\default=false\nfullscreen=true\nconfirmClose\\default=false\nconfirmClose=false\n' > /tmp/home/config/azahar-emu/qt-config.ini
mapfile -t envs < <($CLI/bin/semu render-env --system n3ds --emulator azahar --project /src/config --asset-root $ASSETS | grep '^SEMU_' | grep -v '^SEMU_RENDER_STATE_DIR')
env "${envs[@]}" SEMU_RENDER_STATE_DIR=/out/state SEMU_RENDER_DEBUG=1 DISPLAY=:97 QT_QPA_PLATFORM=xcb XDG_CONFIG_HOME=/tmp/home/config XDG_DATA_HOME=/tmp/home/data \
  VK_DRIVER_FILES=$(ls $MESA/share/vulkan/icd.d/lvp_icd*.json) __EGL_VENDOR_LIBRARY_DIRS=$MESA/share/glvnd/egl_vendor.d \
  VK_ADD_LAYER_PATH=$R/share/vulkan/explicit_layer.d VK_INSTANCE_LAYERS=VK_LAYER_SEMU_compositor \
  timeout 900 $AZ/bin/azahar -f "/rom/$ROM_NAME" > /out/azahar.log 2>&1 & A=$!
sleep ${WAIT:-150}
for w in $(DISPLAY=:97 $XDO/bin/xdotool search --onlyvisible --name '.' 2>/dev/null); do DISPLAY=:97 $XDO/bin/xdotool windowmove $w 0 0 windowsize $w 1280 720 2>/dev/null; done
sleep 30
$XWD/bin/xwd -root -silent -display :97 | $IM/bin/magick xwd:- /out/azahar.png
DISPLAY=:97 $XDO/bin/xdotool mousemove ${TOUCH_X:-1100} ${TOUCH_Y:-400} mousedown 1 sleep 2 mouseup 1
sleep 20
$XWD/bin/xwd -root -silent -display :97 | $IM/bin/magick xwd:- /out/azahar-after-touch.png
kill $A; sleep 2; kill $X
grep -E "touch|swapchain" /out/azahar.log | head -20
INSIDE
podman run --rm --platform linux/amd64 --privileged -e WAIT="${WAIT:-240}" -e TOUCH_X="${TOUCH_X:-1100}" -e TOUCH_Y="${TOUCH_Y:-400}" \
  -e ROM_NAME="$(basename "$rom")" -v semu-nix-x86:/nix -v "$repository":/src:ro -v "$out":/out -v "$(dirname "$rom")":/rom:ro \
  docker.io/nixos/nix:latest bash /out/inside.sh
