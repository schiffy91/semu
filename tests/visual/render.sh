#!/usr/bin/env bash
# The real renderer on this Mac, no emulator: tests/visual/render.sh OUT_DIR WIDTHxHEIGHT CELL...
# A cell is system[:bezel_variant[:shader_variant]], one picture, or a live switch
# system:bezel:shader>bezel:shader[@placement] (either side's parts may be empty: the launch's own; @game
# or @bezel journals a Fit select, code 81; RENDER_HOST_SELECT=CODE,SLOT[,RESERVED];... adds any record), three pictures:
# -before, -toast (two frames after the supervisor's selects) and -after (eight more).
# Each cell is `semu render-env --variants-file` against the bundle's data (build/asset-root: plates, presets,
# layers) fed to build/render-host, its state in a fresh scratch directory. RENDER_HOST_ASPECT=1.7778 for 16:9 titles.
# RENDER_HOST_PLAYERS="P1  PAD 1  NUNCHUK|P2  NO PAD|RESTART GAME|BACK" writes the players page's rows the
# supervisor would (journal 86,1 shows them).
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="$1"; size="$2"; shift 2
width="${size%x*}"; height="${size#*x}"
make -C "$root" --no-print-directory build/semu build/render-host >/dev/null
nix build --no-warn-dirty --out-link "$root/build/asset-root" "$root#asset-root"
mkdir -p "$out"
scratch="$(mktemp -d)"  # the PPMs and each cell's state; left behind, never removed
failures=0

index() {  # VARIANTS_FILE KEY ID: ID's position in the header's KEY list, or the current one for an empty ID
  local file="$1" key="$2" id="$3" position=0 entry
  if [ -z "$id" ]; then
    local current; current="$(grep -m1 '^current=' "$file")"; current="${current#current=}"
    case "$key" in bezels) echo "$current" | cut -d, -f1 ;; shaders) echo "$current" | cut -d, -f2 ;; placements) echo "$current" | cut -d, -f3 ;; *) echo "$current" | cut -d, -f4 ;; esac  # bezel,shader,placement,output
    return
  fi
  IFS='|' read -r -a entries <<<"$(grep -m1 "^$key=" "$file" | cut -d= -f2-)"
  for entry in "${entries[@]}"; do [ "$entry" = "$id" ] && { echo "$position"; return; }; position=$((position + 1)); done
  echo "render.sh: $key has no $id" >&2; return 1
}

for cell in "$@"; do
  IFS=: read -r system bezel shader <<<"${cell%%>*}"
  switching=""; [ "$cell" != "${cell#*>}" ] && switching="${cell#*>}"
  settings="{\"visual\":{\"systems\":{\"$system\":{${bezel:+\"bezel_variant\":\"$bezel\"}${bezel:+${shader:+,}}${shader:+\"shader_variant\":\"$shader\"}}}}}"
  name="$system${bezel:+-$bezel}${shader:+-$shader}"
  state="$(mktemp -d "$scratch/state.XXXXXX")"
  [ -z "${RENDER_HOST_PLAYERS:-}" ] || printf "%s\n" "${RENDER_HOST_PLAYERS//|/$'\n'}" > "$state/semu-render-players.txt"
  pictures=("$name")
  placement=""; [ "$switching" != "${switching#*@}" ] && { placement="${switching#*@}"; switching="${switching%%@*}"; [ -n "$switching" ] || switching=":"; }
  if [ -n "$switching" ]; then IFS=: read -r afterBezel afterShader <<<"$switching"; name="$name-to-${afterBezel:-same}-${afterShader:-same}${placement:+-at-$placement}"; pictures=("$name-before" "$name-toast" "$name-after"); fi
  (
    while IFS= read -r line; do export "$line"; done < <("$root/build/semu" render-env --system "$system" --project "$root/config" \
      --asset-root "${SEMU_ASSET_ROOT:-$root/build/asset-root}" --settings-json "$settings" --variants-file "$state")
    if [ -z "$switching" ]; then
      "$root/build/render-host" "$width" "$height" "$scratch/$name.ppm"
    else
      variants="$state/semu-render-variants.env"
      fromBezel="$(index "$variants" bezels "")"; fromShader="$(index "$variants" shaders "")"
      toBezel="$(index "$variants" bezels "$afterBezel")"; toShader="$(index "$variants" shaders "$afterShader")"
      [ "$toBezel" = "$fromBezel" ] && toBezel=-1; [ "$toShader" = "$fromShader" ] && toShader=-1  # an unchanged kind journals nothing
      [ -z "$placement" ] || export RENDER_HOST_SELECT="81,$(index "$variants" placements "$placement")${RENDER_HOST_SELECT:+;$RENDER_HOST_SELECT}"  # the Fit select, as the radial journals it
      "$root/build/render-host" "$width" "$height" "$scratch/$name-before.ppm" --switch "$toBezel,$toShader" "$scratch/$name-toast.ppm" "$scratch/$name-after.ppm"
      grep 'phase=switch' "$state/semu-render-evidence.log" >&2 2>/dev/null || echo "render.sh: no phase=switch receipt" >&2
    fi
  ) 2>"$out/$name.log" && for picture in "${pictures[@]}"; do magick "$scratch/$picture.ppm" "PNG24:$out/$picture.png"; done \
    && echo "$name: $out/$name*.png" || { echo "$name: FAILED (see $out/$name.log)"; failures=$((failures + 1)); }
done
exit "$failures"
