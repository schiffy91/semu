#!/usr/bin/env bash
# Azahar through VK_LAYER_SEMU_compositor in the Linux VM: lavapipe Vulkan and llvmpipe OpenGL on a
# private Xvfb, so no real display is touched. Azahar runs with Semu's own compiled qt-config.ini.
# Captures the composed 3DS shell, then holds a click at TOUCH_X,TOUCH_Y on the composed picture and
# captures again; the layer logs where the click was mapped in Azahar's own layout (semu-touch.patch).
# Then Azahar's own X cursor: moved over a static corner of the plate, a capture with the cursor
# (maim) and one without (maim -u) must differ there; 4 s later Azahar has blanked it
# (hideInactiveMouse), so they must not. HIDE_INACTIVE_MOUSE=false overrides Semu's pin to show the
# baseline. The ROM is mounted read-only; the container is left exited.
# usage: vm-azahar-layer.sh REPOSITORY ROM OUT_DIR   (env: WAIT seconds before the capture, TOUCH_X, TOUCH_Y,
#   WIDTH and HEIGHT of the virtual screen, 1280x720 unless set; 1280x800 is the Steam Deck)
set -euo pipefail
repository="$(cd "$1" && pwd)"; rom="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"; out="$(mkdir -p "$3" && cd "$3" && pwd)"
cat > "$out/inside.sh" <<'INSIDE'
set -u
export NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false"
git config --global --add safe.directory '*'
B() { nix build --no-link --print-out-paths "git+file:///src#packages.x86_64-linux.$1" 2>/dev/null | tail -1; }
P() { nix build --no-link --inputs-from /src "nixpkgs#$1" >/dev/null 2>&1; nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }  # the flake's own nixpkgs: a newer Mesa needs a newer glibc than the emulators have
R=$(B semu-renderer); AZ=$(B emulator-azahar); CLI=$(B semu-cli); ASSETS=$(B asset-root)
MESA=$(P mesa); XVFB=$(P xvfb); XWD=$(P xwd); IM=$(P imagemagick); XDO=$(P xdotool); MAIM=$(P maim)
echo "renderer $R azahar $AZ"
$XVFB/bin/Xvfb :97 -screen 0 ${WIDTH}x${HEIGHT}x24 >/dev/null 2>&1 & X=$!
sleep 2
mkdir -p /tmp/home/config/azahar-emu /tmp/home/data
$CLI/bin/semu build configs --target linux-desktop --project /src/config --asset-root $ASSETS --output /tmp/configs > /out/configs.log 2>&1
while IFS= read -r line; do  # Semu's file; HIDE_INACTIVE_MOUSE replaces its pin for a baseline (the image has no sed)
  case "$line" in hideInactiveMouse=*) [ -n "${HIDE_INACTIVE_MOUSE:-}" ] && line="hideInactiveMouse=$HIDE_INACTIVE_MOUSE" ;; esac
  printf '%s\n' "$line"
done < /tmp/configs/profiles/azahar/config/azahar-emu/qt-config.ini > /tmp/home/config/azahar-emu/qt-config.ini
grep -E '^(graphics_api|use_integer_scaling|fullscreen|hideInactiveMouse|confirmClose)=' /tmp/home/config/azahar-emu/qt-config.ini
mapfile -t envs < <($CLI/bin/semu render-env --system n3ds --emulator azahar --project /src/config --asset-root $ASSETS | grep '^SEMU_' | grep -v '^SEMU_RENDER_STATE_DIR')
env "${envs[@]}" SEMU_RENDER_STATE_DIR=/out/state SEMU_RENDER_DEBUG=1 DISPLAY=:97 QT_QPA_PLATFORM=xcb XDG_CONFIG_HOME=/tmp/home/config XDG_DATA_HOME=/tmp/home/data \
  VK_DRIVER_FILES=$(ls $MESA/share/vulkan/icd.d/lvp_icd*.json) __EGL_VENDOR_LIBRARY_DIRS=$MESA/share/glvnd/egl_vendor.d \
  VK_ADD_LAYER_PATH=$R/share/vulkan/explicit_layer.d VK_INSTANCE_LAYERS=VK_LAYER_SEMU_compositor \
  timeout 900 $AZ/bin/azahar -f "/rom/$ROM_NAME" > /out/azahar.log 2>&1 & A=$!
sleep ${WAIT:-150}
for w in $(DISPLAY=:97 $XDO/bin/xdotool search --onlyvisible --name '.' 2>/dev/null); do DISPLAY=:97 $XDO/bin/xdotool windowmove $w 0 0 windowsize $w $WIDTH $HEIGHT 2>/dev/null; done
sleep 30
$XWD/bin/xwd -root -silent -display :97 | $IM/bin/magick xwd:- /out/azahar.png
DISPLAY=:97 $XDO/bin/xdotool mousemove ${TOUCH_X:-1100} ${TOUCH_Y:-400} mousedown 1 sleep 2 mouseup 1
sleep 20
$XWD/bin/xwd -root -silent -display :97 | $IM/bin/magick xwd:- /out/azahar-after-touch.png
cursor() {  # NAME: pixels where the X cursor differs at the plate's corner, with it (maim) and without (maim -u)
  DISPLAY=:97 $MAIM/bin/maim /out/$1-with.png; DISPLAY=:97 $MAIM/bin/maim -u /out/$1-without.png
  $IM/bin/magick /out/$1-with.png -crop 64x64+16+16 +repage /out/$1-with-crop.png
  $IM/bin/magick /out/$1-without.png -crop 64x64+16+16 +repage /out/$1-without-crop.png
  $IM/bin/magick /out/$1-with-crop.png -scale 400% /out/$1-zoom.png
  $IM/bin/magick /out/$1-with-crop.png /out/$1-without-crop.png -compose difference -composite -alpha off -separate -evaluate-sequence max -threshold 0 -format '%[fx:round(mean*w*h)]' info:
}
DISPLAY=:97 $XDO/bin/xdotool mousemove 30 30; sleep 0.3; DISPLAY=:97 $XDO/bin/xdotool mousemove 32 32; sleep 0.5
moving=$(cursor cursor-moving)
sleep 4
idle=$(cursor cursor-idle)
echo "azahar cursor: $moving pixels while moving, $idle after 4 s idle" | tee /out/cursor.result
kill $A; sleep 2; kill $X
grep -E "touch|swapchain" /out/azahar.log | head -20
if [ "${moving:-0}" -gt 0 ] && [ "${idle:-1}" -eq 0 ]; then echo "azahar cursor: shown while moving, hidden when idle" | tee -a /out/cursor.result; else echo "azahar cursor: FAIL" | tee -a /out/cursor.result; fi
INSIDE
podman run --name "semu-azahar-layer-$(date +%Y%m%d%H%M%S)" --platform linux/amd64 --privileged -e WAIT="${WAIT:-240}" -e WIDTH="${WIDTH:-1280}" -e HEIGHT="${HEIGHT:-720}" -e TOUCH_X="${TOUCH_X:-1100}" -e TOUCH_Y="${TOUCH_Y:-400}" -e HIDE_INACTIVE_MOUSE="${HIDE_INACTIVE_MOUSE:-}" \
  -e ROM_NAME="$(basename "$rom")" -v semu-nix-x86:/nix -v "$repository":/src:ro -v "$out":/out -v "$(dirname "$rom")":/rom:ro \
  docker.io/nixos/nix:latest bash /out/inside.sh
