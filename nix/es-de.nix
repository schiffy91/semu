{ lib, stdenv, fetchurl, appimageTools ? null, undmg ? null, steamDeck ? false }:

let
  version = "3.4.0";
  sources = {
    aarch64-darwin = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/243196872/download";
      hash = "sha256-fpwMwi7vwbd0n/+Vy1JBDWGJtZ7GkUSBd9b6wbErQ6w=";
      name = "ES-DE_${version}-arm64.dmg";
    };
    # x86_64-darwin: hash not yet pinned. Rather than ship a placeholder
    # SHA-256 that would fail with a "got vs want" mismatch (and tempt users
    # to paste in whatever Nix reports), we omit the entry entirely and let
    # the `sources.${...} or throw "unsupported"` clause below surface a
    # clean error (round-5 critic finding #2). Re-enable when the hash is
    # verified against the upstream release.
    x86_64-linux = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/246875981/download";
      hash = "sha256-TLZs/JIwmXEfoP7Rnuhrl0SmKU4C4//Rnuhn93qI7H4=";
      name = "ES-DE_x64.AppImage";
    };
    x86_64-linux-steamdeck = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/246876039/download";
      hash = "sha256-/RaIA4KO3QaDPlxboj36Nmujvs58BUMpVU+8Qj5+lws=";
      name = "ES-DE_x64_SteamDeck.AppImage";
    };
  };

  sourceKey =
    if steamDeck && stdenv.hostPlatform.system == "x86_64-linux"
    then "x86_64-linux-steamdeck"
    else stdenv.hostPlatform.system;
in
if stdenv.hostPlatform.isDarwin then
  stdenv.mkDerivation {
    pname = "es-de";
    inherit version;
    src = fetchurl (sources.${stdenv.hostPlatform.system} or (throw "ES-DE: unsupported platform ${stdenv.hostPlatform.system}"));
    sourceRoot = ".";
    nativeBuildInputs = [ undmg ];
    installPhase = ''
      mkdir -p $out/Applications
      cp -r "ES-DE.app" $out/Applications/
    '';
    meta = {
      description = "EmulationStation Desktop Edition — emulator frontend";
      homepage = "https://es-de.org";
      platforms = [ "aarch64-darwin" ];  # x86_64-darwin pending verified hash
      license = lib.licenses.mit;
    };
  }
else if stdenv.hostPlatform.system == "x86_64-linux" then
  appimageTools.wrapType2 {
    pname = if steamDeck then "es-de-steamdeck" else "es-de";
    inherit version;
    src = fetchurl sources.${sourceKey};
    meta = {
      description = "EmulationStation Desktop Edition — emulator frontend"
        + lib.optionalString steamDeck " (Steam Deck optimized)";
      homepage = "https://es-de.org";
      platforms = [ "x86_64-linux" ];
      license = lib.licenses.mit;
    };
  }
else
  throw "ES-DE: unsupported platform ${stdenv.hostPlatform.system}"
