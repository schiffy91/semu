{
  description = "Semu: a declarative, Nix-built emulation environment";

  # Every emulator, core, RetroArch, ES-DE and the renderer is its own flake with its own
  # pinned source; this flake only composes them. nixpkgs is shared through follows.
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    btrc = {
      url = "github:schiffy91/btrc";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    renderer = {
      url = "path:./src/renderer";
      inputs = { nixpkgs.follows = "nixpkgs"; btrc.follows = "btrc"; };
    };
    retroarch = {
      url = "path:./config/emulators/retroarch";
      inputs = { nixpkgs.follows = "nixpkgs"; renderer.follows = "renderer"; btrc.follows = "btrc"; };
    };
    esde.url = "path:./packaging/esde";  # keeps its own older nixpkgs for FreeImage
    emulator-azahar = { url = "path:./config/emulators/azahar"; inputs.nixpkgs.follows = "nixpkgs"; };
    emulator-cemu = { url = "path:./config/emulators/cemu"; inputs.nixpkgs.follows = "nixpkgs"; };
    emulator-dolphin = { url = "path:./config/emulators/dolphin"; inputs.nixpkgs.follows = "nixpkgs"; };
    emulator-flycast = { url = "path:./config/emulators/flycast"; inputs.nixpkgs.follows = "nixpkgs"; };
    emulator-melonds = { url = "path:./config/emulators/melonds"; inputs.nixpkgs.follows = "nixpkgs"; };
    emulator-pcsx2 = { url = "path:./config/emulators/pcsx2"; inputs.nixpkgs.follows = "nixpkgs"; };
    emulator-ppsspp = { url = "path:./config/emulators/ppsspp"; inputs.nixpkgs.follows = "nixpkgs"; };
    emulator-ryujinx = { url = "path:./config/emulators/ryujinx"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-azahar = { url = "path:./config/emulators/retroarch/cores/azahar"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-citra = { url = "path:./config/emulators/retroarch/cores/citra"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-desmume = { url = "path:./config/emulators/retroarch/cores/desmume"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-flycast = { url = "path:./config/emulators/retroarch/cores/flycast"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-gambatte = { url = "path:./config/emulators/retroarch/cores/gambatte"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-genesis_plus_gx = { url = "path:./config/emulators/retroarch/cores/genesis_plus_gx"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-mednafen_psx = { url = "path:./config/emulators/retroarch/cores/mednafen_psx"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-melonds = { url = "path:./config/emulators/retroarch/cores/melonds"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-mesen = { url = "path:./config/emulators/retroarch/cores/mesen"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-mgba = { url = "path:./config/emulators/retroarch/cores/mgba"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-mupen64plus_next = { url = "path:./config/emulators/retroarch/cores/mupen64plus_next"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-ppsspp = { url = "path:./config/emulators/retroarch/cores/ppsspp"; inputs.nixpkgs.follows = "nixpkgs"; };
    core-snes9x = { url = "path:./config/emulators/retroarch/cores/snes9x"; inputs.nixpkgs.follows = "nixpkgs"; };
  };

  outputs = inputs@{ self, nixpkgs, btrc, renderer, retroarch, esde, ... }:
    let
      lib = nixpkgs.lib;
      systems = [ "x86_64-linux" "aarch64-darwin" ];  # darwin is a development host: CLI, contracts, bezel tools
      forAllSystems = f: lib.genAttrs systems (system: f system);
      mkPkgs = system: import nixpkgs {
        inherit system;
        config.allowUnfreePredicate = pkg: lib.hasPrefix "libretro-" (lib.getName pkg);
      };
      byPrefix = prefix: lib.mapAttrs' (name: flake: lib.nameValuePair (lib.removePrefix prefix name) flake)
        (lib.filterAttrs (name: _: lib.hasPrefix prefix name) inputs);
      shared = {
        inherit self nixpkgs btrc renderer retroarch esde forAllSystems mkPkgs;
        emulatorFlakes = byPrefix "emulator-";
        coreFlakes = byPrefix "core-";
      };
    in {
      packages = import ./packaging/nix/flake/packages.nix shared;
      apps = import ./packaging/nix/flake/apps.nix shared;
      checks = import ./packaging/nix/flake/checks.nix shared;
      devShells = import ./packaging/nix/flake/dev-shells.nix shared;
    };
}
