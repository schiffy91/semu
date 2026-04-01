# Pre-built RetroArch cores for macOS from the libretro buildbot.
# Nightly builds, pinned by hash for reproducibility.
{ lib, stdenv, fetchurl, unzip }:

let
  arch = if stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";
  baseUrl = "https://buildbot.libretro.com/nightly/apple/osx/${arch}/latest";

  # Best core for each system we have ROMs for
  cores = {
    gambatte         = { hash = "sha256-Y1gBR6N3C1VTk25N8Oc/xnFU9q+ZoWecqp/8HBVxyCQ="; desc = "GB/GBC"; };
    mgba             = { hash = "sha256-v1JZrZrtfOv40VWgBxmttDWeHn6GlSZWxj3FogA9o6M="; desc = "GBA"; };
    genesis_plus_gx  = { hash = "sha256-TAnHLRQmQUtwm61ujNlWphB9DHhBc4+Nf+AfFuu1csA="; desc = "Genesis"; };
    snes9x           = { hash = "sha256-/boCYf5Np+r6ho3D+hHQbRmp/Quq9tmWXBbryzflGxY="; desc = "SNES"; };
    mesen            = { hash = "sha256-5YYK6MMNc3ZHsoYWlKv0esWl+Xzvwe0lbdgaf66P3Dg="; desc = "NES"; };
    mupen64plus_next = { hash = "sha256-MWgaL2OoV71D21C5nP2GoRzKc6ZtK5MmE1nvoUrE9yg="; desc = "N64"; };
    desmume          = { hash = "sha256-8aIGJrN8B+XDd9wx02gtH9S02BAXNi+nlKsuLIU/E8k="; desc = "NDS"; };
    mednafen_psx_hw  = { hash = "sha256-zykVBUFlaP5W67SGGIkYYv6krqMVJp0RB5qLh/oTWro="; desc = "PSX"; };
    ppsspp           = { hash = "sha256-x7AAXg/oRFt7wsXCzNymU2Harm8NQpAJ7rFWxZREfrg="; desc = "PSP"; };
    flycast          = { hash = "sha256-6WEcYN56ClhyguHo4o7SKR1l+Ih/XdjC8fAOwj9jN3s="; desc = "Dreamcast"; };
  };

  fetchCore = name: info: fetchurl {
    url = "${baseUrl}/${name}_libretro.dylib.zip";
    inherit (info) hash;
    name = "${name}_libretro.dylib.zip";
  };
in
stdenv.mkDerivation {
  pname = "retroarch-cores";
  version = "nightly";
  srcs = lib.mapAttrsToList fetchCore cores;
  sourceRoot = ".";
  nativeBuildInputs = [ unzip ];
  unpackPhase = ''
    mkdir -p cores
    for src in $srcs; do
      unzip -o "$src" -d cores/
    done
  '';
  installPhase = ''
    mkdir -p $out/cores
    cp cores/*.dylib $out/cores/
  '';
  meta = {
    description = "Pre-built RetroArch cores for macOS (${lib.concatStringsSep ", " (lib.mapAttrsToList (_: i: i.desc) cores)})";
    platforms = [ "aarch64-darwin" "x86_64-darwin" ];
  };
}
