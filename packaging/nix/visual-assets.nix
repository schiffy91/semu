{ lib, stdenvNoCC, fetchFromGitHub }:

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
in
stdenvNoCC.mkDerivation {
  pname = "semu-visual-assets";
  version = "2026-06-12";

  dontUnpack = true;

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
    mkdir -p "$duimon/Graphics"
    for name in _Backgrounds _Common _Common_Assets Nintendo_NDS Nintendo_3DS; do
      if [ -d "${duimonMega}/Graphics/$name" ]; then
        cp -R "${duimonMega}/Graphics/$name" "$duimon/Graphics/$name"
      fi
    done
    for file in LICENSE LICENSE.md README README.md; do
      if [ -f "${duimonMega}/$file" ]; then
        cp "${duimonMega}/$file" "$duimon/$file"
      fi
    done
    chmod -R u+w "$duimon"

    runHook postInstall
  '';

  meta = {
    description = "Pinned Semu visual asset packs for era-accurate shaders and bezels";
    platforms = lib.platforms.all;
  };
}
