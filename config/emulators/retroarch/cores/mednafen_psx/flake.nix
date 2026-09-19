{
  description = "Semu build of libretro core mednafen_psx 0-unstable-82d8e051d1 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:libretro/beetle-psx-libretro/82d8e051d1c7741a18d930be90e458b48abaa9a1"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.beetle-psx.overrideAttrs (previous: {
          version = "0-unstable-82d8e051d1";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
        });
    in {
      semu = {
        id = "mednafen_psx";
        version = "0-unstable-82d8e051d1";
        inherit platforms;
        source = { url = "github:libretro/beetle-psx-libretro/82d8e051d1c7741a18d930be90e458b48abaa9a1"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
