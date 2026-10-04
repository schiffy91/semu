#!/bin/bash
# The Semu menu is modal for pad input: while it is open the emulator's SDL sees none of the pad.
# On Linux in the podman VM, `semu launch azahar` runs a stand-in Azahar, an SDL program
# (sdl2-jstest --event) that prints every joystick event it receives with a timestamp, while a
# replica of Steam's virtual pad (tests/visual/virtual_pad.btrc --steam-virtual-pad) plays a fixed
# script: A; A held while Select+Y opens the menu, then A released; the d-pad, R1, L1, X, L3 and R2
# inside the menu; B closes it; A and d-pad up after; Start+Select quits. Azahar's menu.pause is
# none, so nothing pauses the stand-in: whatever stops its events is the menu's hold on the pad.
# The emulator must see A and Select+Y go down and come up, nothing of the menu or of B, and the
# presses after; the supervisor's "holds" and "gave back" lines bound the window in which it may
# see nothing. A rootless container builds, then a rootful one runs the session from that Nix store
# mounted read-only, with /dev/input bound in, because only real root may open the VM's /dev/uinput.
# Never touches the Mac display; scratch lives in mktemp -d, containers are left exited, nothing
# is removed.
#
#   menu-modal.sh OUT_DIR             # on the Mac: builds this checkout's tracked files, then runs
#   menu-modal.sh --build OUT_DIR     # stage 1 in Linux with nix: tools, virtual pad, OUT/paths.env
#   menu-modal.sh --inside OUT_DIR    # stage 2 in Linux as root: the sessions, OUT/result
#
# SEMU_BASELINE_REV=<rev> adds a first session with that revision's semu, to show what the emulator
# sees when the menu does not hold the pad. The exit status is the verdict.
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ] && [ "${1:-}" != "--build" ]; then
  out="$(mkdir -p "${1:?usage: menu-modal.sh OUT_DIR}" && cd "$1" && pwd -P)"
  repository="$(cd "$(dirname "$0")/../.." && pwd -P)"
  [ "$(podman machine inspect --format '{{.State}}')" = running ] || podman machine start
  podman machine ssh 'test -e /proc/sys/fs/binfmt_misc/rosetta || { sudo touch /etc/containers/enable-rosetta && sudo systemctl start rosetta-activation.service; }'
  name="semu-menu-modal-$(date +%Y%m%d%H%M%S)"  # both containers are left behind exited
  [ -n "${SEMU_BASELINE_REV:-}" ] && SEMU_BASELINE_REV="$(git -C "$repository" rev-parse "$SEMU_BASELINE_REV")"  # nix wants the full hash
  echo "containers $name-build and $name-run, results in $out"
  podman run --name "$name-build" --platform linux/amd64 --privileged \
    -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix -v "$repository":/src:ro -v "$out":/out \
    -e SEMU_BASELINE_REV="${SEMU_BASELINE_REV:-}" -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" "$image" bash /src/tests/integration/menu-modal.sh --build /out
  store="$(podman volume inspect semu-nix-x86 --format '{{.Mountpoint}}')"
  podman machine ssh "sudo podman image exists $image" || podman image save "$image" | podman machine ssh 'sudo podman image load'
  exec podman machine ssh "sudo podman run --name $name-run --platform linux/amd64 --privileged --security-opt label=disable \
    -v /dev/input:/dev/input -v '$store':/nix:ro -v '$repository':/src:ro -v '$out':/out $image bash /src/tests/integration/menu-modal.sh --inside /out"
fi
out="$2"

if [ "$1" = "--build" ]; then
  git config --global --add safe.directory '*'
  flake() { nix build --no-link --print-out-paths "git+file:///src${2:+?rev=$2}#$1" 2>>"$out/nix.log" | tail -1; }
  package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
  echo "building (log: $out/nix.log)"
  cli="$(flake packages.x86_64-linux.semu-cli)"
  baseline=""
  [ -n "${SEMU_BASELINE_REV:-}" ] && baseline="$(flake packages.x86_64-linux.semu-cli "$SEMU_BASELINE_REV")"
  btrcpy="$(flake packages.x86_64-linux.btrcpy)"
  gcc="$(package gcc)"
  {
    echo "cli=$cli"; echo "baseline=$baseline"; echo "baseline_rev=${SEMU_BASELINE_REV:-}"
    for tool in sdl-jstest coreutils gawk; do echo "${tool%%-*}=$(package "$tool")"; done
  } > "$out/paths.env"
  mkdir -p "$out/tools"
  work="$(mktemp -d)"
  "$btrcpy/bin/btrcpy" --strict-imports --no-cache --no-stdlib /src/tests/visual/virtual_pad.btrc -o "$work/virtual_pad.c" >/dev/null
  "$gcc/bin/gcc" -std=c11 -O1 -w -I/src/src/launch "$work/virtual_pad.c" -o "$out/tools/virtual_pad"
  cat "$out/paths.env"
  exit 0
fi

. "$out/paths.env"
jstest="$sdl/bin/sdl2-jstest"
for tool in "$cli/lib/semu/semu-btrc" "$jstest" "$coreutils/bin/stdbuf" "$gawk/bin/awk" "$out/tools/virtual_pad"; do
  [ -e "$tool" ] || { echo "FAIL: missing $tool" | tee "$out/result"; exit 1; }
done
awk="$gawk/bin/awk"
export PATH="$coreutils/bin:$PATH"
stamp() { while IFS= read -r line; do printf '%s %s\n' "$(date +%s.%N)" "$line"; done; }
script=(press:south sleep:0.5
  hold:south hold:select press:north release:select sleep:1 release:south sleep:1
  press:dpad_down press:dpad_down press:dpad_up press:dpad_up press:tr press:tl press:west press:thumbl press:tr2
  press:east sleep:1
  press:south press:dpad_up sleep:1
  hold:select hold:start release:start release:select)

session() {  # $1 label, $2 semu-cli: one launch of the stand-in Azahar driven by the replica of Steam's pad
  label="$1"; semu="$2/lib/semu/semu-btrc"
  root="$(mktemp -d)"
  mkdir -p "$root/home" "$root/assets/bin" "$root/roms/n3ds"
  printf 'semu modal stand-in\n' > "$root/roms/n3ds/stand-in.3ds"
  sdl_log="$out/$label-sdl.log"
  cat > "$root/assets/bin/azahar" <<STAND_IN
#!$(command -v bash)
# Stands in for Azahar: an SDL program printing every joystick event it receives, each stamped.
export SDL_JOYSTICK_DISABLE_UDEV=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
export SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1  # SDL ignores a Steam virtual pad without it; Steam sets it for every game
"$coreutils/bin/stdbuf" -oL "$jstest" --event 0 2>&1 | while IFS= read -r line; do printf '%s %s\n' "\$("$coreutils/bin/date" +%s.%N)" "\$line"; done > "$sdl_log"
STAND_IN
  chmod +x "$root/assets/bin/azahar"
  settings="{\"paths\":{\"roms\":\"$root/roms\",\"state_root\":\"$root/state\",\"content_root\":\"$root/content\",\"emulation_root\":\"$root/emulation\",\"bios\":\"$root/emulation\"}}"
  "$out/tools/virtual_pad" --steam-virtual-pad 9 "${script[@]}" > "$out/$label-pad.log" 2>&1 & pad=$!
  sleep 1.5
  HOME="$root/home" SEMU_RENDER_DEBUG=1 "$semu" launch azahar --system n3ds --rom stand-in.3ds --project /src/config \
    --asset-root "$root/assets" --settings-json "$settings" --semu-home "$root/home/semu" > >(stamp > "$out/$label-launch.log") 2>&1 &
  launcher=$!
  wait "$pad" 2>/dev/null || true
  for _ in $(seq 1 20); do kill -0 "$launcher" 2>/dev/null || break; sleep 0.5; done
  if kill -0 "$launcher" 2>/dev/null; then kill -TERM "$launcher"; quit=no; else quit=yes; fi
  wait "$launcher" 2>/dev/null || true
  sleep 1
  sequence="$("$awk" '$2 == "SDL_JOYBUTTONDOWN:" || $2 == "SDL_JOYBUTTONUP:" { for (field = 3; field <= NF; field++) if ($field == "button:") button = $(field + 1); printf "%sb%s%s", separator, button, ($2 == "SDL_JOYBUTTONDOWN:" ? "+" : "-"); separator = " " }
    $2 == "SDL_JOYHATMOTION:" { for (field = 3; field <= NF; field++) if ($field == "value:") position = $(field + 1); printf "%sh%s", separator, position; separator = " " }' "$sdl_log")"
  held="$("$awk" '/semu: the menu holds/ { print $1; exit }' "$out/$label-launch.log")"
  freed="$("$awk" '/semu: the menu gave/ { print $1; exit }' "$out/$label-launch.log")"
  inside="n/a"
  # The release that lets the menu grab reaches both readers at once, and two stampers order it within
  # milliseconds either way; the script leaves a second on each side, so 0.1 s of slack hides nothing.
  [ -n "$held" ] && [ -n "$freed" ] && inside="$("$awk" -v from="$held" -v to="$freed" '$1 > from + 0.1 && $1 < to && $2 ~ /^SDL_JOY/ { count++ } END { print count + 0 }' "$sdl_log")"
  {
    echo "label=$label"
    echo "sdl_opened=$(grep -c -E 'SDL_JOYDEVICEADDED|Joystick Name|Name:' "$sdl_log" || true)"
    echo "menu_gamepad=$(grep -c -F 'semu: action ui.menu (gamepad)' "$out/$label-launch.log" || true)"
    echo "back_gamepad=$(grep -c -F 'semu: action ui.menu.back (gamepad)' "$out/$label-launch.log" || true)"
    echo "down_gamepad=$(grep -c -F 'semu: action ui.menu.down (gamepad)' "$out/$label-launch.log" || true)"
    echo "holds=$(grep -c -F 'semu: the menu holds 1 pad(s)' "$out/$label-launch.log" || true)"
    echo "gave_back=$(grep -c -F 'semu: the menu gave 1 pad(s) back' "$out/$label-launch.log" || true)"
    echo "sdl_events_while_held=$inside"
    echo "quit_by_pad=$quit"
    echo "sequence=$sequence"
  } > "$out/$label.result"
  cat "$out/$label.result"
}

[ -n "${baseline:-}" ] && { session baseline "$baseline" || true; }
session current "$cli" || { echo "FAIL: the session did not run" | tee -a "$out/result"; exit 1; }

expect() { if [ "$2" = "$3" ]; then echo "ok   $1 ($2)"; else echo "FAIL $1: got '$2', want '$3'"; fi; }
value() { "$awk" -v key="$1" 'index($0, key "=") == 1 { print substr($0, length(key) + 2) }' "$out/current.result"; }
# Steam's pad as SDL numbers its buttons: 0 A (BTN_SOUTH), 3 the top button (BTN_WEST), 6 Select, 7 Start; hat 1 is up.
want="b0+ b0- b0+ b6+ b3+ b3- b6- b0- b0+ b0- h1 h0"  # then the quit chord, which ends the stand-in mid-print
{
  expect "Select plus the top button opens the menu once" "$(value menu_gamepad)" 1
  expect "the d-pad moves the open menu (two downs)" "$(value down_gamepad)" 2
  expect "B closes it" "$(value back_gamepad)" 1
  expect "the menu holds the pad once, after A came up" "$(value holds)" 1
  expect "and gives it back once, after B came up" "$(value gave_back)" 1
  expect "the emulator's SDL sees no joystick event while the menu holds the pad" "$(value sdl_events_while_held)" 0
  case "$(value sequence)" in
    "$want"*) echo "ok   the emulator sees A, Select+Y down and up and A's late release, nothing of the menu or of B, then A and d-pad up ($(value sequence))" ;;
    *) echo "FAIL the emulator's button sequence: got '$(value sequence)', want it to start '$want'" ;;
  esac
  expect "Start+Select still quits" "$(value quit_by_pad)" yes
  if [ -f "$out/baseline.result" ]; then  # the same script without the hold: the harness must see the leak
    leaked="$("$awk" -F= '$1 == "sequence" { print $2 }' "$out/baseline.result")"
    case "$leaked" in *"h4 h0 h4 h0"*"b1+ b1-"*) echo "ok   without the hold ($baseline_rev) the emulator saw the menu's d-pad and B ($leaked)" ;; *) echo "FAIL the baseline should show the menu's input reaching the emulator: $leaked" ;; esac
  fi
} | tee "$out/result"
grep -q '^FAIL' "$out/result" && { echo "menu-modal: FAIL"; exit 1; }
echo "menu-modal: PASS" | tee -a "$out/result"
