{ lib
, writeShellApplication
, coreutils
, emulatorName
, emulatorPackage
, schemulatorCli
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
  name = "schem-${emulatorName}";
  runtimeInputs = [ coreutils schemulatorCli ];
  text = ''
    set -euo pipefail
    exec schemulator launcher routed ${lib.escapeShellArg emulatorName} ${lib.escapeShellArg executable} ${renderedExtraArgs} "$@"
  '';
}
