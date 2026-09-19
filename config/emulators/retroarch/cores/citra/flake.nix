{
  description = "Semu build of libretro core citra 0-unstable-e4cbe379bf from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "git+https://github.com/libretro/citra?rev=e4cbe379bfa8990af706977d699e16a618523778&submodules=1"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.citra.overrideAttrs (previous: {
          version = "0-unstable-e4cbe379bf";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
        });
    in {
      semu = {
        id = "citra";
        version = "0-unstable-e4cbe379bf";
        inherit platforms;
        source = { url = "git+https://github.com/libretro/citra?rev=e4cbe379bfa8990af706977d699e16a618523778&submodules=1"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
