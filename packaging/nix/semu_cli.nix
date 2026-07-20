# semu_cli.nix - combine the independently built program and runtime source.
{ lib, symlinkJoin, makeWrapper
, semuProgram
, semuSource
, syncthing ? null
, syncthingtray ? null
, curl ? null
, bubblewrap ? null
}:

let
  runtimeTools = lib.filter (tool: tool != null) [
    syncthing
    syncthingtray
    curl
    bubblewrap
  ];
  runtimePath = lib.makeBinPath runtimeTools;
in
symlinkJoin {
  name = "semu-cli";
  paths = [ semuProgram semuSource ];
  nativeBuildInputs = [ makeWrapper ];

  postBuild = ''
    mkdir -p "$out/bin"
    makeWrapper "$out/lib/semu/semu-btrc" "$out/bin/semu" \
      --set SEMU_ASSET_ROOT "$out" \
      --set SEMU_BIN "$out/bin/semu" \
      --set SEMU_SOURCE_ROOT "$out/share/semu/config" \
      --prefix PATH : ${lib.escapeShellArg runtimePath}
  '';

  meta = {
    description = "Semu BTRC runtime CLI";
    license = lib.licenses.mit;
  };
}
