{
  description = "Semu build of RetroArch 1.22.2 from its pinned source, with the direct render hook";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:libretro/RetroArch/v1.22.2"; flake = false; };
    renderer = {
      url = "path:../../../src/renderer";
      inputs = { nixpkgs.follows = "nixpkgs"; btrc.follows = "btrc"; };  # one btrc pin for the bridge and the renderer
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
      platforms = { linux = true; macos = true; windows = "planned"; };  # the gl3 hook on GLX/EGL and on CGL
      systems = [ "x86_64-linux" "aarch64-darwin" ];
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        if pkgs.stdenv.hostPlatform.isDarwin then pkgs.callPackage ./darwin.nix {
          inherit source version;
          btrcpy = btrc.packages.${system}.btrcpy;
          semuRendererLoader = renderer.packages.${system}.loader;
          bridgeSource = renderer.retroarchBridge;
        } else pkgs.callPackage ./package.nix {
          inherit pkgs source version;
          btrcpy = btrc.packages.${system}.btrcpy;
          semuRendererLoader = renderer.packages.${system}.loader;
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
