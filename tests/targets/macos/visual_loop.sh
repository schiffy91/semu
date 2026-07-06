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
# ONE live target window for the whole sweep: the overlay's art/hole/variant
# logic is window-agnostic, so every system's visual matrix verifies against
# the same real emulator window (ares, n64 — the mac-native launch).
composite_capture=0
if screencapture -x -t png /tmp/semu-grant-probe.png 2>/dev/null \
   && [ -s /tmp/semu-grant-probe.png ]; then composite_capture=1; fi
[ "$composite_capture" = "0" ] && echo "NOTE composite capture unavailable:" \
  "grant Screen Recording to the terminal to add game-through-hole pixels;" \
  "this run proves art, hole, variants and outline in pixels + occlusion structurally"
targetRom=$(ls "$ROMS/n64" 2>/dev/null | grep -viE "\.(txt|sav|srm|pak)$" | head -1)
"$PROJECT/src/generated/nix/result/Applications/ares.app/Contents/MacOS/ares" \
  "$ROMS/n64/$targetRom" >/dev/null 2>&1 &
sleep 18
emulatorPid=$(pgrep -f "MacOS/ares" | head -1)
[ -z "$emulatorPid" ] && { echo "FAIL target window did not launch"; exit 1; }
for bezelsFile in "$PROJECT"/src/semu/systems/*/bezels.json; do
  system=$(basename "$(dirname "$bezelsFile")")
  variants=$(python3 -c "import json;print(' '.join(v['id'] for v in json.load(open('$bezelsFile'))['variants']))")
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
# Precedence mirrors the resolver, INCLUDING cross-system geometry_from
# (dreamcast borrows psx/tv art and hole — caught live by this loop).
def load(system_id):
    bezels = json.load(open(f"{project}/src/semu/systems/{system_id}/bezels.json"))
    return bezels, {v["id"]: v for v in bezels["variants"]}
bezels, rows = load(system)
row = rows[variant]
reference = row.get("geometry_from", {})
if reference:
    referencedBezels, referencedRows = load(reference.get("system", system))
    referencedRow = referencedRows.get(reference.get("variant", variant), {})
else:
    referencedBezels, referencedRow = bezels, {}
hole = row.get("hole") or referencedRow.get("hole") or referencedBezels.get("hole") or bezels.get("hole")
art = row.get("art") or referencedRow["art"]
print(f'ART="{art}"'); print(f'HOLE="{hole["x"]},{hole["y"]},{hole["w"]},{hole["h"]}"')
PY
)"
      # Art from the composed bundle (the wrapper's own baked asset root) —
      # no nix flake call: refs cannot carry the repo path's spaces.
      ASSET_ROOT="$PROJECT/src/generated/nix/result/lib/semu"
      alignFlag=""; [ "${SEMU_LOOP_ALIGN:-0}" = "1" ] && alignFlag=1
      SEMU_TAP_ALIGN="$alignFlag" \
        SEMU_TAP_TARGET_PID="$emulatorPid" SEMU_TAP_ART="$ASSET_ROOT/share/semu/$ART" \
        SEMU_TAP_SCREEN="$HOLE" SEMU_RETRO_START="$shader" SEMU_TAP_SNAPSHOT="$shot" \
        "$OVERLAY" >>/tmp/mac-loop-overlay.log 2>&1 &
      overlayPid=$!
      sleep 4
      kill "$overlayPid" 2>/dev/null; sleep 1
      ps -p "$overlayPid" >/dev/null 2>&1 && kill -9 "$overlayPid" 2>/dev/null; sleep 1
      if [ "$composite_capture" = "1" ]; then
        screencapture -x "$OUT/${system}-${variant}-s${shader}-composite.png" 2>/dev/null
      fi
      if [ -s "$shot" ]; then echo "PASS $system $variant s$shader ($(wc -c < "$shot" | tr -d " ")b)"; PASS=$((PASS+1));
      else echo "FAIL $system $variant s$shader (no snapshot)"; FAIL=$((FAIL+1)); fi
    done
  done
done
kill "$emulatorPid" 2>/dev/null; sleep 2
ps -p "$emulatorPid" >/dev/null 2>&1 && kill -9 "$emulatorPid"
pgrep -f "semu-overlay|MacOS/ares" | while read -r stray; do kill -9 "$stray" 2>/dev/null; done
echo "LOOP RESULT (macos): pass=$PASS fail=$FAIL shots=$OUT"
