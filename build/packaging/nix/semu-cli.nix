{ lib, stdenv, makeWrapper
, syncthing ? null
, syncthingtray ? null
, curl ? null
, bubblewrap ? null
}:

let
  runtimeTools = lib.filter (x: x != null) [ syncthing syncthingtray curl bubblewrap ];
  runtimePath = lib.makeBinPath runtimeTools;
  generatedDir = "build/generated";
  generatedSemuC = "${generatedDir}/semu.c";
  generatedQuitWatchC = "${generatedDir}/semu-quit-watch.c";
  requiredGeneratedArtifacts = [ generatedSemuC ]
    ++ lib.optionals stdenv.hostPlatform.isLinux [ generatedQuitWatchC ];
  checkGeneratedArtifacts = lib.concatMapStringsSep "\n" (path: ''
    if [ ! -f ${lib.escapeShellArg path} ]; then
      echo "Semu generated artifact missing: ${path}; run make btrc-build first" >&2
      exit 1
    fi
  '') requiredGeneratedArtifacts;
in
stdenv.mkDerivation {
  pname = "semu";
  version = "0.1.0";
  src = lib.cleanSource ../../..;
  nativeBuildInputs = [ makeWrapper ];
  dontBuild = true;
  installPhase = ''
    mkdir -p $out/bin $out/lib/semu
    ${checkGeneratedArtifacts}

    cp src/main.btrc $out/lib/semu/main.btrc
    cp semu.json $out/lib/semu/
    mkdir -p $out/lib/semu/build
    cp -r ${lib.escapeShellArg generatedDir} $out/lib/semu/build/
    for dir in config; do
      if [ -d "$dir" ]; then
        cp -r "$dir" "$out/lib/semu/"
      fi
    done
    if [ -d assets ]; then
      cp -r assets "$out/lib/semu/"
    fi
    ${stdenv.cc.targetPrefix}cc ${lib.escapeShellArg generatedSemuC} -std=c11 -o $out/lib/semu/semu-btrc -lm
    ${if stdenv.hostPlatform.isLinux then ''
      ${stdenv.cc.targetPrefix}cc ${lib.escapeShellArg generatedQuitWatchC} -std=c11 -O2 -o $out/bin/semu-quit-watch
      install -Dm755 build/packaging/linux/bin/semu-render $out/bin/semu-render
    '' else ''
      cat > $out/bin/semu-quit-watch <<'WRAPPER'
      #!/usr/bin/env sh
      [ "$1" = "--" ] && shift
      exec "$@"
      WRAPPER
      chmod +x $out/bin/semu-quit-watch
      install -Dm755 build/packaging/linux/bin/semu-render $out/bin/semu-render
    ''}
    makeWrapper $out/lib/semu/semu-btrc $out/bin/semu \
      --set-default SEMU_ASSET_ROOT $out/lib/semu \
      --set-default SEMU_BIN $out/bin/semu \
      --prefix PATH : ${lib.escapeShellArg runtimePath}

    cat > $out/bin/semu-settings <<'WRAPPER'
    #!/usr/bin/env sh
    set -eu
    here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
    semu="$here/semu"
    project="''${SEMU_PROJECT_DIR:-$HOME/.local/share/semu}"
    entry="''${1:-}"
    if [ -n "$entry" ] && [ -f "$entry" ]; then
      exec "$semu" settings entry "$entry" --project "$project"
    fi
    exec "$semu" settings ui --project "$project"
    WRAPPER
    chmod +x $out/bin/semu-settings

    if [ -d build/packaging/linux ]; then
      mkdir -p $out/lib/semu/packaging
      cp -r build/packaging/linux $out/lib/semu/packaging/
    fi
  '';
  meta = {
    description = "Semu BTRC CLI";
    license = lib.licenses.mit;
  };
}
