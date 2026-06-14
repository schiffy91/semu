{ lib, runCommand, libretro-shaders-slang, semuVisualAssets }:

runCommand "semu-shader-bundle" {
  meta = {
    description = "Semu reference shader and bezel asset tree";
    platforms = lib.platforms.all;
  };
} ''
  mkdir -p "$out/share/libretro/shaders"
  cp -aL ${libretro-shaders-slang}/share/libretro/shaders/shaders_slang \
    "$out/share/libretro/shaders/shaders_slang"
  cp -aL ${semuVisualAssets}/share/libretro/shaders/Mega_Bezel_Packs \
    "$out/share/libretro/shaders/Mega_Bezel_Packs"
''
