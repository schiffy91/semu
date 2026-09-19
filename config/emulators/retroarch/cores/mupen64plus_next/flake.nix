{
  description = "Semu build of libretro core mupen64plus_next 0-unstable-f275caf4b2 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:libretro/mupen64plus-libretro-nx/f275caf4b2bfa1e6d1c51636746ea793f3d80320"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.mupen64plus.overrideAttrs (previous: {
          version = "0-unstable-f275caf4b2";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
        });
    in {
      semu = {
        id = "mupen64plus_next";
        version = "0-unstable-f275caf4b2";
        inherit platforms;
        source = { url = "github:libretro/mupen64plus-libretro-nx/f275caf4b2bfa1e6d1c51636746ea793f3d80320"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
