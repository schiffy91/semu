#!/usr/bin/env bash
# Install the RetroArch cores Semu's es_systems routes to, into the flatpak RetroArch core dir.
#
# The Flatpak org.libretro.RetroArch ships with only a handful of cores, but Semu's generated
# es_systems.xml routes several systems to cores that aren't bundled:
#   nes  -> mesen          nds -> melonds (and desmume as alt)
#   psx  -> mednafen_psx
# Without these the affected systems fail to launch (or render black). This pulls the matching
# linux/x86_64 cores from the libretro buildbot (same ABI the Flatpak's own updater uses).
#
# Run once on the Deck after installing the Flatpak RetroArch. Idempotent.
set -euo pipefail
CORES="$HOME/.var/app/org.libretro.RetroArch/config/retroarch/cores"
mkdir -p "$CORES"
# Every core Semu routes to, so a fresh Deck has them all regardless of what the Flatpak shipped.
NEEDED=(mesen nestopia snes9x genesis_plus_gx gambatte mgba mupen64plus_next mednafen_psx melonds desmume)
for c in "${NEEDED[@]}"; do
  if [ -f "$CORES/${c}_libretro.so" ]; then echo "$c: present"; continue; fi
  echo "downloading $c..."
  if curl -fsSL "https://buildbot.libretro.com/nightly/linux/x86_64/latest/${c}_libretro.so.zip" -o "/tmp/${c}.zip"; then
    unzip -o "/tmp/${c}.zip" -d "$CORES" >/dev/null && echo "$c: installed ($(stat -c%s "$CORES/${c}_libretro.so") bytes)"
  else
    echo "$c: download FAILED"
  fi
done
echo "cores installed: $(ls "$CORES"/*.so 2>/dev/null | wc -l)"

# Controller autoconfig pack. The flatpak's bundled /app autoconfig has NO profile for the Steam
# Deck's virtual pad (Valve 28DE:11FF, presented as "Microsoft X-Box 360 pad"/"Steam Virtual
# Gamepad"), so RetroArch shows "<pad> not configured" and input is dead. Semu's generated config
# sets joypad_autoconfig_dir to this dir; install the full libretro pack so the pad matches.
AUTODIR="$HOME/.var/app/org.libretro.RetroArch/config/retroarch/autoconfig"
mkdir -p "$AUTODIR"
if [ -z "$(ls "$AUTODIR"/sdl2/*.cfg 2>/dev/null)" ]; then
  echo "installing autoconfig pack..."
  if curl -fsSL https://buildbot.libretro.com/assets/frontend/autoconfig.zip -o /tmp/autoconfig.zip; then
    unzip -oq /tmp/autoconfig.zip -d "$AUTODIR" && echo "autoconfig: $(find "$AUTODIR" -name '*.cfg' | wc -l) profiles"
  else
    echo "autoconfig: download FAILED"
  fi
else
  echo "autoconfig: present ($(find "$AUTODIR" -name '*.cfg' | wc -l) profiles)"
fi
