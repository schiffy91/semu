# ES-DE built from source on the pinned nixpkgs that still carries FreeImage.
{ lib, stdenv, esDePackages, source }:

let
  version = "3.4.1";
in
assert lib.assertMsg (stdenv.hostPlatform.system == "x86_64-linux") "ES-DE: only x86_64-linux is packaged";
esDePackages.emulationstation-de.overrideAttrs (previous: {
  pname = "es-de";
  inherit version;

  src = source;  # the flake input pins the GitLab revision

  patches = [ ./settings-menu.patch ];  # SEMU SETTINGS entry; nixpkgs' core find-rules patch is dropped, Semu writes its own find rules
  allowSubstitutes = false;  # never a cache binary

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
