{
  description = "Semu build of RetroArch 1.22.2 from its pinned source, with the direct render hook";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:libretro/RetroArch/v1.22.2"; flake = false; };
    renderer = {
      url = "path:../../../src/renderer";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    btrc = {
      url = "github:schiffy91/btrc";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, source, renderer, btrc }:
    let
      lib = nixpkgs.lib;
      version = "1.22.2";
      platforms = { linux = true; macos = false; windows = "planned"; };  # the gl3 hook is Linux today; Metal is the macOS port to write
      systems = [ "x86_64-linux" ];
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.callPackage ./package.nix {
          inherit pkgs source version;
          btrcpy = btrc.packages.${system}.btrcpy;
          semuRenderer = renderer.packages.${system}.default;
          bridgeSource = renderer.retroarchBridge;
        };
    in {
      semu = {
        id = "retroarch";
        inherit version platforms;
        source = { url = "github:libretro/RetroArch/v1.22.2"; rev = source.rev; narHash = source.narHash; };
      };
      sourceTree = source;  # the synthetic-core check compiles against libretro-common
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
