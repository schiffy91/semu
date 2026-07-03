# semu_shaders.nix — generic interpreter for the shader half of
# src/semu/assets/sources.json: stages every "trees"
# entry under share/libretro/shaders/<stage> and emits every slang_wrapper
# asset at share/libretro/shaders/semu/<relative> — the "semu/" namespace
# ShaderSelector.resolvePath maps "assets/shaders/..." onto. No pins or
# preset names live here; sources.json additions need zero nix edits.
{ lib, stdenvNoCC, fetchFromGitHub, pkgs }:

let
  sources = lib.importJSON ./sources.json;

  upstreamSource = name: spec:
    if spec.kind == "github" then
      fetchFromGitHub { inherit (spec) owner repo rev; hash = spec.nar_hash; }
    else if spec.kind == "nixpkgs" then
      pkgs.${spec.attr}
    else
      throw "semu_shaders.nix: unknown upstream kind '${spec.kind}' (${name})";
  upstreams = lib.mapAttrs upstreamSource sources.upstreams;

  root = "share/libretro/shaders";

  stageTree = name: tree:
    let
      src = upstreams.${tree.from};
      kind = sources.upstreams.${tree.from}.kind;
      dest = ''"$out/${root}/${tree.stage}"'';
      leaf = baseNameOf tree.stage;
    in
    ''
      mkdir -p "$(dirname "$out/${root}/${tree.stage}")"
    '' + (if kind == "nixpkgs" then ''
      # nixpkgs upstreams ship the tree somewhere inside the package;
      # locate the directory named after the stage leaf.
      treeSrc="$(find ${src} -type d -name "${leaf}" -print -quit)"
      if [ -z "$treeSrc" ]; then
        echo "semu-shaders: tree '${name}': no '${leaf}' directory inside ${src}" >&2
        exit 1
      fi
      cp -RL "$treeSrc" ${dest}
    '' else if tree ? paths then ''
      mkdir -p ${dest}
      ${lib.concatMapStrings (path: ''
        cp -R "${src}/${path}" "$out/${root}/${tree.stage}/${path}"
      '') tree.paths}
      for licenseFile in "${src}"/LICENSE* "${src}"/README*; do
        if [ -f "$licenseFile" ]; then cp "$licenseFile" ${dest}/; fi
      done
    '' else ''
      cp -R "${src}" ${dest}
    '') + ''
      chmod -R u+w ${dest}
    '';

  slangAssets = lib.filterAttrs (_: recipe: recipe.type == "slang_wrapper")
    sources.assets;

  # Nix renders floats with six decimals; the presets want the manifest's
  # literal value shape (60.57, not 60.570000).
  trimZeros = text:
    if lib.hasSuffix "0" text then trimZeros (lib.removeSuffix "0" text) else text;
  renderParam = value:
    if builtins.isInt value then toString value
    else lib.removeSuffix "." (trimZeros (toString value));

  emitWrapper = key: recipe:
    let
      relative = lib.removePrefix "assets/shaders/" key;
      wrapperDir = dirOf "semu/${relative}";
      up = lib.concatStrings
        (lib.genList (_: "../") (lib.length (lib.splitString "/" wrapperDir)));
      refTree = sources.trees.${recipe.reference.tree};
      missingRequires =
        lib.filter (t: !(sources.trees ? ${t})) (recipe.requires or [ ]);
      text = ''#reference "${up}${refTree.stage}/${recipe.reference.path}"''
        + "\n"
        + lib.concatStrings (lib.mapAttrsToList
            (param: value: "${param} = \"${renderParam value}\"\n")
            (recipe.params or { }));
    in
    assert lib.assertMsg (missingRequires == [ ])
      "slang_wrapper ${key} requires trees missing from sources.json: ${toString missingRequires}";
    ''
      mkdir -p "$out/${root}/${wrapperDir}"
      printf '%s' '${text}' > "$out/${root}/semu/${relative}"
    '';
in
stdenvNoCC.mkDerivation {
  pname = "semu-shaders";
  version = toString sources.schema_version;

  dontUnpack = true;

  installPhase = ''
    runHook preInstall
    ${lib.concatStrings (lib.mapAttrsToList stageTree sources.trees)}
    ${lib.concatStrings (lib.mapAttrsToList emitWrapper slangAssets)}
    runHook postInstall
  '';

  passthru = {
    shaderAssetNames = lib.attrNames slangAssets;
    treeNames = lib.attrNames sources.trees;
  };

  meta = {
    description = "Semu RetroArch shader tree rendered from the sources.json manifest";
    platforms = lib.platforms.all;
  };
}
