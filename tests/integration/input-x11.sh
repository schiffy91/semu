#!/bin/sh
# Chords reach the supervisor however Steam sends them, checked on Linux on a private Xvfb: XTest
# keys (xdotool, the way Steam types into Game Mode's Xwayland) through the X raw-key adapter into
# a real RetroArch running the synthetic core, and Select chords from a virtual pad on /dev/uinput,
# positional and as a replica of Steam's virtual pad (BTN_WEST is its top button). Asserts the
# adapter listens, each chord runs exactly once, RetroArch's own keys no longer act on a chord
# (frame advance on K, reset on H), the pads' face buttons and the journal records. On a Mac it
# runs inside the podman VM (x86_64 under Rosetta, the release builder's Nix store) and never
# touches the Mac display: a rootless container builds everything, then a rootful one runs the
# session from that store mounted read-only, because only real root may open the VM's /dev/uinput
# (and /dev/input is bound in, so the pads it creates appear to the supervisor).
# Scratch lives in mktemp -d directories, containers are left exited, and nothing is removed.
#
#   input-x11.sh OUT_DIR             # on the Mac: builds this checkout's tracked files, then runs
#   input-x11.sh --build OUT_DIR     # stage 1 in Linux with nix: tools, virtual pad, OUT/paths.env
#   input-x11.sh --inside OUT_DIR    # stage 2 in Linux as root: the sessions, OUT/result
#
# SEMU_BASELINE_REV=<rev> adds a first session with that revision's RetroArch profile, to show what
# the harness sees when RetroArch's own keys still act. The exit status is the verdict.
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ] && [ "${1:-}" != "--build" ]; then
  out="$(mkdir -p "${1:?usage: input-x11.sh OUT_DIR}" && cd "$1" && pwd -P)"
  repository="$(cd "$(dirname "$0")/../.." && pwd -P)"
  [ "$(podman machine inspect --format '{{.State}}')" = running ] || podman machine start
  podman machine ssh 'test -e /proc/sys/fs/binfmt_misc/rosetta || { sudo touch /etc/containers/enable-rosetta && sudo systemctl start rosetta-activation.service; }'
  name="semu-input-x11-$(date +%Y%m%d%H%M%S)"  # both containers are left behind exited
  echo "containers $name-build and $name-run, results in $out"
  podman run --name "$name-build" --platform linux/amd64 --privileged \
    -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix -v "$repository":/src:ro -v "$out":/out \
    -e SEMU_BASELINE_REV="${SEMU_BASELINE_REV:-}" -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" "$image" bash /src/tests/integration/input-x11.sh --build /out
  store="$(podman volume inspect semu-nix-x86 --format '{{.Mountpoint}}')"
  podman machine ssh "sudo podman image exists $image" || podman image save "$image" | podman machine ssh 'sudo podman image load'
  exec podman machine ssh "sudo podman run --name $name-run --platform linux/amd64 --privileged --security-opt label=disable \
    -v /dev/input:/dev/input -v '$store':/nix:ro -v '$repository':/src:ro -v '$out':/out $image bash /src/tests/integration/input-x11.sh --inside /out"
fi
out="$2"

if [ "$1" = "--build" ]; then
  git config --global --add safe.directory '*'
  flake() { nix build --no-link --print-out-paths "git+file:///src#$1" 2>>"$out/nix.log" | tail -1; }
  package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
  echo "building (log: $out/nix.log)"
  cli="$(flake packages.x86_64-linux.semu-cli)"
  retroarch="$(flake packages.x86_64-linux.retroarch.unwrapped || true)"
  [ -x "$retroarch/bin/retroarch" ] || retroarch="$(flake packages.x86_64-linux.retroarch)"
  core="$(flake checks.x86_64-linux.synthetic-core)"
  renderer="$(flake packages.x86_64-linux.semu-renderer)"
  btrcpy="$(flake packages.x86_64-linux.btrcpy)"
  gcc="$(package gcc)"
  {
    echo "cli=$cli"; echo "retroarch=$retroarch"; echo "core=$core"; echo "renderer=$renderer"
    for tool in mesa xvfb xdotool socat xwd imagemagick gawk; do echo "$tool=$(package "$tool")"; done
  } > "$out/paths.env"
  mkdir -p "$out/tools"
  work="$(mktemp -d)"
  "$btrcpy/bin/btrcpy" --strict-imports --no-cache --no-stdlib /src/tests/visual/virtual_pad.btrc -o "$work/virtual_pad.c" >/dev/null
  "$gcc/bin/gcc" -std=c11 -O1 -w -I/src/src/launch "$work/virtual_pad.c" -o "$out/tools/virtual_pad"
  if [ -n "${SEMU_BASELINE_REV:-}" ]; then  # the old RetroArch profile in a copy of today's config, for comparison only
    cp -R /src/config "$out/baseline-config"
    chmod -R u+w "$out/baseline-config"
    git -C /src show "$SEMU_BASELINE_REV:config/emulators/retroarch/profile.json" > "$out/baseline-config/emulators/retroarch/profile.json"
  fi
  cat "$out/paths.env"
  exit 0
fi

. "$out/paths.env"
for tool in "$cli/lib/semu/semu-btrc" "$retroarch/bin/retroarch" "$core/lib/retroarch/cores/synthetic_libretro.so" "$renderer/lib/libsemurenderer.so" \
    "$xvfb/bin/Xvfb" "$xdotool/bin/xdotool" "$socat/bin/socat" "$gawk/bin/awk" "$out/tools/virtual_pad"; do
  [ -e "$tool" ] || { echo "FAIL: missing $tool" | tee "$out/result"; exit 1; }
done
awk="$gawk/bin/awk"
tr -c '[:print:]' '\n' < "$cli/lib/semu/semu-btrc" | grep -E '^/nix/store/.*libxcb' | "$awk" 'NR <= 2 { printf "semu-btrc loads %s\n", $0 }' | tee "$out/xcb-paths"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LIBGL_DRIVERS_PATH="$mesa/lib/dri" __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_DIRS="$mesa/share/glvnd/egl_vendor.d" LD_LIBRARY_PATH="$mesa/lib"
unset WAYLAND_DISPLAY
display=:91
"$xvfb/bin/Xvfb" "$display" -screen 0 1280x800x24 >"$out/xvfb.log" 2>&1 & xvfb_pid=$!
sleep 2
command_port() { printf '%s' "$1" | "$socat/bin/socat" -t 1 - UDP:127.0.0.1:55355 2>/dev/null | tr -d '\n'; }
key() { DISPLAY="$display" "$xdotool/bin/xdotool" key --delay 80 "$1"; sleep 1.5; }
shot() { DISPLAY="$display" "$xwd/bin/xwd" -root -silent | "$imagemagick/bin/magick" xwd:- "$1"; }
moving() {  # true when two captures a second apart differ: the core is running, not paused
  shot "$1-a.png"; sleep 1; shot "$1-b.png"
  [ "$("$imagemagick/bin/magick" compare -metric AE "$1-a.png" "$1-b.png" null: 2>&1 | cut -d' ' -f1)" != 0 ]
}
journal() { od -An -v -t d4 -w56 "$1" 2>/dev/null | "$awk" '{ printf "%s%s:%s", (NR > 1 ? " " : ""), $7, $9 }'; }  # action:slot per record
count() { grep -c -F "$2" "$1" || true; }
pad="$out/tools/virtual_pad"

session() {  # $1 label, $2 config root: one RetroArch session driven by keys, then by both pads
  label="$1"; config="$2"
  root="$(mktemp -d)"
  mkdir -p "$root/home" "$root/assets/bin" "$root/assets/lib/retroarch/cores" "$root/roms/gb"
  ln -s "$retroarch/bin/retroarch" "$root/assets/bin/retroarch"
  ln -s "$core/lib/retroarch/cores/synthetic_libretro.so" "$root/assets/lib/retroarch/cores/gambatte_libretro.so"
  ln -s "$renderer/lib/libsemurenderer.so" "$root/assets/lib/libsemurenderer.so"
  printf 'semu synthetic content\n' > "$root/roms/gb/pattern.semu"
  settings="{\"paths\":{\"roms\":\"$root/roms\",\"state_root\":\"$root/state\",\"content_root\":\"$root/content\"}}"
  log="$out/$label.log"
  HOME="$root/home" DISPLAY="$display" SEMU_RENDER_DEBUG=1 SDL_VIDEODRIVER=x11 "$cli/lib/semu/semu-btrc" launch retroarch --system gb --rom pattern.semu \
    --project "$config" --asset-root "$root/assets" --settings-json "$settings" --semu-home "$root/home/semu" > "$log" 2>&1 &
  launcher=$!
  version=""
  for _ in $(seq 1 120); do
    version="$(command_port VERSION || true)"
    [ -n "$version" ] && break
    kill -0 "$launcher" 2>/dev/null || break
    sleep 0.5
  done
  [ -n "$version" ] || { echo "$label: RetroArch never answered VERSION"; tail -20 "$log"; kill -TERM "$launcher" 2>/dev/null; return 1; }
  sleep 4
  shot "$out/$label-0-running.png"
  key ctrl+m; shot "$out/$label-1-menu.png"
  key ctrl+m
  key ctrl+k
  status_after_k="$(command_port GET_STATUS || true)"
  moving "$out/$label-2-after-ctrl-k" && running_after_k=yes || running_after_k=no
  key ctrl+h
  moving "$out/$label-3-after-ctrl-h" && running_after_h=yes || running_after_h=no
  key ctrl+shift+F9
  "$pad" 4 hold:select press:north release:select sleep:1.5 press:east > "$out/$label-pad.log" 2>&1 || true
  sleep 1
  "$pad" --steam-virtual-pad 4 hold:select press:north release:select sleep:1.5 press:east sleep:1.5 \
    hold:select press:west release:select sleep:1.5 hold:select hold:start release:start release:select > "$out/$label-steam-pad.log" 2>&1 || true
  for _ in $(seq 1 20); do kill -0 "$launcher" 2>/dev/null || break; sleep 0.5; done
  if kill -0 "$launcher" 2>/dev/null; then kill -TERM "$launcher"; quit=no; else quit=yes; fi
  wait "$launcher" 2>/dev/null || true
  cp "$root/state/retroarch/semu-render-actions.bin" "$out/$label-journal.bin" 2>/dev/null || true
  {
    echo "label=$label"
    echo "listening=$(count "$log" 'semu: listening for keys on X display')"
    echo "menu_keyboard=$(count "$log" 'semu: action ui.menu (keyboard)')"
    echo "next_keyboard=$(count "$log" 'semu: action state.next (keyboard)')"
    echo "shader_keyboard=$(count "$log" 'semu: action visual.shaders.toggle (keyboard)')"
    echo "wiimote_keyboard=$(count "$log" 'semu: action wii.controller.wiimote (keyboard)')"
    echo "menu_gamepad=$(count "$log" 'semu: action ui.menu (gamepad)')"
    echo "back_gamepad=$(count "$log" 'semu: action ui.menu.back (gamepad)')"
    echo "duplicates_dropped=$(count "$log" 'already ran from another input source')"
    echo "status_after_ctrl_k=$status_after_k"
    echo "running_after_ctrl_k=$running_after_k"
    echo "running_after_ctrl_h=$running_after_h"
    echo "core_resets=$(count "$log" 'synthetic: reset')"  # the core writes to RetroArch's stderr, which is the launcher's
    echo "quit_by_pad=$quit"
    echo "journal=$(journal "$out/$label-journal.bin")"
  } > "$out/$label.result"
  cat "$out/$label.result"
}

[ -d "$out/baseline-config" ] && { session baseline "$out/baseline-config" || true; }
session current /src/config || { kill "$xvfb_pid" 2>/dev/null; echo "FAIL: the session did not start" | tee -a "$out/result"; exit 1; }
kill "$xvfb_pid" 2>/dev/null || true

expect() { if [ "$2" = "$3" ]; then echo "ok   $1 ($2)"; else echo "FAIL $1: got $2, want $3"; fi; }
value() { "$awk" -v key="$1" 'index($0, key "=") == 1 { print substr($0, length(key) + 2) }' "$out/current.result"; }
{
  expect "the X adapter listens on the emulator's display" "$(value listening)" 1
  expect "Ctrl+M twice: ui.menu from the keyboard twice, once per press" "$(value menu_keyboard)" 2
  expect "Ctrl+K: state.next once" "$(value next_keyboard)" 1
  expect "Ctrl+H: the shader switch once" "$(value shader_keyboard)" 1
  expect "Ctrl+Shift+F9: a two-modifier chord once" "$(value wiimote_keyboard)" 1
  expect "RetroArch keeps running after Ctrl+K (frame advance cleared)" "$(value running_after_ctrl_k)" yes
  expect "RetroArch keeps running after Ctrl+H" "$(value running_after_ctrl_h)" yes
  expect "Ctrl+H does not reset the core" "$(value core_resets)" 0
  expect "Select+north opens the menu on the positional pad and on Steam's pad (BTN_WEST), Select+west on Steam's pad does not" "$(value menu_gamepad)" 2
  expect "B closes the menu from both pads" "$(value back_gamepad)" 2
  expect "Start+Select on Steam's pad quits" "$(value quit_by_pad)" yes
  expect "journal: menu, back, slot 1, shader switch, then each pad's menu and back" "$(value journal)" "1:0 5:0 77:1 12:0 1:0 5:0 1:0 5:0"
  case "$(value status_after_ctrl_k)" in *PLAYING*) echo "ok   GET_STATUS after Ctrl+K: $(value status_after_ctrl_k)" ;; *) echo "FAIL GET_STATUS after Ctrl+K: $(value status_after_ctrl_k)" ;; esac
  echo "note: Semu's uinput typing cannot echo on bare Xvfb (no evdev input), so echo suppression is proven by the contracts only"
} | tee "$out/result"
grep -q '^FAIL' "$out/result" && { echo "input-x11: FAIL"; exit 1; }
echo "input-x11: PASS" | tee -a "$out/result"
