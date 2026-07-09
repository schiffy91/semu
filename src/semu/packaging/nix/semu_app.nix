# semu_app.nix — the composed bundle `make setup` links at
# src/generated/nix/result: the CLI, every contract-required emulator
# package, ES-DE, the bezels.json/shaders.json visual assets, and the runtime tools.
#
# lib/semu is materialized (symlinkJoin would link it wholesale to the CLI
# store path) so the staged asset trees land under it, and bin/semu is
# re-wrapped with SEMU_ASSET_ROOT pointing there — that is the root the
# runtime resolves share/semu (bezels) against.
{ lib, symlinkJoin, makeWrapper
, semuCli
, emulatorPackages ? [ ]
, esDe ? null
, semuBezels ? null
, semuShaders ? null
, runtimeTools ? [ ]
}:

let
  runtimePath = lib.makeBinPath runtimeTools;
in
symlinkJoin {
  name = "semu-app";
  paths = [ semuCli ] ++ emulatorPackages
    ++ lib.filter (x: x != null) [ esDe semuShaders ]
    ++ runtimeTools;
  nativeBuildInputs = [ makeWrapper ];
  postBuild = lib.optionalString (semuBezels != null) ''
    if [ -L "$out/lib/semu" ]; then
      cliLib="$(readlink "$out/lib/semu")"
      rm "$out/lib/semu"
      mkdir -p "$out/lib/semu"
      cp -RL "$cliLib/." "$out/lib/semu/"
    fi
    chmod -R u+w "$out/lib/semu"

    mkdir -p "$out/lib/semu/share"
    for staged in ${semuBezels}/share/*; do
      name="$(basename "$staged")"
      rm -rf "$out/lib/semu/share/$name"
      cp -RL "$staged" "$out/lib/semu/share/$name"
    done
    if [ -d "${semuBezels}/lib/semu" ]; then
      cp -RL "${semuBezels}/lib/semu/." "$out/lib/semu/"
    fi
    # shader tree stays at the bundle root (share/libretro/shaders); keep it
    # reachable from the asset root too.
    if [ -e "$out/share/libretro" ] && [ ! -e "$out/lib/semu/share/libretro" ]; then
      ln -s "$out/share/libretro" "$out/lib/semu/share/libretro"
    fi
    chmod -R u+w "$out/lib/semu"

    rm "$out/bin/semu"
    makeWrapper "$out/lib/semu/semu-btrc" "$out/bin/semu" \
      --set SEMU_ASSET_ROOT "$out/lib/semu" \
      --set SEMU_BIN "$out/bin/semu" \
      --prefix PATH : ${lib.escapeShellArg runtimePath}
  '';
  meta.description = "Semu with all contract-required emulators and assets bundled";
}
