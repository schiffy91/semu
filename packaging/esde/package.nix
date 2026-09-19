# ES-DE built from source on the pinned nixpkgs that still carries FreeImage.
{ lib, stdenv, esDePackages }:

let
  version = "3.4.0";
  sourceRevision = "4f2830048ee002fee337cd7affea3d5333f8faf5";
  sourceHash = "sha256-poegMKtPtUbdUbAwVj6O+rh7bxou+Wc+IDS3TBHh2LU=";
in
assert lib.assertMsg (stdenv.hostPlatform.system == "x86_64-linux") "ES-DE: only x86_64-linux is packaged";
esDePackages.emulationstation-de.overrideAttrs (previous: {
  pname = "es-de";
  inherit version;

  src = esDePackages.fetchzip {
    url = "https://gitlab.com/es-de/emulationstation-de/-/archive/${sourceRevision}/emulationstation-de-${sourceRevision}.tar.gz";
    hash = sourceHash;
  };

  patches = (previous.patches or [ ]) ++ [ ./settings-menu.patch ];
  allowSubstitutes = false;  # never a cache binary  # SEMU SETTINGS entry in the main menu

  cmakeFlags = (previous.cmakeFlags or [ ]) ++ [ (esDePackages.lib.cmakeBool "APPLICATION_UPDATER" false) ];

  doInstallCheck = true;
  installCheckPhase = ''
    test -x "$out/bin/es-de"
    grep -Fq 'semu-settings-v2' "$out/bin/es-de"
    for theme in slate-es-de linear-es-de modern-es-de; do
      test -s "$out/share/es-de/themes/$theme/theme.xml"
    done
  '';

  meta = previous.meta // {
    description = "ES-DE ${version} emulator frontend";
    mainProgram = "es-de";
  };
})
