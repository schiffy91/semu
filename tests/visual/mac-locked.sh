#!/usr/bin/env bash
# Every RetroArch system of the macos target, run while the Mac's screen is locked, so nothing ever
# reaches the owner's display. The lock hides the windows; the checks read what RetroArch itself
# reports: two core frames that are not blank and differ (the game runs), a state saved and loaded,
# and an exit within two seconds of QUIT. Each launch is semu's own plan (`--print-plan`) with one
# test-only line appended, `pause_nonactive = "false"`, because a locked session never focuses a
# window. The library is read-only; everything written goes to OUT_DIR.
# usage: mac-locked.sh BUNDLE OUT_DIR [system...]   (refuses to run while the screen is unlocked)
set -uo pipefail
bundle="$(cd "$1" && pwd -P)"; out="$2"; shift 2
locked() { ioreg -n Root -d1 -a | grep -A1 CGSSessionScreenIsLocked | tail -1 | grep -q '<true/>'; }
locked || { echo "mac-locked: the screen is unlocked; lock it first, this never draws on the owner's display" >&2; exit 2; }
root="$(cd "$(dirname "$0")/../.." && pwd)"
socat="$(nix build --no-warn-dirty --no-link --print-out-paths --inputs-from "$root" nixpkgs#socat)/bin/socat"
port() { printf '%s' "$1" | "$socat" -t 1 - UDP:127.0.0.1:55355 2>/dev/null | tr -d '\n'; }
milliseconds() { perl -MTime::HiRes=time -e 'printf "%d", time * 1000'; }
config="$bundle/share/semu/config"
roms="$("$bundle/bin/semu" path roms --target macos)"
systems=("$@")
[ ${#systems[@]} -gt 0 ] || systems=(gb gbc gba nes snes genesis n64 psx nds psp)
mkdir -p "$out"
failures=0
for system in "${systems[@]}"; do
  directory="$roms/$(jq -r '.rom.dir' "$config/systems/$system/system.json")"
  rom="$(ls "$directory" 2>/dev/null | grep -viE '\.(txt|xml|json|png|jpg|pak|srm|sav|eep|mpk|ram)$' | head -1)"
  [ -n "$rom" ] || { echo "$system: no ROM in $directory"; failures=$((failures + 1)); continue; }
  work="$out/$system"; rm -rf "$work"; mkdir -p "$work/content/states" "$work/content/saves" "$work/content/screenshots"
  "$bundle/bin/semu" launch retroarch --system "$system" --rom "$rom" --target macos --asset-root "$bundle" --semu-home "$work/home" \
    --settings-json "{\"paths\":{\"state_root\":\"$work/state\",\"content_root\":\"$work/content\"}}" --print-plan > "$work/plan.json" \
    || { echo "$system: no launch plan"; failures=$((failures + 1)); continue; }
  printf 'pause_nonactive = "false"\n' > "$work/test-only.cfg"
  mapfile -t command < <(jq -r --arg extra "$work/test-only.cfg" '.argv[0:2] + ["--appendconfig", $extra] + .argv[2:] | .[]' "$work/plan.json")
  mapfile -t variables < <(jq -r '.environment[]' "$work/plan.json")
  env "${variables[@]}" "${command[@]}" > "$work/retroarch.log" 2>&1 &
  retroarch=$!
  version=""
  for _ in $(seq 1 80); do version="$(port VERSION)"; [ -n "$version" ] && break; kill -0 "$retroarch" 2>/dev/null || break; sleep 0.5; done
  case "$system" in n64) boot=25 ;; psx) boot=15 ;; *) boot=10 ;; esac  # the darwin N64 core has no dynarec
  sleep "${SEMU_BOOT_WAIT:-$boot}"; port SCREENSHOT >/dev/null
  sleep 3; port SCREENSHOT >/dev/null
  sleep 1; port SAVE_STATE >/dev/null; sleep 2; port LOAD_STATE >/dev/null; sleep 1
  locked || { kill -KILL "$retroarch" 2>/dev/null; echo "mac-locked: the screen was unlocked mid-run; stopped" >&2; exit 2; }
  start="$(milliseconds)"; port QUIT >/dev/null
  while kill -0 "$retroarch" 2>/dev/null && [ $(( $(milliseconds) - start )) -lt 5000 ]; do sleep 0.05; done
  quit=$(( $(milliseconds) - start ))
  kill -0 "$retroarch" 2>/dev/null && { kill -KILL "$retroarch"; quit=-1; }
  wait "$retroarch" 2>/dev/null
  shots=(); while IFS= read -r -d '' shot; do shots+=("$shot"); done < <(find "$work/content/screenshots" -name '*.png' -print0 | sort -z)
  spreads=""; for shot in "${shots[@]}"; do spreads="$spreads $(magick identify -format '%[fx:standard_deviation]' "$shot")"; done
  moving="no"; [ ${#shots[@]} -ge 2 ] && ! cmp -s "${shots[0]}" "${shots[1]}" && moving="yes"
  state="$(find "$work/content/states" -type f | head -1)"
  lively="$(awk -v list="$spreads" 'BEGIN { n = split(list, values, " "); ok = n >= 2; for (i = 1; i <= n; i++) if (values[i] <= 0.02) ok = 0; print ok ? "yes" : "no" }')"
  if [ -z "$version" ]; then echo "$system: RetroArch never answered ($rom)"; failures=$((failures + 1))
  elif [ "$lively" != yes ] || [ "$moving" != yes ]; then echo "$system: frames blank or frozen (spreads$spreads, moving $moving)"; failures=$((failures + 1))
  elif [ -z "$state" ]; then echo "$system: SAVE_STATE wrote nothing"; failures=$((failures + 1))
  elif [ "$quit" -lt 0 ] || [ "$quit" -gt 2000 ]; then echo "$system: QUIT took ${quit} ms"; failures=$((failures + 1))
  else echo "$system: $rom, frames$spreads (moving), state saved and loaded, quit in ${quit} ms"; fi
done
exit "$failures"
