{
  description = "Semu build of ES-DE 3.4.0 from its pinned source with the SEMU SETTINGS menu";

  inputs = {
    # ES-DE still depends on FreeImage, which current nixpkgs removed; this pin keeps its toolchain.
    nixpkgs.url = "github:NixOS/nixpkgs/ac62194c3917d5f474c1a844b6fd6da2db95077d";
    source = { url = "gitlab:es-de/emulationstation-de/4f2830048ee002fee337cd7affea3d5333f8faf5"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = false; windows = "planned"; };  # upstream builds on macOS; the Nix recipe is Linux only today
      systems = [ "x86_64-linux" ];
      build = system:
        let
          esDePackages = import nixpkgs {
            inherit system;
            config.allowInsecurePredicate = package: lib.hasPrefix "freeimage" (lib.getName package);
          };
        in esDePackages.callPackage ./package.nix { inherit esDePackages source; };
    in {
      semu = {
        id = "es-de";
        version = "3.4.0";
        inherit platforms;
        source = { url = "gitlab:es-de/emulationstation-de"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
