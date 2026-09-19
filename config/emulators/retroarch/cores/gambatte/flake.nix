{
  description = "Semu build of libretro core gambatte 0-unstable-d9d6cd0638 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:libretro/gambatte-libretro/d9d6cd06382d1ced30de34d56d3609452323dab1"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.gambatte.overrideAttrs (previous: {
          version = "0-unstable-d9d6cd0638";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
        });
    in {
      semu = {
        id = "gambatte";
        version = "0-unstable-d9d6cd0638";
        inherit platforms;
        source = { url = "github:libretro/gambatte-libretro/d9d6cd06382d1ced30de34d56d3609452323dab1"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
