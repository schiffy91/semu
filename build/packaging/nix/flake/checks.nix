{ self, nixpkgs, btrc, systems, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
      pkgs = mkPkgs system;
      semuCli = pkgs.callPackage ../semu-cli.nix {
        syncthing = null;
        syncthingtray = null;
        curl = null;
        bubblewrap = null;
      };
    in {
      compiler-verify = pkgs.runCommand "semu-compiler-verify-check" {
        nativeBuildInputs = [ semuCli ];
      } ''
        export HOME="$TMPDIR/home"
        project="$TMPDIR/project"
        mkdir -p "$HOME" "$project"
        semu verify target steam-deck --project "$project"
        touch "$out"
      '';

      compiler-build = pkgs.runCommand "semu-compiler-build-check" {
        nativeBuildInputs = [ semuCli pkgs.gnugrep ];
      } ''
        export HOME="$TMPDIR/home"
        project="$TMPDIR/project"
        mkdir -p "$HOME" "$project"
        semu build target steam-deck --project "$project"

        test -f "$project/.semu/generated/packaging/appimage/manifest.json"
        grep -F '"path": "usr/bin/semu-retroarch"' "$project/.semu/generated/packaging/appimage/manifest.json"
        test -x "$project/.semu/generated/bin/semu-retroarch"
        test -f "$project/ES-DE/custom_systems/es_systems.xml"
        touch "$out"
      '';
    })
