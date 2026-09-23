{ self, forAllSystems, mkPkgs, ... }:

forAllSystems (system:
  let
    lib = (mkPkgs system).lib;
    packages = self.packages.${system};
    app = program: { type = "app"; inherit program; };
  in {
    btrcpy = app "${packages.btrcpy}/bin/btrcpy";
    semu-cli = app "${packages.semu-cli}/bin/semu";
  } // lib.optionalAttrs (packages ? semu) {
    default = app "${packages.semu}/bin/semu";
    es-de = app "${packages.semu}/bin/semu-es-de";
  })
