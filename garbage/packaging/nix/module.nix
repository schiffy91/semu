{ config, lib, pkgs, ... }:

let
  cfg = config.programs.semu;
in {
  options.programs.semu = {
    enable = lib.mkEnableOption "Semu emulation environment";

    configDir = lib.mkOption {
      type = lib.types.path;
      description = "Path to the semu config directory (cloud-synced)";
      example = "/home/user/GoogleDrive/media/Games/Emulation";
    };

    user = lib.mkOption {
      type = lib.types.str;
      description = "User to run semu setup as";
      example = "alex";
    };

    emulators = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ "dolphin" "azahar" "pcsx2" "cemu" "retroarch" ];
      description = "Which emulators to install (only nixpkgs-available ones)";
    };

    extraPackages = lib.mkOption {
      type = lib.types.listOf lib.types.package;
      default = [];
      description = "Additional packages to install (e.g., custom-packaged ryujinx, es-de from the flake overlay)";
    };

    flatpak = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Whether to enable Flatpak support";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = let
      emulatorPackages = {
        dolphin = pkgs.dolphin-emu;
        azahar = pkgs.azahar;
        pcsx2 = pkgs.pcsx2;
        cemu = pkgs.cemu;
        retroarch = pkgs.retroarch-bare;
      };
    in
      lib.filter (x: x != null)
        (map (name: emulatorPackages.${name} or null) cfg.emulators)
      ++ cfg.extraPackages;

    services.flatpak.enable = lib.mkIf cfg.flatpak true;

    # Run BTRC bootstrap as the specified user, not root.
    system.activationScripts.semu = ''
      if [ -d "${cfg.configDir}" ]; then
        echo "Semu: bootstrapping from ${cfg.configDir}"
        if [ -x "${cfg.configDir}/src/generated/build/semu" ]; then
          ${pkgs.su}/bin/su - ${cfg.user} -c 'cd "${cfg.configDir}" && ./src/generated/build/semu bootstrap --project "$PWD"' || true
        else
          echo "Semu: ${cfg.configDir}/src/generated/build/semu missing; run make btrc-build first"
        fi
      else
        echo "Semu: config dir ${cfg.configDir} not found, skipping"
      fi
    '';
  };
}
