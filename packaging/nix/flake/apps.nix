# flake/apps.nix — runnable entry points: the semu CLI, the btrcpy transpiler
# (make btrc-build runs `nix run .#btrcpy`), and bake-bezels (regenerate the
# committed bezel art from the bezels.json recipes).
{ self, mkPkgs, forAllSystems, ... }:

forAllSystems (system: let
  pkgs = mkPkgs system;

  # `nix run .#bake-bezels` — build the regenerator (fetches Duimon/soqueroeu
  # upstreams + imagemagick-renders every bezels.json recipe), then overwrite
  # the committed config/assets/bezels/ tree with its output so the in-tree
  # PNGs stay byte-identical to what the recipes produce. Idempotent: a second
  # run leaves the tree unchanged.
  bakeBezels = pkgs.writeShellApplication {
    name = "bake-bezels";
    runtimeInputs = [ pkgs.git pkgs.coreutils ];
    text = ''
      repo="$(git rev-parse --show-toplevel)"
      generated="${self.packages.${system}.semu-bezels-generate}/share/semu/assets/bezels"
      dest="$repo/config/assets/bezels"

      if [ ! -d "$generated" ]; then
        echo "bake-bezels: regenerator produced no bezels tree at $generated" >&2
        exit 1
      fi

      rm -rf "$dest"
      mkdir -p "$dest"
      cp -RL "$generated/." "$dest/"
      chmod -R u+w "$dest"

      echo "bake-bezels: regenerated $dest from the bezels.json recipes"
    '';
  };
in {
  default = {
    type = "app";
    program = "${self.packages.${system}.default}/bin/semu";
  };
  semu-cli = {
    type = "app";
    program = "${self.packages.${system}.semu-cli}/bin/semu";
  };
  btrcpy = {
    type = "app";
    program = "${self.packages.${system}.btrcpy}/bin/btrcpy";
  };
  bake-bezels = {
    type = "app";
    program = "${bakeBezels}/bin/bake-bezels";
  };
})
