# RetroArch for macOS — pre-built universal DMG + pre-built cores from buildbot.
# nixpkgs retroarch is marked broken on Darwin, so we package the official builds.
# Cores are baked into the app bundle so no manual downloading is needed.
{ lib, stdenv, fetchurl, undmg, unzip, makeWrapper }:

let
  version = "1.22.2";

  arch = if stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";
  coreBaseUrl = "https://buildbot.libretro.com/nightly/apple/osx/${arch}/latest";

  cores = {
    gambatte         = { hash = "sha256-Y1gBR6N3C1VTk25N8Oc/xnFU9q+ZoWecqp/8HBVxyCQ="; desc = "GB/GBC"; };
    mgba             = { hash = "sha256-v1JZrZrtfOv40VWgBxmttDWeHn6GlSZWxj3FogA9o6M="; desc = "GBA"; };
    genesis_plus_gx  = { hash = "sha256-TAnHLRQmQUtwm61ujNlWphB9DHhBc4+Nf+AfFuu1csA="; desc = "Genesis"; };
    snes9x           = { hash = "sha256-/boCYf5Np+r6ho3D+hHQbRmp/Quq9tmWXBbryzflGxY="; desc = "SNES"; };
    mesen            = { hash = "sha256-5YYK6MMNc3ZHsoYWlKv0esWl+Xzvwe0lbdgaf66P3Dg="; desc = "NES"; };
    mupen64plus_next = { hash = "sha256-MWgaL2OoV71D21C5nP2GoRzKc6ZtK5MmE1nvoUrE9yg="; desc = "N64"; };
    desmume          = { hash = "sha256-8aIGJrN8B+XDd9wx02gtH9S02BAXNi+nlKsuLIU/E8k="; desc = "NDS"; };
    swanstation      = { hash = "sha256-36HY4rbnGqhWzJAYJ7hKEV+vKSFktgEsdZ77BwlE6rw="; desc = "PSX"; };
    mednafen_psx     = { hash = "sha256-PSDXxaJAQAUiD9B+bbxYHi7UgGIKB3u86gZa0kYzsto="; desc = "PSX (alt)"; };
    mednafen_psx_hw  = { hash = "sha256-zykVBUFlaP5W67SGGIkYYv6krqMVJp0RB5qLh/oTWro="; desc = "PSX (HW)"; };
    ppsspp           = { hash = "sha256-x7AAXg/oRFt7wsXCzNymU2Harm8NQpAJ7rFWxZREfrg="; desc = "PSP"; };
    flycast          = { hash = "sha256-6WEcYN56ClhyguHo4o7SKR1l+Ih/XdjC8fAOwj9jN3s="; desc = "Dreamcast"; };
    azahar           = { hash = "sha256-0GRdBdzGTUIpDodr5wbn1ELooQBgh2SefWWoWFdURkc="; desc = "3DS"; };
  };

  coreSrcs = lib.mapAttrsToList (name: info: fetchurl {
    url = "${coreBaseUrl}/${name}_libretro.dylib.zip";
    inherit (info) hash;
    name = "${name}_libretro.dylib.zip";
  }) cores;
in
stdenv.mkDerivation {
  pname = "retroarch";
  inherit version;
  src = fetchurl {
    url = "https://buildbot.libretro.com/stable/${version}/apple/osx/universal/RetroArch_Metal.dmg";
    hash = "sha256-gbeRIbom1TkGSuE7TQQZoSDD0WWvvmVs9fVBKxX9tDQ=";
  };
  sourceRoot = ".";
  nativeBuildInputs = [ undmg unzip makeWrapper ];
  installPhase = ''
    mkdir -p $out/Applications $out/cores $out/bin

    cp -r RetroArch.app $out/Applications/

    # Install pre-built cores
    ${lib.concatMapStringsSep "\n" (src: "unzip -o ${src} -d $out/cores/") coreSrcs}

    # Create a CLI wrapper that tells RetroArch where cores are
    makeWrapper $out/Applications/RetroArch.app/Contents/MacOS/RetroArch $out/bin/retroarch \
      --append-flags "--libretro-directory=$out/cores"
  '';
  meta = {
    description = "RetroArch with ${toString (lib.length (lib.attrNames cores))} cores (${lib.concatStringsSep ", " (lib.mapAttrsToList (_: i: i.desc) cores)})";
    homepage = "https://www.retroarch.com";
    platforms = [ "aarch64-darwin" "x86_64-darwin" ];
    license = lib.licenses.gpl3;
  };
}
