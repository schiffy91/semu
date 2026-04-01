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

    emulators = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ "dolphin" "azahar" "pcsx2" "cemu" "retroarch" "ryujinx" "es-de" ];
      description = "Which emulators to install";
    };

    flatpak = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Whether to enable Flatpak support (for Steam Deck compatibility)";
    };
  };

  config = lib.mkIf cfg.enable {
    # Install emulator packages
    environment.systemPackages = let
      emulatorPackages = {
        dolphin = pkgs.dolphin-emu;
        azahar = pkgs.azahar;
        pcsx2 = pkgs.pcsx2;
        cemu = pkgs.cemu;
        retroarch = pkgs.retroarch-bare;
        # ryujinx and es-de would need to be overlaid from the flake
      };
    in
      lib.filter (x: x != null)
        (map (name: emulatorPackages.${name} or null) cfg.emulators)
      ++ [ (pkgs.python3.withPackages (ps: [ ps.pycryptodome ])) ];

    # Enable Flatpak if requested (useful for Steam Deck)
    services.flatpak.enable = lib.mkIf cfg.flatpak true;

    # Symlink setup as a system activation script
    system.activationScripts.schemulator = lib.mkIf (builtins.pathExists cfg.configDir) ''
      echo "Schemulator: setting up symlinks from ${cfg.configDir}"
      cd "${cfg.configDir}" && ${pkgs.python3}/bin/python setup.py symlink || true
    '';
  };
}
