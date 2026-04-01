{
  description = "Schemulator — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    systems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
  in {
    packages = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      isLinux = pkgs.stdenv.hostPlatform.isLinux;
      ryujinx = pkgs.callPackage ./nix/ryujinx.nix {};
      es-de = pkgs.callPackage ./nix/es-de.nix {};
    in {
      # --- Individual emulators ---
      dolphin = pkgs.dolphin-emu;
      azahar = pkgs.azahar;
      inherit ryujinx es-de;
    } // pkgs.lib.optionalAttrs isLinux {
      pcsx2 = pkgs.pcsx2;
      cemu = pkgs.cemu;
      retroarch = pkgs.retroarch-bare;
    } // {
      # --- Unified bundle ---
      default = pkgs.callPackage ./nix/schemulator.nix {
        inherit (pkgs) dolphin-emu azahar;
        pcsx2 = if isLinux then pkgs.pcsx2 else null;
        cemu = if isLinux then pkgs.cemu else null;
        retroarch-bare = if isLinux then pkgs.retroarch-bare else null;
        inherit ryujinx es-de;
      };
    });

    # `nix run` launches schemulator CLI
    apps = forAllSystems (system: {
      default = {
        type = "app";
        program = "${self.packages.${system}.default}/bin/schemulator";
      };
    });

    # NixOS module
    nixosModules.default = import ./nix/module.nix;

    # Dev shell
    devShells = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      default = pkgs.mkShell {
        buildInputs = [
          (pkgs.python3.withPackages (ps: [
            ps.pycryptodome
            ps.pytest
          ]))
        ];
      };
    });
  };
}
