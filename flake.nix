{
  description = "Schemulator — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    systems = [ "aarch64-darwin" "x86_64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
  in {
    packages = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      isLinux = pkgs.stdenv.hostPlatform.isLinux;
    in {
      # --- Cross-platform emulators ---
      dolphin = pkgs.dolphin-emu;
      azahar = pkgs.azahar;

      # --- Linux-only emulators ---
    } // pkgs.lib.optionalAttrs isLinux {
      pcsx2 = pkgs.pcsx2;
      cemu = pkgs.cemu;
      retroarch = pkgs.retroarch-bare;
    });

    # Dev shell for working on schemulator itself
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
