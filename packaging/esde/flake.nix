{
  description = "Semu build of ES-DE 3.5.0 from its pinned source with the SEMU SETTINGS menu";

  inputs = {
    # ES-DE still depends on FreeImage, which current nixpkgs removed; this pin keeps its toolchain.
    nixpkgs.url = "github:NixOS/nixpkgs/ac62194c3917d5f474c1a844b6fd6da2db95077d";
    source = { url = "gitlab:es-de/emulationstation-de/50e4b600ae533d772bae3ff880d11a09b05dbe84"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # macOS builds an ES-DE.app against Nix libraries (darwin.nix)
      systems = [ "x86_64-linux" "aarch64-darwin" ];
      build = system:
        let
          esDePackages = import nixpkgs {
            inherit system;
            config.allowInsecurePredicate = package: lib.hasPrefix "freeimage" (lib.getName package);
          };
        in esDePackages.callPackage (if lib.hasSuffix "darwin" system then ./darwin.nix else ./package.nix) { inherit esDePackages source; };
    in {
      semu = {
        id = "es-de";
        version = "3.5.0";
        inherit platforms;
        source = { url = "gitlab:es-de/emulationstation-de"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
