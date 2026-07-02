# RetroArch for macOS — pre-built universal DMG + pre-built cores from buildbot.
# nixpkgs retroarch is marked broken on Darwin, so we package the official builds.
# Cores are baked in so no manual downloading is needed.
{ lib, stdenv, fetchurl, undmg, unzip }:

let
  version = "1.22.2";

  arch = if stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";
  coreBaseUrl = "https://buildbot.libretro.com/nightly/apple/osx/${arch}/latest";

  cores = {
    gambatte         = { hash = "sha256-8OzixaD1BU3/mCpqKkIu52gmprermOzvMwrNWprpACk="; desc = "GB/GBC"; };
    mgba             = { hash = "sha256-Nj4ycSzqpV2G1YtvK95Zj6ujflVXJ0AEMoCEb+KrE5Y="; desc = "GBA"; };
    genesis_plus_gx  = { hash = "sha256-NeuJmCpcH1gVvc0Vq46WK9Yig3O3g1GHWNZjIgVRvV0="; desc = "Genesis"; };
    snes9x           = { hash = "sha256-ZykQuEdE6Hw39EAhl1LKEOVEAhuefYSaxpHgp3ocJT4="; desc = "SNES"; };
    mesen            = { hash = "sha256-sR0XhlTwYyLUXdSKoqPIalZgtkZyqdU6Jke6DEx9C9w="; desc = "NES"; };
    mupen64plus_next = { hash = "sha256-t5WaLXckPsBclONQObYsp1MY80OLluYFf4vsaccXL48="; desc = "N64"; };
    desmume          = { hash = "sha256-Opebjz3KsOikDbKwTAI/Ol4QCqJirfvzzs7kRCCDqxY="; desc = "NDS"; };
    swanstation      = { hash = "sha256-JRgvm4hzm6CV84oFZiUMdZ4aThPS2MbKeckzOqFtao4="; desc = "PSX"; };
    mednafen_psx     = { hash = "sha256-8XVpJFpO0B5A4M41ut45Qy8cqukQr1r7tEk5UCa8hZo="; desc = "PSX (alt)"; };
    mednafen_psx_hw  = { hash = "sha256-LH+s08JnP9IsXFFiU4Rxd8K0RwofG4L+tpqc1O5s1JM="; desc = "PSX (HW)"; };
    ppsspp           = { hash = "sha256-OCVTkUFwqtzd5Eizur96PIWmoCeEKv/iBBS94w/a8+Q="; desc = "PSP"; };
    flycast          = { hash = "sha256-eI1d/EECOKpu2k0Gp21fESDC4X5J4t8VSE2ye4ATmZ4="; desc = "Dreamcast"; };
  };

  coreSrcs = lib.mapAttrsToList (name: info: fetchurl {
    url = "${coreBaseUrl}/${name}_libretro.dylib.zip";
    inherit (info) hash;
    name = "${name}_libretro.dylib.zip";
  }) cores;
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

    # Install pre-built cores
    ${lib.concatMapStringsSep "\n" (src: "unzip -o ${src} -d $out/cores/") coreSrcs}

    # Wrapper: passes --config (user config with video_driver=gl) and
    # sets libretro_directory via --appendconfig so RetroArch finds our cores.
    # We write the cores path to a temp config that gets appended.
    echo 'libretro_directory = "$out/cores"' > $out/cores/path.cfg

    cat > $out/bin/retroarch <<'WRAPPER'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="$HOME/Library/Application Support/RetroArch/retroarch.cfg"
CORES_CFG="$SCRIPT_DIR/cores/path.cfg"
exec "$SCRIPT_DIR/Applications/RetroArch.app/Contents/MacOS/RetroArch" \
  --config="$CONFIG" \
  --appendconfig="$CORES_CFG" \
  "$@"
WRAPPER
    chmod +x $out/bin/retroarch

    # Write the actual cores path (nix store path, not variable)
    echo "libretro_directory = \"$out/cores\"" > $out/cores/path.cfg
  '';
  meta = {
    description = "RetroArch with ${toString (lib.length (lib.attrNames cores))} cores (${lib.concatStringsSep ", " (lib.mapAttrsToList (_: i: i.desc) cores)})";
    homepage = "https://www.retroarch.com";
    platforms = [ "aarch64-darwin" "x86_64-darwin" ];
    license = lib.licenses.gpl3;
  };
}
