{ lib
, writeShellApplication
, coreutils
, emulatorName
, emulatorPackage
, semuCli
, executableName ? null
, extraArgs ? []
}:

let
  executable =
    if executableName == null
    then lib.getExe emulatorPackage
    else lib.getExe' emulatorPackage executableName;
  renderedExtraArgs = lib.escapeShellArgs extraArgs;
in
writeShellApplication {
  name = "semu-${emulatorName}";
  runtimeInputs = [ coreutils semuCli ];
  text = ''
    set -euo pipefail
    exec semu launcher routed ${lib.escapeShellArg emulatorName} ${lib.escapeShellArg executable} ${renderedExtraArgs} "$@"
  '';
}
