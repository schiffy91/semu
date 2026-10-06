#!/usr/bin/env bash
# The reflection audit on the real renderer (M15 item 8): every system x bezel variant x placement at the given sizes.
# Each cell renders through build/render-host as `semu render-env` declares it, then with every mirror zeroed
# (SEMU_RENDER_SCREEN_<n>_REFLECT and _REFLECT_B, in the environment and in the cell's variants file): the difference is
# the mirror and nothing else. A two-screen cell also renders once per screen with only that screen's mirror zeroed, so
# each screen is measured on its own: one screen with no lip cannot hide behind the other (the M15 review's 1-px 3DS
# touch lip). A variant that declares "output": "widescreen" (the Wii's 16:9 TV) is not in the radial's bezel list, so
# `semu render-env` never selects it by name: it is reached as the owner reaches it, through the variant that lists it
# under "outputs" on a 16:9 card (RENDER_HOST_ASPECT), which the renderer answers with its _B keys. Every cell checks the
# package the renderer's debug line names against the variant's own (PACKAGE when they differ). A system that declares
# no bezel gets a row per size, unrendered.
# A row per cell goes to OUT_DIR/reflection-audit.tsv (the committed copy is tests/visual/reflection-audit.tsv): the
# package drawn, declared strength, pixels the mirror changes, its brightest step in 0..255, each screen's reach (the pixels its own
# mirror changes over its picture's perimeter, about the band's mean width in pixels; lanes from the renderer's
# SEMU_RENDER_DEBUG lines), the live path, the verdict; the picture and the doubled difference go beside it as PNGs.
# usage: reflection-audit.sh OUT_DIR [SIZE...] (default 1280x800 1920x1080); CELLS="nds:shell:fit n64:tv:game" narrows it.
# RENDER_HOST_CARD=/path/frame.png feeds a real frame (a game's native picture) instead of the test card.
# Verdicts: mirror (declared and drawn on every screen), none (nothing declared), MISSING (declared, a screen draws
# none), THIN (a screen's reach under a quarter of its sibling's), PACKAGE (the renderer drew another package than the
# variant's). Exits with the number of failing cells.
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
    variants="$(jq -r '.variants[].id' "$root/config/systems/$system/bezels.json" 2>/dev/null || true)"
    [ -n "$variants" ] || { cells+=("$system:-:-"); continue; }  # no bezel declared (switch, wiiu)
    for variant in $variants; do
      for placement in game bezel game_fractional bezel_fractional; do cells+=("$system:$variant:$placement"); done  # the four Fit states
    done
  done
fi

live() {  # SYSTEM: the path that hands the compositor its frame on the Deck, then what was seen there
  case "$1" in
    dreamcast) printf 'flycast-gl-preload\tsame entry and compositor; Deck capture after deploy' ;;
    gc) printf 'dolphin-gl-preload\tsame entry and compositor; Deck capture after deploy' ;;
    wii) printf 'dolphin-gl-preload\tDeck capture 2026-10-04: white picture mirrored bright on the lip' ;;
    ps2) printf 'pcsx2-gl-preload\tDeck capture 2026-10-04: dark picture, dark mirror (expected)' ;;
    psp) printf 'ppsspp-gl-preload\tDeck capture 2026-10-04: lip tinted green by the picture' ;;
    n3ds) printf 'azahar-vulkan-layer\tsame entry and compositor; Deck capture after deploy' ;;
    n64) printf 'retroarch-gl-tap\tVM SM64 real core: native 313x237 after the crop, lip tinted by the picture (was 7 black columns)' ;;
    switch) printf 'ryujinx-vulkan-layer\t-' ;;
    wiiu) printf 'cemu-vulkan-layer\t-' ;;
    *) printf 'retroarch-gl-tap\tsame entry and compositor; Deck capture after deploy' ;;
  esac
}

chosen_variant() {  # BEZELS VARIANT: the bezel choice that reaches VARIANT, itself unless it declares an output; empty when no variant lists it under "outputs"
  if [ -z "$(jq -r --arg id "$2" '.variants[] | select(.id == $id) | .output // ""' "$1")" ]; then echo "$2"; return; fi
  jq -r --arg id "$2" '.variants[] | select((.outputs // {}) | to_entries | any(.value == $id)) | .id' "$1" | head -1
}

revision="$(git -C "$root" rev-parse --short HEAD)$(git -C "$root" diff --quiet -- src config tests/visual || echo '+local')"
table="$out/reflection-audit.tsv"
{
  echo "# M15 item 8 reflection audit: tests/visual/reflection-audit.sh on the render host at ${sizes[*]}, revision $revision, $(date -u +%Y-%m-%d), ${RENDER_HOST_CARD:-test} card; mirror = the picture with REFLECT minus without."
  echo "# expected: mirror when the package declares one (no placement drops a DS or 3DS shell, M15 item 2); none for computed layouts. reach0/reach1: each screen's own mirror pixels over its picture's perimeter (about the band's mean width, px)."
  echo "# package: the package the renderer's debug line names, which must be the variant's own; a variant that declares an output (wii tv_wide) is drawn through the variant listing it under outputs (wii tv) on a 16:9 card, as a launch reaches it."
  echo "# live: the path that hands the compositor its frame on the Deck; every path ends in semu_render_game_gl, which crops (N64) and extracts the reported content rect, so the mirror samples what the emulator presents there."
  printf 'system\tvariant\tpackage\tplacement\tsize\tdeclared\texpected\tchanged_px\tpeak\treach0_px\treach1_px\trender_host\tlive_path\tlive_evidence\tverdict\n'
} > "$table"
failures=0
for size in "${sizes[@]}"; do
  for cell in "${cells[@]}"; do
    IFS=: read -r system variant placement <<<"$cell"
    if [ "$variant" = - ]; then
      printf '%s\t-\t-\t-\t%s\t0\tnone (no bezel declared)\t0\t0\t-\t-\tnone\t%s\tok\n' "$system" "$size" "$(live "$system")" >> "$table"
      continue
    fi
    name="$system-$variant-$placement-$size"; state="$(mktemp -d "$scratch/state.XXXXXX")"
    bezels="$root/config/systems/$system/bezels.json"
    package="$(jq -r --arg id "$variant" '.variants[] | select(.id == $id) | .bezel' "$bezels")"
    chosen="$(chosen_variant "$bezels" "$variant")"  # the Wii's tv_wide is reached through tv: render-env never selects it by name
    settings="{\"visual\":{\"systems\":{\"$system\":{\"bezel_variant\":\"${chosen:-$variant}\",\"placement\":\"$placement\"}}}}"
    wide=""; [ "$(jq -r --arg id "$variant" '.variants[] | select(.id == $id) | .output // ""' "$bezels")" = widescreen ] && wide=1.7778
    row="$(
      while IFS= read -r line; do export "$line"; done < <("$root/build/semu" render-env --system "$system" --project "$root/config" \
        --asset-root "${SEMU_ASSET_ROOT:-$root/build/asset-root}" --settings-json "$settings" --variants-file "$state")
      [ -z "$wide" ] || export RENDER_HOST_ASPECT="$wide"
      variants="$state/semu-render-variants.env"
      cp "$variants" "$state/declared.env"
      if [ -z "$wide" ]; then
        declared="$(env | sed -n 's/^SEMU_RENDER_SCREEN_[01]_REFLECT=\([^,]*\),.*/\1/p' | sort -u | paste -sd/ -)"
      else  # the widescreen package's own keys, in the variants file's current section
        current="$(sed -n 's/^current=\([0-9]*\),\([0-9]*\),.*/[\1,\2]/p' "$variants")"
        declared="$(awk -v section="$current" '$0 == section { inside = 1; next } /^\[/ { inside = 0 } inside' "$variants" | sed -n 's/^SEMU_RENDER_SCREEN_[01]_REFLECT_B=\([^,]*\),.*/\1/p' | sort -u | paste -sd/ -)"
      fi
      screens=1; [ -n "${SEMU_RENDER_SURFACE_1_NATIVE:-}" ] && screens=2
      draw() {  # ZEROED OUT.ppm: the cell with the listed screens' mirrors zeroed
        local zeroed="$1" picture="$2"
        awk -v zeroed=" $zeroed " '/^SEMU_RENDER_SCREEN_[01]_REFLECT(_B)?=/ { split($0, pair, "="); if (index(zeroed, " " substr(pair[1], 20, 1) " ") > 0) { print pair[1] "=0,0,0"; next } } { print }' "$state/declared.env" > "$variants"
        ( for screen in $zeroed; do export "SEMU_RENDER_SCREEN_${screen}_REFLECT=0,0,0" "SEMU_RENDER_SCREEN_${screen}_REFLECT_B=0,0,0"; done
          SEMU_RENDER_DEBUG=1 "$root/build/render-host" "${size%x*}" "${size#*x}" "$picture" ) >"$picture.log" 2>&1
      }
      changed() { magick compare -metric AE -fuzz 1.5% "$1" "$2" null: 2>&1 | cut -d' ' -f1 | cut -d. -f1; }
      draw "" "$state/with.ppm"
      drawnPackage="$(sed -n 's/^semu-renderer: frame [0-9]* bezel \([^ ]*\) fb .*/\1/p' "$state/with.ppm.log" | head -1)"
      draw "0 1" "$state/without.ppm"
      total="$(changed "$state/with.ppm" "$state/without.ppm")"
      peak="$(magick "$state/with.ppm" "$state/without.ppm" -compose difference -composite -colorspace gray -format '%[fx:round(maxima*255)]' info:)"
      magick "$state/with.ppm" "PNG24:$out/$name.png"
      magick "$state/with.ppm" "$state/without.ppm" -compose difference -composite -level 0,50% "PNG24:$out/$name-mirror.png"
      reaches=(- -)
      if [ -n "$declared" ] && [ "$declared" != 0 ]; then
        for screen in $(seq 0 $((screens - 1))); do
          own="$total"
          if [ "$screens" = 2 ]; then draw "$screen" "$state/without$screen.ppm"; own="$(changed "$state/with.ppm" "$state/without$screen.ppm")"; fi
          lane="$(sed -n "s/.* lane$screen out -\{0,1\}[0-9]*,-\{0,1\}[0-9]* \([0-9]*\)x\([0-9]*\) .*/\1 \2/p" "$state/with.ppm.log" | head -1)"
          reaches[$screen]="$(awk -v own="$own" -v lane="$lane" 'BEGIN { split(lane, size, " "); perimeter = 2 * (size[1] + size[2]); printf "%.1f", (perimeter > 0 ? own / perimeter : 0) }')"
        done
      fi
      expected=none; drawn=none
      if [ -n "$declared" ] && [ "$declared" != 0 ]; then
        expected=mirror; drawn=mirror
        [ "$total" -gt 0 ] && [ "$peak" -gt 2 ] || drawn=MISSING
        for screen in $(seq 0 $((screens - 1))); do awk -v reach="${reaches[$screen]}" 'BEGIN { exit !(reach < 0.5) }' && drawn=MISSING; done
        if [ "$screens" = 2 ] && [ "$drawn" = mirror ]; then
          awk -v first="${reaches[0]}" -v second="${reaches[1]}" 'BEGIN { exit !(first < second / 4 || second < first / 4) }' && drawn=THIN
        fi
      elif [ "$total" -gt 0 ]; then drawn=UNDECLARED
      fi
      [ "$drawnPackage" = "$package" ] || drawn=PACKAGE  # another package's mirror measures nothing about this variant
      verdict=ok; [ "$drawn" = "$expected" ] || verdict=FAIL
      printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$system" "$variant" "${drawnPackage:-nothing}" "$placement" "$size" "${declared:-0}" "$expected" "$total" "$peak" "${reaches[0]}" "${reaches[1]}" "$drawn" "$(live "$system")" "$verdict"
    )"
    printf '%s\n' "$row" >> "$table"
    case "$row" in *FAIL) failures=$((failures + 1)); echo "FAIL: $name: $row" >&2 ;; esac
  done
done
echo "$table: $(($(grep -vc '^#' "$table") - 1)) rows, $failures failing"
exit "$failures"
