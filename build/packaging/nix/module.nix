{ config, lib, pkgs, ... }:

let
  cfg = config.programs.semu;
in {
  options.programs.semu = {
    enable = lib.mkEnableOption "Semu emulation environment";

    configDir = lib.mkOption {
      type = lib.types.path;
      description = "Path to the Semu project/data directory";
      example = "/home/user/GoogleDrive/media/Games/Emulation";
    };

    sourceDir = lib.mkOption {
      type = lib.types.path;
      description = "Path to the Semu source checkout";
      example = "/home/user/GoogleDrive/dev/semu";
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

    # Build generated Semu target outputs as the specified user, not root.
    system.activationScripts.semu = ''
      if [ -d "${cfg.sourceDir}" ]; then
        echo "Semu: building Linux target from ${cfg.sourceDir} for ${cfg.configDir}"
        if [ -x "${cfg.sourceDir}/build/out/semu" ]; then
          ${pkgs.su}/bin/su - ${cfg.user} -c 'cd "${cfg.sourceDir}" && SEMU_ASSET_ROOT="${cfg.sourceDir}" SEMU_PROJECT_DIR="${cfg.configDir}" ./build/out/semu build target linux --project "${cfg.configDir}"' || true
        else
          echo "Semu: ${cfg.sourceDir}/build/out/semu missing; run make btrc-build first"
        fi
      else
        echo "Semu: source dir ${cfg.sourceDir} not found, skipping"
      fi
    '';
  };
}
