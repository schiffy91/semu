{ config, lib, pkgs, ... }:

let
  cfg = config.programs.schemulator;
in {
  options.programs.schemulator = {
    enable = lib.mkEnableOption "Schemulator emulation environment";

    configDir = lib.mkOption {
      type = lib.types.path;
      description = "Path to the schemulator config directory (cloud-synced)";
      example = "/home/user/GoogleDrive/media/Games/Emulation";
    };

    user = lib.mkOption {
      type = lib.types.str;
      description = "User to run schemulator setup as";
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
      ++ cfg.extraPackages
      ++ [ (pkgs.python3.withPackages (ps: [ ps.pycryptodome ])) ];

    services.flatpak.enable = lib.mkIf cfg.flatpak true;

    # Run symlink setup as the specified user, not root
    system.activationScripts.schemulator = ''
      if [ -d "${cfg.configDir}" ]; then
        echo "Schemulator: setting up symlinks from ${cfg.configDir}"
        ${pkgs.su}/bin/su - ${cfg.user} -c 'cd "${cfg.configDir}" && ${pkgs.python3}/bin/python setup.py symlink' || true
      else
        echo "Schemulator: config dir ${cfg.configDir} not found, skipping"
      fi
    '';
  };
}
