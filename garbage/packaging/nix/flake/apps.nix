{ self, nixpkgs, btrc, systems, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
      pkgs = mkPkgs system;
      isX86Linux = system == "x86_64-linux";
      emulatorApp = name: {
        type = "app";
        program = "${self.packages.${system}.${name}}/bin/${name}";
      };
    in {
      default = {
        type = "app";
        program = "${self.packages.${system}.default}/bin/semu";
      };
    } // pkgs.lib.optionalAttrs isX86Linux {
      semu-retroarch = emulatorApp "semu-retroarch";
      semu-dolphin = emulatorApp "semu-dolphin";
      semu-ppsspp = emulatorApp "semu-ppsspp";
      semu-flycast = emulatorApp "semu-flycast";
      semu-melonds = emulatorApp "semu-melonds";
      semu-pcsx2 = emulatorApp "semu-pcsx2";
      semu-cemu = emulatorApp "semu-cemu";
      semu-azahar = emulatorApp "semu-azahar";
      semu-ryujinx = emulatorApp "semu-ryujinx";
      semu-es-de = emulatorApp "semu-es-de";
    })
