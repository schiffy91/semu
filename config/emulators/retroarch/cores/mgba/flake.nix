{
  description = "Semu build of libretro core mgba 0-unstable-7a12d6d4b9 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/d5cc6833999746cffdfe0598f7bc801a57b887a9";  # the first with the cmake recipe this source needs
    source = { url = "github:libretro/mgba/7a12d6d4b9acb14c0ae62c9166b6a2f3d08007f6"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.mgba.overrideAttrs (previous: {
          version = "0-unstable-7a12d6d4b9";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
        });
    in {
      semu = {
        id = "mgba";
        version = "0-unstable-7a12d6d4b9";
        inherit platforms;
        source = { url = "github:libretro/mgba/7a12d6d4b9acb14c0ae62c9166b6a2f3d08007f6"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
