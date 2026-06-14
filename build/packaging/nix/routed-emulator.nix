{ lib
, writeShellApplication
, coreutils
, emulatorName
, emulatorPackage
, semuCli
, executableName ? null
, extraArgs ? []
, extraRuntimeInputs ? []
}:

let
  executable =
    if executableName == null
    then lib.getExe emulatorPackage
    else lib.getExe' emulatorPackage executableName;
  renderedExtraArgs = lib.escapeShellArgs extraArgs;
  retroarchCoreDir = "${emulatorPackage}/lib/retroarch/cores";
in
writeShellApplication {
  name = "semu-${emulatorName}";
  runtimeInputs = [ coreutils semuCli ] ++ extraRuntimeInputs;
  text = if emulatorName == "es-de" then ''
    set -euo pipefail
    unset CDPATH
    project="''${SEMU_PROJECT_DIR:-$HOME/.local/share/semu}"
    export SEMU_PROJECT_DIR="$project"
    export SEMU_ESDE_SETTINGS_COMMAND="''${SEMU_ESDE_SETTINGS_COMMAND:-$project/.semu/generated/bin/semu-settings}"
    if command -v nixGL >/dev/null 2>&1; then
      exec nixGL ${lib.escapeShellArg executable} ${renderedExtraArgs} "$@"
    fi
    exec ${lib.escapeShellArg executable} ${renderedExtraArgs} "$@"
  '' else ''
    set -euo pipefail
    unset CDPATH
    launcher_bin="$(cd -- "$(dirname -- "$0")" && pwd -P)"
    export SEMU_ACTIVE_LAUNCHER_BIN="''${SEMU_ACTIVE_LAUNCHER_BIN:-$launcher_bin}"
    ${lib.optionalString (emulatorName == "retroarch") ''
    export SEMU_RETROARCH_CORE_DIR="''${SEMU_RETROARCH_CORE_DIR:-${retroarchCoreDir}}"
    ''}
    export PATH="$SEMU_ACTIVE_LAUNCHER_BIN:$PATH"
    exec semu launcher routed ${lib.escapeShellArg emulatorName} ${lib.escapeShellArg executable} ${renderedExtraArgs} "$@"
  '';
}
