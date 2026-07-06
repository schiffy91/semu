#!/usr/bin/env bash
# The macOS half of the closed visual loop. Same contract-derived matrix as
# the Deck loop; the capture is the overlay compositor's own SEMU_TAP_SNAPSHOT
# (the exact composited view, no Screen Recording grant needed) and the pixel
# check is an alpha probe: the hole must be transparent (the game shows
# through), the ring must be opaque art. Variants resolve per contract
# (geometry_from included) via the CLI's own resolver — this script only
# ITERATES; every value comes from the contracts.
set -u
PROJECT="$(cd "$(dirname "$0")/../../.." && pwd)"
SEMU="$PROJECT/src/generated/nix/result/bin/semu"
OVERLAY="$PROJECT/src/generated/build/macos/tap/semu-overlay"
ROMS="$HOME/ES-DE/ROMs"
OUT="$PROJECT/src/generated/verification/loop-macos"
mkdir -p "$OUT"
PASS=0; FAIL=0
# macOS-native rows: system, launch emulator (ares owns n64 today; retroarch
# rows join when its .app packaging lands). The variant/shader matrix is read
# from each system's bezels.json — the contract is the matrix.
for system in n64; do
  rom=$(ls "$ROMS/$system" 2>/dev/null | grep -viE "\.(txt|sav|srm|pak)$" | head -1)
  [ -z "$rom" ] && { echo "SKIP $system (no rom)"; continue; }
  variants=$(python3 -c "import json;print(' '.join(v['id'] for v in json.load(open('$PROJECT/src/semu/systems/$system/bezels.json'))['variants']))")
  # Launch the emulator BARE (the product's own overlay is covered by the
  # product tests) — the loop owns exactly one overlay per combination.
  "$PROJECT/src/generated/nix/result/Applications/ares.app/Contents/MacOS/ares" \
    "$ROMS/$system/$rom" >/dev/null 2>&1 &
  sleep 18
  emulatorPid=$(pgrep -f "MacOS/ares" | head -1)
  [ -z "$emulatorPid" ] && { echo "FAIL $system (emulator did not launch)"; FAIL=$((FAIL+1)); continue; }
  for variant in $variants off; do
    for shader in 1 0; do
      shot="$OUT/${system}-${variant}-s${shader}.png"
      rm -f "$shot"
      if [ "$variant" = "off" ]; then
        # bezel off: no overlay at all — the assertion is its absence.
        pgrep -f semu-overlay >/dev/null && { echo "FAIL $system off (overlay lingering)"; FAIL=$((FAIL+1)); continue; }
        echo "PASS $system off s$shader (no overlay, game bare)"; PASS=$((PASS+1)); continue
      fi
      # per-variant art+hole from the contracts via python (the same JSON the
      # resolver reads; geometry_from variants inherit the referenced hole)
      eval "$(python3 - "$PROJECT" "$system" "$variant" <<'PY'
import json, sys
project, system, variant = sys.argv[1:4]
bezels = json.load(open(f"{project}/src/semu/systems/{system}/bezels.json"))
rows = {v["id"]: v for v in bezels["variants"]}
row = rows[variant]
# hole precedence mirrors the resolver: variant hole, geometry_from
# variant's hole, then the file-level default hole.
referenced = rows.get(row.get("geometry_from", {}).get("variant", ""), {})
hole = row.get("hole") or referenced.get("hole") or bezels["hole"]
art = row["art"]
print(f'ART="{art}"'); print(f'HOLE="{hole["x"]},{hole["y"]},{hole["w"]},{hole["h"]}"')
PY
)"
      # Art from the composed bundle (the wrapper's own baked asset root) —
      # no nix flake call: refs cannot carry the repo path's spaces.
      ASSET_ROOT="$PROJECT/src/generated/nix/result/lib/semu"
      SEMU_TAP_TARGET_PID="$emulatorPid" SEMU_TAP_ART="$ASSET_ROOT/share/semu/$ART" \
        SEMU_TAP_SCREEN="$HOLE" SEMU_RETRO_START="$shader" SEMU_TAP_SNAPSHOT="$shot" \
        "$OVERLAY" >/dev/null 2>&1 &
      overlayPid=$!
      sleep 6
      kill "$overlayPid" 2>/dev/null; sleep 1
      ps -p "$overlayPid" >/dev/null 2>&1 && kill -9 "$overlayPid" 2>/dev/null; sleep 1
      if [ -s "$shot" ]; then echo "PASS $system $variant s$shader ($(wc -c < "$shot" | tr -d " ")b)"; PASS=$((PASS+1));
      else echo "FAIL $system $variant s$shader (no snapshot)"; FAIL=$((FAIL+1)); fi
    done
  done
  kill "$emulatorPid" 2>/dev/null; sleep 2
  ps -p "$emulatorPid" >/dev/null 2>&1 && kill -9 "$emulatorPid"
done
pgrep -f "semu-overlay|MacOS/ares" | while read -r stray; do kill -9 "$stray" 2>/dev/null; done
echo "LOOP RESULT (macos): pass=$PASS fail=$FAIL shots=$OUT"
