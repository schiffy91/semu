{
  description = "Semu build of PCSX2 2.6.3 from its pinned upstream source, with the direct render hook";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:PCSX2/pcsx2/v2.6.3"; flake = false; };
    renderer = {
      url = "path:../../../src/renderer";
      inputs = { nixpkgs.follows = "nixpkgs"; btrc.follows = "btrc"; };  # one btrc pin, shared with the renderer
    };
    btrc = {
      url = "github:schiffy91/btrc";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, source, renderer, ... }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = false; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        let semuRendererLoader = renderer.packages.${system}.loader; in  # the loader: renderer changes never rebuild PCSX2
        pkgs.pcsx2.overrideAttrs (previous: {
          version = "2.6.3";
          src = source // { tag = "v2.6.3"; };  # the recipe stamps PCSX2_GIT_TAG from it
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          patches = (previous.patches or [ ]) ++ [ ./semu_render_hook.patch ];  # GSDeviceOGL publishes the presented frame to libsemurenderer (ABI 3)
          buildInputs = (previous.buildInputs or [ ]) ++ [ semuRendererLoader ];
          env = (previous.env or { }) // {
            NIX_CFLAGS_COMPILE = (previous.env.NIX_CFLAGS_COMPILE or "") + " -DHAVE_SEMU_RENDERER -I${semuRendererLoader}/include";
            NIX_LDFLAGS = (previous.env.NIX_LDFLAGS or "") + " -L${semuRendererLoader}/lib --whole-archive -lsemurendererloader --no-whole-archive -ldl";
          };
        });
    in {
      semu = {
        id = "pcsx2";
        version = "2.6.3";
        inherit platforms;
        source = { url = "github:PCSX2/pcsx2/v2.6.3"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
