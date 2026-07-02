# semu_emulators.nix — emulator inventory derived from the JSON contracts.
#
# WHICH emulators need a Nix package comes from
# src/semu/emulators/<id>/emulator.json: a macos platform with backend
# "nix"/"native" needs a mac recipe below; linux platforms declare flatpak
# ids, which are runtime concerns and never packaged here. WHICH libretro
# cores RetroArch carries comes from src/semu/systems/*/system.json emulator
# entries. Only source pins/hashes for the concrete packages live in this
# file — the inventory itself is contract-driven, and a contract entry with
# no recipe fails evaluation loudly.
{ lib, stdenv, fetchurl, undmg, unzip, symlinkJoin, libretro, ares }:

let
  emulatorsDir = ../../emulators;
  systemsDir = ../../systems;

  contractIds = dir: fileName: lib.attrNames (lib.filterAttrs
    (name: type: type == "directory"
      && builtins.pathExists (dir + "/${name}/${fileName}"))
    (builtins.readDir dir));

  emulatorContracts = lib.genAttrs (contractIds emulatorsDir "emulator.json")
    (id: lib.importJSON (emulatorsDir + "/${id}/emulator.json"));

  macIds = lib.attrNames (lib.filterAttrs
    (_: contract: contract ? platforms.macos
      && lib.elem (contract.platforms.macos.backend or "") [ "nix" "native" ])
    emulatorContracts);

  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json"))
    (contractIds systemsDir "system.json");

  # Every libretro core any system routes to RetroArch on the platform
  # (entries without a "platforms" filter apply everywhere).
  coresFor = platform: lib.unique (lib.concatMap
    (system: map (entry: entry.core) (lib.filter
      (entry: (entry.emulator or "") == "retroarch" && entry ? core
        && lib.elem platform (entry.platforms or [ platform ]))
      (system.emulators or [ ])))
    systemContracts);

  # --- macOS RetroArch: official universal DMG + buildbot cores -------------
  # nixpkgs retroarch is broken on Darwin. The core zips come from the
  # buildbot's rolling "latest" nightly directory, so these hashes go stale
  # when the buildbot rotates; re-prefetch on mismatch (2026-07-01 pins).
  retroarchMac =
    let
      version = "1.22.2";
      arch = if stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";
      coreHashes = {
        arm64 = {
          gambatte = "sha256-i1LZ58cF3blG0vqwK3RkkBc3REU5tTEh5rMaQJonte4=";
          mgba = "sha256-p/YKT5dcZhoXtP4qEkD87/FIBbsaXPjV+lWj8KoIocY=";
          genesis_plus_gx = "sha256-tbkfsiJNh8zN+VTUHmXgF/iAFqP7JGg96XIVMewCcSo=";
          snes9x = "sha256-HF7Q+lblEHy2qGGlrvc62YCL0VF8miAouno/31sziuU=";
          mesen = "sha256-suOxgCRBFdduyHR1rxD2pgy96G2Ye2qf6shlc2Sj1nM=";
          desmume = "sha256-ixjCY8kaFjDOuoi+hGggvYtrjYhz5UDyXULIpgLQcs8=";
          ppsspp = "sha256-2EehMYawCTxEdI5nJIMrlq4ZZLUoKb+oLCDcIpFnVmM=";
          flycast = "sha256-HG22yjl4TSLXm9dHUBJEfzjsqKM7DWQg34QeUXdNsWo=";
          mednafen_psx_hw = "sha256-ev03pEiVlK8Yc2FKbRC88Ge44co/FtkBQM54huqzDng=";
        };
        x86_64 = {
          gambatte = "sha256-9gXMqq+3rai3KeTAfPyuf8upKUH7a/hbfeInK/3mWRM=";
          mgba = "sha256-dGZGqhXLTn2FEsdHJzBeKm2cPYeygVbje3aJ5SyNm5A=";
          genesis_plus_gx = "sha256-3jDrz1lobNmsoieklRpH3a1Y1Fqt8MNSh/GSCK1IjXo=";
          snes9x = "sha256-OkNdZfnh2V/RIvbonXPqd0jeyoYkVvXWaTDutwIc/Hw=";
          mesen = "sha256-2Wf8AZ1pPP/eMSAGITuEDq8z7n7n6PS1ZfZ1xNMONSA=";
          desmume = "sha256-r2L3tLvVPxN312fw7kgCQhPP4XCS0b4M/vGt1mWfCrE=";
          ppsspp = "sha256-Ly9O4Zp0sQtWNU30hDwW/tCVWM0+imbrubBND1KTT00=";
          flycast = "sha256-oMml323FE7KDmw8JpdUJCgo/r4xnpKl9FLJ6q0R6I8M=";
          mednafen_psx_hw = "sha256-f5oG3PiKA7Qct/oa/wtR5+nlmSOURNCllul9lI1i5qs=";
        };
      };
      coreZip = core: fetchurl {
        url = "https://buildbot.libretro.com/nightly/apple/osx/${arch}/latest/${core}_libretro.dylib.zip";
        name = "${core}_libretro.dylib.zip";
        hash = coreHashes.${arch}.${core} or (throw
          "semu_emulators.nix: no buildbot hash for core '${core}' (${arch}); prefetch and add it");
      };
      cores = coresFor "macos";
    in
    stdenv.mkDerivation {
      pname = "retroarch";
      inherit version;
      src = fetchurl {
        url = "https://buildbot.libretro.com/stable/${version}/apple/osx/universal/RetroArch_Metal.dmg";
        hash = "sha256-gbeRIbom1TkGSuE7TQQZoSDD0WWvvmVs9fVBKxX9tDQ=";
      };
      sourceRoot = ".";
      nativeBuildInputs = [ undmg unzip ];
      installPhase = ''
        mkdir -p $out/Applications $out/cores $out/bin
        cp -r RetroArch.app $out/Applications/

        ${lib.concatMapStringsSep "\n"
          (core: "unzip -o ${coreZip core} -d $out/cores/") cores}

        # --appendconfig points RetroArch at the bundled cores while the user
        # config stays the writable one under ~/Library.
        echo "libretro_directory = \"$out/cores\"" > $out/cores/path.cfg
        cat > $out/bin/retroarch <<'WRAPPER'
        #!/bin/bash
        SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
        CONFIG="$HOME/Library/Application Support/RetroArch/retroarch.cfg"
        exec "$SCRIPT_DIR/Applications/RetroArch.app/Contents/MacOS/RetroArch" \
          --config="$CONFIG" \
          --appendconfig="$SCRIPT_DIR/cores/path.cfg" \
          "$@"
        WRAPPER
        chmod +x $out/bin/retroarch
      '';
      meta = {
        description = "RetroArch with the ${toString (lib.length cores)} cores the system contracts route on macOS";
        homepage = "https://www.retroarch.com";
        platforms = lib.platforms.darwin;
        license = lib.licenses.gpl3;
      };
    };

  # --- macOS standalone emulators (official prebuilt releases) --------------
  dolphinMac = stdenv.mkDerivation rec {
    pname = "dolphin-emu";
    version = "2506a";
    src = fetchurl {
      url = "https://dl.dolphin-emu.org/releases/${version}/dolphin-${version}-universal.dmg";
      hash = "sha256-DqV+rNgKtRy/F6DNYwm1lzFVzEbA+pD16Mb7UO6WZ8w=";
    };
    sourceRoot = ".";
    nativeBuildInputs = [ undmg ];
    installPhase = ''
      mkdir -p $out/Applications
      cp -r Dolphin.app $out/Applications/
    '';
    meta = {
      description = "GameCube/Wii emulator";
      homepage = "https://dolphin-emu.org";
      platforms = lib.platforms.darwin;
      license = lib.licenses.gpl2Plus;
    };
  };

  # nixpkgs azahar (2124.3) crashes on macOS; the official release works.
  azaharMac = stdenv.mkDerivation rec {
    pname = "azahar";
    version = "2125.0.1";
    src = fetchurl {
      url = "https://github.com/azahar-emu/azahar/releases/download/${version}/azahar-macos-universal-${version}.zip";
      hash = "sha256-b7lEWXztuGW+XdCsbSF7TnE3wnAGZoJY2+OoMpeuJ68=";
    };
    sourceRoot = "azahar-macos-universal-${version}";
    nativeBuildInputs = [ unzip ];
    installPhase = ''
      mkdir -p $out/Applications $out/bin
      cp -r Azahar.app $out/Applications/azahar.app
      ln -s $out/Applications/azahar.app/Contents/MacOS/azahar $out/bin/azahar
    '';
    meta = {
      description = "Nintendo 3DS emulator";
      homepage = "https://azahar-emu.org";
      platforms = lib.platforms.darwin;
      license = lib.licenses.gpl2Plus;
    };
  };

  pcsx2Mac = stdenv.mkDerivation rec {
    pname = "pcsx2";
    version = "2.6.3";
    src = fetchurl {
      url = "https://github.com/PCSX2/pcsx2/releases/download/v${version}/pcsx2-v${version}-macos-Qt.tar.xz";
      hash = "sha256-y3ueYzDxq/DPkslAZffrmD0PqK/8/msMy5wqTr8Gfxo=";
    };
    sourceRoot = ".";
    installPhase = ''
      mkdir -p $out/Applications
      cp -r PCSX2-v${version}.app $out/Applications/PCSX2.app
    '';
    meta = {
      description = "PlayStation 2 emulator";
      homepage = "https://pcsx2.net";
      platforms = lib.platforms.darwin;
      license = lib.licenses.gpl3;
    };
  };

  # x64 build; runs under Rosetta on Apple silicon.
  cemuMac = stdenv.mkDerivation rec {
    pname = "cemu";
    version = "2.6";
    src = fetchurl {
      url = "https://github.com/cemu-project/Cemu/releases/download/v${version}/cemu-${version}-macos-12-x64.dmg";
      hash = "sha256-aYxLKY+UmD5NbDDpaHuo/wUJTdOTCDfFEEzdwLCknk4=";
    };
    sourceRoot = ".";
    nativeBuildInputs = [ undmg ];
    installPhase = ''
      mkdir -p $out/Applications
      cp -r Cemu.app $out/Applications/
    '';
    meta = {
      description = "Wii U emulator";
      homepage = "https://cemu.info";
      platforms = lib.platforms.darwin;
      license = lib.licenses.mpl20;
    };
  };

  # .NET needs a writable app dir; the wrapper extracts the stored tarball to
  # ~/.local/share/ryujinx-app on first run (the path emulator.json launches).
  ryujinxMac = stdenv.mkDerivation rec {
    pname = "ryujinx";
    version = "1.3.3";
    src = fetchurl {
      url = "https://git.ryujinx.app/api/v4/projects/1/packages/generic/Ryubing/${version}/ryujinx-${version}-macos_universal.app.tar.gz";
      hash = "sha256-5IGLuEyY4NMSBpGCHpB3IJnkYQEnPT8UX/2xDu4sDbs=";
    };
    dontUnpack = true;
    installPhase = ''
      mkdir -p $out/share/ryujinx $out/bin
      cp $src $out/share/ryujinx/ryujinx.tar.gz
      cat > $out/bin/ryujinx <<WRAPPER
      #!/bin/bash
      APP_DIR="\$HOME/.local/share/ryujinx-app"
      if [ ! -d "\$APP_DIR/Ryujinx.app" ]; then
        echo "First run: extracting Ryujinx..."
        mkdir -p "\$APP_DIR"
        tar xzf "$out/share/ryujinx/ryujinx.tar.gz" -C "\$APP_DIR"
        xattr -dr com.apple.quarantine "\$APP_DIR/Ryujinx.app" 2>/dev/null
      fi
      open "\$APP_DIR/Ryujinx.app" --args "\$@"
      WRAPPER
      chmod +x $out/bin/ryujinx
    '';
    meta = {
      description = "Nintendo Switch emulator (Ryubing fork)";
      homepage = "https://git.ryujinx.app/ryubing/ryujinx";
      platforms = lib.platforms.darwin;
      license = lib.licenses.mit;
      # The pinned artifact URL 404s (post-takedown registry reshuffle) and the
      # GitHub mirror is legally blocked (HTTP 451). Re-pin against a trusted
      # mirror before shipping Switch on macOS; the hash must keep matching.
      broken = true;
    };
  };

  macRecipes = {
    retroarch = retroarchMac;
    dolphin = dolphinMac;
    azahar = azaharMac;
    pcsx2 = pcsx2Mac;
    cemu = cemuMac;
    ryujinx = ryujinxMac;
    ares = ares; # nixpkgs build ships Applications/ares.app + bin/ares
  };

  # --- Linux: flatpak emulators, but the Deck flatpak RetroArch loads the
  # contract cores from ${nix_result}/lib/retroarch/cores -------------------
  linuxCoreAttr = {
    gambatte = "gambatte";
    mgba = "mgba";
    genesis_plus_gx = "genesis-plus-gx";
    snes9x = "snes9x";
    mesen = "mesen";
    mupen64plus_next = "mupen64plus";
    desmume = "desmume";
    mednafen_psx = "beetle-psx";
    mednafen_psx_hw = "beetle-psx-hw";
    ppsspp = "ppsspp";
    flycast = "flycast";
  };
  linuxCore = core: libretro.${linuxCoreAttr.${core} or (throw
    "semu_emulators.nix: no nixpkgs libretro mapping for core '${core}'")};
  linuxCores = coresFor "linux";
in
{
  # id -> package for every emulator the macOS contracts require; {} on Linux.
  inventory = lib.optionalAttrs stdenv.hostPlatform.isDarwin
    (lib.genAttrs macIds (id: macRecipes.${id} or (throw
      "semu_emulators.nix: no mac package recipe for emulator '${id}' (src/semu/emulators/${id}/emulator.json)")));

  retroarchCores =
    if stdenv.hostPlatform.isLinux && linuxCores != [ ] then
      symlinkJoin {
        name = "semu-retroarch-cores";
        paths = map linuxCore linuxCores;
      }
    else null;
}
