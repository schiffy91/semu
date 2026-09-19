{ self, forAllSystems, ... }:

forAllSystems (system: {
  default = {
    type = "app";
    program = "${self.packages.${system}.semu}/bin/semu";
  };
  es-de = {
    type = "app";
    program = "${self.packages.${system}.semu}/bin/semu-es-de";
  };
  btrcpy = {
    type = "app";
    program = "${self.packages.${system}.btrcpy}/bin/btrcpy";
  };
})
