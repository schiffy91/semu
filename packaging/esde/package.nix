# ES-DE is source-built on Linux so Semu's native settings entry is part of
# the executable. macOS remains an upstream app bundle until that platform's
# package is moved to the same source pipeline.
{
  lib,
  stdenv,
  fetchurl,
  undmg ? null,
  python3 ? null,
  esDePackages ? null,
  steamDeck ? false,
}:

let
  version = "3.4.0";
  sourceRevision = "4f2830048ee002fee337cd7affea3d5333f8faf5";
  sourceHash = "sha256-poegMKtPtUbdUbAwVj6O+rh7bxou+Wc+IDS3TBHh2LU=";
  settingsPatch = ./settings-menu.patch;
  settingsPatchHash = builtins.hashFile "sha256" settingsPatch;
  settingsProtocol = "semu-settings-v2";
  splashSha256 = "09496504269ee3b77111f07a773ec3204e03dcf3b26b7c846d36096a2f15f645";

  darwinSources = {
    aarch64-darwin = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/243196872/download";
      hash = "sha256-fpwMwi7vwbd0n/+Vy1JBDWGJtZ7GkUSBd9b6wbErQ6w=";
      name = "ES-DE_${version}-arm64.dmg";
    };
    x86_64-darwin = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/243196947/download";
      hash = "sha256-iDBvw/caxHCIlQ3QuoOcGUYjtxl3IqmjikYBUN4ZH2w=";
      name = "ES-DE_${version}-x64.dmg";
    };
  };

  system = stdenv.hostPlatform.system;
  unsupported = throw "ES-DE: unsupported platform ${system}";

  linuxPackage =
    assert lib.assertMsg (esDePackages != null)
      "ES-DE: the pinned source-build package set is required on Linux";
    esDePackages.emulationstation-de.overrideAttrs (finalAttrs: previousAttrs: {
      pname = if steamDeck then "es-de-steamdeck" else "es-de";
      inherit version;

      src = esDePackages.fetchzip {
        url = "https://gitlab.com/es-de/emulationstation-de/-/archive/${sourceRevision}/emulationstation-de-${sourceRevision}.tar.gz";
        hash = sourceHash;
      };

      patches = [ settingsPatch ];
      cmakeFlags = [
        (esDePackages.lib.cmakeBool "APPLICATION_UPDATER" false)
        (esDePackages.lib.cmakeBool "STEAM_DECK" steamDeck)
      ];

      postInstall = (previousAttrs.postInstall or "") + ''
        mkdir -p "$out/share/semu"
        cat > "$out/share/semu/es-de-build-contract.json" <<JSON
        {
          "schema_version": 2,
          "source_revision": "${sourceRevision}",
          "source_sha256": "${sourceHash}",
          "patch_sha256": "${settingsPatchHash}",
          "settings_protocol": "${settingsProtocol}",
          "program_data_root": "$out/share/es-de",
          "splash": {
            "logical_path": ":/graphics/splash.svg",
            "package_path": "$out/share/es-de/resources/graphics/splash.svg",
            "sha256": "${splashSha256}"
          }
        }
        JSON
      '';

      doInstallCheck = true;
      installCheckPhase = ''
        test -x "$out/bin/es-de"
        grep --binary-files=text --fixed-strings --quiet \
          "${settingsProtocol}" "$out/bin/es-de"
        grep --binary-files=text --fixed-strings --quiet \
          "semu_settings.json" "$out/bin/es-de"
        grep --fixed-strings --quiet \
          "\"source_revision\": \"${sourceRevision}\"" \
          "$out/share/semu/es-de-build-contract.json"
        splash="$out/share/es-de/resources/graphics/splash.svg"
        test -s "$splash"
        test "$(sha256sum "$splash" | cut -d ' ' -f 1)" = "${splashSha256}"
        grep --fixed-strings --quiet '#00A1B0' "$splash"
        for theme in slate-es-de linear-es-de modern-es-de; do
          test -s "$out/share/es-de/themes/$theme/theme.xml"
        done
        grep --fixed-strings --quiet \
          'mBoolMap["SplashScreen"] = {true, true};' \
          es-core/src/Settings.cpp
      '';

      passthru = (previousAttrs.passthru or { }) // {
        semuSettings = {
          protocol = settingsProtocol;
          patch = settingsPatch;
          patchHash = settingsPatchHash;
          inherit sourceRevision sourceHash splashSha256;
        };
      };

      meta = previousAttrs.meta // {
        description = "ES-DE 3.4.0 with Semu's native settings menu";
        mainProgram = "es-de";
      };
    });
in
if stdenv.hostPlatform.isDarwin then
  stdenv.mkDerivation {
    pname = "es-de";
    inherit version;
    src = fetchurl (darwinSources.${system} or unsupported);
    sourceRoot = ".";
    nativeBuildInputs = [ undmg python3 ];
    installPhase = ''
      python3 - <<'PLIST'
      import plistlib
      path = "ES-DE.app/Contents/Info.plist"
      with open(path, "rb") as handle:
          info = plistlib.load(handle)
      usage = "ES-DE scans for Bluetooth game controllers."
      info.setdefault("NSBluetoothAlwaysUsageDescription", usage)
      info.setdefault("NSBluetoothPeripheralUsageDescription", usage)
      with open(path, "wb") as handle:
          plistlib.dump(info, handle)
      PLIST
      mkdir -p "$out/Applications"
      cp -r "ES-DE.app" "$out/Applications/"
    '';
    meta = {
      description = "EmulationStation Desktop Edition emulator frontend";
      homepage = "https://es-de.org";
      platforms = [ "aarch64-darwin" "x86_64-darwin" ];
      license = lib.licenses.mit;
    };
  }
else if system == "x86_64-linux" then
  linuxPackage
else
  unsupported
