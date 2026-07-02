{ lib, stdenvNoCC, fetchFromGitHub, imagemagick }:

let
  duimonMega = fetchFromGitHub {
    owner = "Duimon";
    repo = "Duimon-Mega-Bezel";
    rev = "d03dabf6e6b190dbf9b692efd492d8edd21abbb9";
    hash = "sha256-EvWkMiw5ne0Im43404NVTJiQO5mWhMVNEk7fdpgb5rY=";
  };

  duimonVintage = fetchFromGitHub {
    owner = "Duimon";
    repo = "Duimon-Vintage-TV";
    rev = "28478a2bcb21416c619dd2fa2d67d70d91c715dc";
    hash = "sha256-tGyIaMM169zIRD9tgmzLy+LthdWEjYtE0e+PmAuIR+4=";
  };

  soqueroeuTv = fetchFromGitHub {
    owner = "soqueroeu";
    repo = "Soqueroeu-TV-Backgrounds_V2.0";
    rev = "52bc4588d88c7bd319f922659d3ba74ed81ce959";
    hash = "sha256-8IoaMLw7i4TttFR08Ycji6COuaxaeNeBwPCLyfyTD90=";
  };

  generic4x3 = ../../emulators/rendering/assets/reshade/bezel-4x3.png;
  generic16x9 = ../../emulators/rendering/assets/reshade/bezel-16x9.png;
  genericDual = ../../emulators/rendering/assets/reshade/bezel-3ds.png;
in
stdenvNoCC.mkDerivation {
  pname = "semu-visual-assets";
  version = "2026-06-12";

  dontUnpack = true;

  nativeBuildInputs = [ imagemagick ];

  installPhase = ''
    runHook preInstall

    packs="$out/share/libretro/shaders/Mega_Bezel_Packs"
    mkdir -p "$packs"

    cp -R ${duimonVintage} "$packs/Duimon-Vintage-TV"
    chmod -R u+w "$packs/Duimon-Vintage-TV"

    cp -R ${soqueroeuTv} "$packs/Soqueroeu-TV-Backgrounds_V2.0"
    chmod -R u+w "$packs/Soqueroeu-TV-Backgrounds_V2.0"

    duimon="$packs/Duimon-Mega-Bezel"
    mkdir -p "$duimon"
    cp -R ${duimonMega}/Presets "$duimon/Presets"
    cp -R ${duimonMega}/res "$duimon/res"
    cp -R ${duimonMega}/zzz_global_params "$duimon/zzz_global_params"
    # Ship the full Graphics tree: the per-system Duimon "Guest" Bezel presets used for
    # RetroArch systems (gb/gbc/gba/nes/snes/genesis/n64) reference per-system LUTs and
    # bezel art (e.g. Graphics/SuperGBA/SuperGB_Gel.png). Copying only NDS/3DS made those
    # presets fail with "Failed to load LUT" / "Failed to create preset".
    cp -R ${duimonMega}/Graphics "$duimon/Graphics"
    for file in LICENSE LICENSE.md README README.md; do
      if [ -f "${duimonMega}/$file" ]; then
        cp "${duimonMega}/$file" "$duimon/$file"
      fi
    done
    chmod -R u+w "$duimon"

    # Photoreal handheld device SHELLS (+ iconic colorway/model variants), derived from the Duimon
    # art at build time. Runtime rendering uses these as bezel art; each device's screen hole is
    # declared in src/semu/systems/<system>/bezels.json. Recolor = desaturate then
    # Multiply a solid color (preserves plastic shading + alpha). gba variant b = the GBA SP clamshell
    # (a different model -> its own hole). Game fills the hole at the display aspect (SEMU_TAP_FILL).
    shells="$packs/semu-shells"; mkdir -p "$shells"
    gfx="$duimon/Graphics"
    # flat <out> <layers...> — -flatten the whole device LAYER STACK (not pairwise -composite, which only
    # merges the last two). Top adds the molded button definition; GBC's d-pad+A/B buttons are on a separate
    # Device_LED layer. Glass (screen) + the standalone LED layer are excluded.
    flat(){ out="$1"; shift; magick "$@" -background none -flatten -resize '2048x2048>' "$out"; }
    colorway(){ magick "$1" -modulate 100,0,100 \( +clone -fill "$3" -colorize 100% \) -compose Multiply -composite -alpha on "PNG32:$2"; }
    flat "$shells/gb.png"    "$gfx/Nintendo_Game_Boy/Gameboy_DMG01_Device.png" "$gfx/Nintendo_Game_Boy/Gameboy_DMG01_Decal.png" "$gfx/Nintendo_Game_Boy/Gameboy_DMG01_Top.png"
    flat "$shells/gbc.png"   "$gfx/Nintendo_Game_Boy_Color/GBC_Device.png" "$gfx/Nintendo_Game_Boy_Color/GBC_Device_LED.png" "$gfx/Nintendo_Game_Boy_Color/GBC_Decal.png" "$gfx/Nintendo_Game_Boy_Color/GBC_Top.png"
    flat "$shells/gba.png"   "$gfx/Nintendo_GBA/GBA_Device.png" "$gfx/Nintendo_GBA/GBA_Decal.png" "$gfx/Nintendo_GBA/GBA_Top.png"
    flat "$shells/gba-b.png" "$gfx/Nintendo_GBA_SP/GBA_SP_Device.png" "$gfx/Nintendo_GBA_SP/GBA_SP_Decal.png" "$gfx/Nintendo_GBA_SP/GBA_SP_Top.png"
    # screen-glass layers (live cutout MASK in .a + reflections in .rgb), downscaled, RGBA preserved
    glass(){ magick "$1" -resize '2048x2048>' "PNG32:$2"; }
    glass "$gfx/Nintendo_Game_Boy/Gameboy_DMG01_Glass.png" "$shells/gb-glass.png"
    glass "$gfx/Nintendo_Game_Boy_Color/GBC_Glass.png"     "$shells/gbc-glass.png"
    glass "$gfx/Nintendo_GBA/GBA_Glass.png"                "$shells/gba-glass.png"
    glass "$gfx/Nintendo_GBA_SP/GBA_SP_Glass.png"          "$shells/gba-b-glass.png"
    colorway "$shells/gb.png"  "$shells/gb-b.png"  '#d23440'   # Play It Loud red
    colorway "$shells/gb.png"  "$shells/gb-c.png"  '#2f9e4f'   # Play It Loud green
    colorway "$shells/gbc.png" "$shells/gbc-b.png" '#c0294b'   # Berry
    colorway "$shells/gbc.png" "$shells/gbc-c.png" '#7a3fb0'   # Grape

    # Geometry-preserving tap variants for systems without real B/C art. These are build output,
    # not source art: the source of truth remains the pinned asset pack plus rendering assets.
    variants="$packs/semu-tap-variants"; mkdir -p "$variants"
    variant_b(){ magick "$1" -alpha on -modulate 106,82,96 "PNG32:$2"; }
    variant_c(){ magick "$1" -alpha on -modulate 94,78,112 "PNG32:$2"; }
    variant_b "${generic4x3}" "$variants/generic-4x3-b.png"
    variant_c "${generic4x3}" "$variants/generic-4x3-c.png"
    variant_b "${generic16x9}" "$variants/generic-16x9-b.png"
    variant_c "${generic16x9}" "$variants/generic-16x9-c.png"
    variant_b "${genericDual}" "$variants/generic-dual-b.png"
    variant_c "${genericDual}" "$variants/generic-dual-c.png"
    variant_c "$shells/gba.png" "$variants/gba-c.png"
    variant_b "$packs/Soqueroeu-TV-Backgrounds_V2.0/img/Nintendo_N64/N64.png" "$variants/n64-b.png"
    variant_c "$packs/Soqueroeu-TV-Backgrounds_V2.0/img/Nintendo_N64/N64.png" "$variants/n64-c.png"
    variant_c "$packs/Soqueroeu-TV-Backgrounds_V2.0/img/Nintendo_NES/NES.png" "$variants/nes-c.png"
    variant_b "$packs/Soqueroeu-TV-Backgrounds_V2.0/img/Sony_Playstation/PSX.png" "$variants/psx-b.png"
    variant_c "$packs/Soqueroeu-TV-Backgrounds_V2.0/img/Sony_Playstation/PSX.png" "$variants/psx-c.png"

    # Deck-tuned presentation presets (measured on physical Steam Deck 2026-06-24).
    #
    # Consoles: a cozy Soqueroeu entertainment-center scene — a curved CRT TV in a dim living
    # room that FILLS the 16:10 screen (no black margins) — with ShortAxis integer scaling
    # (HSM_INT_SCALE_MODE=1). Why ShortAxis and not BothAxes: BothAxes (mode 2) both IGNORES the
    # height cap AND crashes the Soqueroeu presets; ShortAxis integer-scales the game crisply
    # inside the scene's tube without disturbing the room. nes/snes/genesis have per-system
    # scenes (the actual console sits below the TV); n64/psx use the generic CRT scene
    # (Soqueroeu only ships a flat-panel [FLAT] scene for those, which has black margins).
    #
    # Handhelds: the Duimon device shell with ShortAxis + curvature forced off (FLAT — handhelds
    # were never curved), capped at the shell's OWN resolved screen-fit ratio
    # (HSM_NON_INTEGER_SCALE pulled from each preset's #reference chain) so the largest integer
    # that keeps the whole device on-screen is chosen. All knobs are viewport-relative
    # percentages — nothing is a hardcoded pixel size, so it adapts to any screen.
    semudeck="$packs/semu-deck"
    mkdir -p "$semudeck"
    sq="../Soqueroeu-TV-Backgrounds_V2.0/presets/TV-Console-Night"
    printf '#reference "%s/Nintendo_NES.slangp"\nHSM_INT_SCALE_MODE = "1"\n' "$sq" > "$semudeck/nes.slangp"
    printf '#reference "%s/Nintendo_SuperNintendo-[Special_Grey].slangp"\nHSM_INT_SCALE_MODE = "1"\n' "$sq" > "$semudeck/snes.slangp"
    printf '#reference "%s/Sega_Genesis.slangp"\nHSM_INT_SCALE_MODE = "1"\n' "$sq" > "$semudeck/genesis.slangp"
    # n64 (mupen64plus_next): the launcher forces the core to native 320x240 (Mupen64Plus-Next.opt,
    # see launcherSeedRoutedState). MAX_HEIGHT=100 references the FULL screen (not the scene's tube),
    # so ShortAxis integer scaling picks floor(screenH / 240) — the largest integer that fits the
    # screen with ZERO buffer (3x = 720px on the Deck's 800px panel; auto-recomputes for any res/DPI).
    # HSM_BG_FILL_MODE=2 (STRETCH) makes the generic room background COVER the full 16:10 viewport —
    # the default (0=KEEP TEXTURE ASPECT) letterboxes the 16:9 scene, leaving black side bars. The
    # game/tube is a separate layer so the bg stretch doesn't touch it. (Native 240p required: at
    # mupen's ~480p+ default, integer mode over-scales past the screen.) Measured 960x720 on-Deck.
    printf '#reference "%s/00_Generic_02.slangp"\nHSM_INT_SCALE_MODE = "1"\nHSM_INT_SCALE_MAX_HEIGHT = "100"\nHSM_BG_FILL_MODE = "2"\n' "$sq" > "$semudeck/n64.slangp"
    printf '#reference "%s/00_Generic_02.slangp"\nHSM_INT_SCALE_MODE = "1"\n' "$sq" > "$semudeck/psx.slangp"
    dmb="../Duimon-Mega-Bezel/Presets/Advanced"
    printf '#reference "%s/Nintendo_Game_Boy/Gameboy-[ADV]-[Guest]-[Bezel]-[Night].slangp"\nHSM_INT_SCALE_MODE = "1"\nHSM_INT_SCALE_MAX_HEIGHT = "60.57"\nHSM_CURVATURE_MODE = "0"\n' "$dmb" > "$semudeck/gb.slangp"
    printf '#reference "%s/Nintendo_Game_Boy_Color/GBC-[ADV]-[Guest]-[Bezel]-[Night].slangp"\nHSM_INT_SCALE_MODE = "1"\nHSM_INT_SCALE_MAX_HEIGHT = "60.57"\nHSM_CURVATURE_MODE = "0"\n' "$dmb" > "$semudeck/gbc.slangp"
    printf '#reference "%s/Nintendo_GBA/GBA-[ADV]-[Guest]-[Bezel]-[Night].slangp"\nHSM_INT_SCALE_MODE = "1"\nHSM_INT_SCALE_MAX_HEIGHT = "59.77"\nHSM_CURVATURE_MODE = "0"\n' "$dmb" > "$semudeck/gba.slangp"
    # Additional RA-routable systems (cores in the bundle, ROMs on device, Dreamcast BIOS present).
    # gc/dreamcast are 4:3-capable consoles -> the cozy generic CRT scene. nds/psp are LCD handhelds
    # -> Duimon device shell, flat, capped at each shell's resolved screen-fit ratio.
    # gc (dolphin) renders at a high internal resolution like n64 -> integer mode over-scales the tube
    # off-screen (game fills the display, cozy room lost). Use art-driven fit (the 00_Generic_02 default
    # scale mode) so the game sits framed in the CRT. Verified on-Deck (F-Zero GX, DK Jungle Beat).
    printf '#reference "%s/00_Generic_02.slangp"\n' "$sq" > "$semudeck/gc.slangp"
    # dreamcast (flycast) outputs at DC native res -> integer mode frames it fine in the cozy CRT.
    printf '#reference "%s/00_Generic_02.slangp"\nHSM_INT_SCALE_MODE = "1"\n' "$sq" > "$semudeck/dreamcast.slangp"
    printf '#reference "%s/Nintendo_NDS/NDS-[ADV]-[Guest]-[Night].slangp"\nHSM_INT_SCALE_MODE = "1"\nHSM_INT_SCALE_MAX_HEIGHT = "117.17"\nHSM_CURVATURE_MODE = "0"\n' "$dmb" > "$semudeck/nds.slangp"
    printf '#reference "%s/SONY_PSP/PSP-[ADV]-[Guest]-[Night].slangp"\nHSM_INT_SCALE_MODE = "1"\nHSM_INT_SCALE_MAX_HEIGHT = "69.17"\nHSM_CURVATURE_MODE = "0"\n' "$dmb" > "$semudeck/psp.slangp"

    runHook postInstall
  '';

  meta = {
    description = "Pinned Semu visual asset packs for era-accurate shaders and bezels";
    platforms = lib.platforms.all;
  };
}
