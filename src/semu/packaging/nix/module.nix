# module.nix — the NixOS system module (programs.semu): installs the chosen
# nixpkgs emulator packages (plus any flake-built extras via extraPackages),
# optionally enables Flatpak, and bootstraps a cloud-synced semu project
# directory at activation as the configured user. Exposed by the root flake
# as nixosModules.default; nothing in the CLI/bundle depends on it.
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
      default = [ ];
      description = "Additional packages to install (e.g. the flake's es-de or ryujinx)";
    };

    flatpak = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Whether to enable Flatpak support (the Linux emulator backend)";
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

    # Run the BTRC bootstrap as the configured user, never root.
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
