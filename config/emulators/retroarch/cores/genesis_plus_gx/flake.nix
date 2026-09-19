{
  description = "Semu build of libretro core genesis_plus_gx 0-unstable-c2838c7dc4 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:libretro/Genesis-Plus-GX/c2838c7dc4236fc2fe94e5dbd08b41486067918e"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.genesis-plus-gx.overrideAttrs (previous: {
          version = "0-unstable-c2838c7dc4";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
        });
    in {
      semu = {
        id = "genesis_plus_gx";
        version = "0-unstable-c2838c7dc4";
        inherit platforms;
        source = { url = "github:libretro/Genesis-Plus-GX/c2838c7dc4236fc2fe94e5dbd08b41486067918e"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
