# flake/apps.nix — runnable entry points: the semu CLI and the btrcpy
# transpiler (make btrc-build runs `nix run .#btrcpy`).
{ self, forAllSystems, ... }:

forAllSystems (system: {
  default = {
    type = "app";
    program = "${self.packages.${system}.default}/bin/semu";
  };
  btrcpy = {
    type = "app";
    program = "${self.packages.${system}.btrcpy}/bin/btrcpy";
  };
})
