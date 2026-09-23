# The Deck's RetroArch autoconfig profiles (Steam Virtual Gamepad under both SDL names),
# written from the manifest retroarch/package.json names in runtime_extras.input_autoconfig.
# The bundle lists this before retroarch-joypad-autoconfig, so these files win in
# $SEMU_ASSET_ROOT/share/libretro/autoconfig, the joypad_autoconfig_dir the profile sets.
{ lib, runCommand, writeText, repositoryRoot }:

let
  packageDirectory = repositoryRoot + "/config/emulators/retroarch";
  declaration = (lib.importJSON (packageDirectory + "/package.json")).runtime_extras.input_autoconfig;
  manifest = lib.importJSON (packageDirectory + "/${declaration.manifest}");
  destination = "$out/${declaration.destination}";
  install = profile:
    let body = writeText "retroarch-autoconfig.cfg" (lib.concatStringsSep "\n" profile.lines + "\n"); in
    ''install -Dm644 ${body} ${destination}/${lib.escapeShellArg profile.file}
    '';
in
runCommand "semu-retroarch-autoconfig" { } ''
  ${lib.concatMapStrings install manifest.profiles}
  test "$(ls ${destination} | wc -l)" -eq ${toString (lib.length manifest.profiles)}
''
