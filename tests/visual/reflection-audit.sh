#!/usr/bin/env bash
# The reflection audit on the real renderer (M15 item 8): every system x bezel variant x placement at the given sizes.
# Each cell renders through build/render-host as `semu render-env` declares it, then again with every
# SEMU_RENDER_SCREEN_<n>_REFLECT zeroed in the cell's variants file; the difference is the mirror and nothing else.
# A row per cell goes to OUT_DIR/reflection-audit.tsv (declared strength, pixels the mirror changes, its brightest
# step in 0..255, the verdict), with the picture and the doubled difference as PNGs to judge by eye.
# usage: reflection-audit.sh OUT_DIR [SIZE...] (default 1280x800 1920x1080); CELLS="nds:shell:fit n64:tv:game" narrows it.
# RENDER_HOST_CARD=/path/frame.png feeds a real frame (a game's native picture) instead of the test card.
# Verdicts: mirror (declared and drawn), none (nothing declared), none-no-shell (DS and 3DS screen placement drops the
# shell), MISSING (declared, shell shown, nothing drawn). Exits with the number of MISSING cells.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="$1"; shift
sizes=("$@"); [ ${#sizes[@]} -gt 0 ] || sizes=(1280x800 1920x1080)
make -C "$root" --no-print-directory build/semu build/render-host >/dev/null
nix build --no-warn-dirty --out-link "$root/build/asset-root" "$root#asset-root"
mkdir -p "$out"
scratch="$(mktemp -d)"  # states and PPMs, left behind
cells=()
if [ -n "${CELLS:-}" ]; then read -r -a cells <<<"$CELLS"; else
  for system in $(cd "$root/config/systems" && ls); do
    for variant in $(jq -r '.variants[].id' "$root/config/systems/$system/bezels.json"); do
      for placement in fit bezel game; do cells+=("$system:$variant:$placement"); done
    done
  done
fi
table="$out/reflection-audit.tsv"
printf 'system\tvariant\tplacement\tsize\tdeclared\tchanged_px\tpeak\tverdict\n' > "$table"
missing=0
for size in "${sizes[@]}"; do
  for cell in "${cells[@]}"; do
    IFS=: read -r system variant placement <<<"$cell"
    name="$system-$variant-$placement-$size"; state="$(mktemp -d "$scratch/state.XXXXXX")"
    settings="{\"visual\":{\"systems\":{\"$system\":{\"bezel_variant\":\"$variant\",\"placement\":\"$placement\"}}}}"
    row="$(
      while IFS= read -r line; do export "$line"; done < <("$root/build/semu" render-env --system "$system" --project "$root/config" \
        --asset-root "${SEMU_ASSET_ROOT:-$root/build/asset-root}" --settings-json "$settings" --variants-file "$state")
      declared="$(env | sed -n 's/^SEMU_RENDER_SCREEN_[01]_REFLECT=\([^,]*\),.*/\1/p' | sort -u | paste -sd/ -)"
      "$root/build/render-host" "${size%x*}" "${size#*x}" "$state/with.ppm" >"$state/with.log" 2>&1
      variants="$state/semu-render-variants.env"
      awk '/^SEMU_RENDER_SCREEN_[01]_REFLECT=/{split($0, pair, "="); print pair[1] "=0,0,0"; next} {print}' "$variants" > "$variants.zeroed" && mv "$variants.zeroed" "$variants"
      SEMU_RENDER_SCREEN_0_REFLECT=0,0,0 SEMU_RENDER_SCREEN_1_REFLECT=0,0,0 "$root/build/render-host" "${size%x*}" "${size#*x}" "$state/without.ppm" >"$state/without.log" 2>&1
      changed="$(magick compare -metric AE -fuzz 1.5% "$state/with.ppm" "$state/without.ppm" null: 2>&1 | cut -d' ' -f1)"
      peak="$(magick "$state/with.ppm" "$state/without.ppm" -compose difference -composite -colorspace gray -format '%[fx:round(maxima*255)]' info:)"
      magick "$state/with.ppm" "PNG24:$out/$name.png"
      magick "$state/with.ppm" "$state/without.ppm" -compose difference -composite -level 0,50% "PNG24:$out/$name-mirror.png"
      verdict=none
      if [ -n "$declared" ] && [ "$declared" != 0 ]; then
        if [ "$placement" = game ] && [ "${SEMU_RENDER_SURFACE_COUNT:-1}" = 2 ]; then verdict=none-no-shell
        elif [ "${changed%.*}" -gt 0 ] && [ "$peak" -gt 2 ]; then verdict=mirror; else verdict=MISSING; fi
      fi
      printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$system" "$variant" "$placement" "$size" "${declared:-0}" "$changed" "$peak" "$verdict"
    )"
    printf '%s\n' "$row" >> "$table"
    case "$row" in *MISSING) missing=$((missing + 1)); echo "MISSING: $name" >&2 ;; esac
  done
done
echo "$table: $(($(wc -l < "$table") - 1)) cells, $missing missing"
exit "$missing"
