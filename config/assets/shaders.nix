# semu_shaders.nix — generic interpreter for the shader half of
# config/assets/shaders.json: stages every "trees"
# entry under share/libretro/shaders/<stage> and emits each declarative shader
# recipe at share/libretro/shaders/semu/<relative> — the "semu/" namespace
# ShaderSelector.resolvePath maps "assets/shaders/..." onto. No pins or
# preset names live here; shaders.json additions need zero nix edits.
{ lib, stdenvNoCC, fetchFromGitHub, pkgs }:

let
  sources = lib.importJSON ./shaders.json;

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
    '');

  slangAssets = lib.filterAttrs (_: recipe: recipe.type == "slang_wrapper")
    sources.assets;
  pipelineAssets = lib.filterAttrs (_: recipe: recipe.type == "slang_pipeline")
    sources.assets;
  shaderAssets = slangAssets // pipelineAssets;

  validSha256 = digest:
    builtins.isString digest
      && builtins.match "^[0-9a-f]{64}$" digest != null;

  stagedReference = reference:
    let tree = sources.trees.${reference.tree};
    in "$out/${root}/${tree.stage}/${reference.path}";

  verifyHash = label: path: expected: ''
    if [ ! -f "${path}" ]; then
      echo "semu-shaders: missing ${label}: ${path}" >&2
      exit 1
    fi
    actualHash="$(sha256sum "${path}" | cut -d ' ' -f 1)"
    if [ "$actualHash" != "${expected}" ]; then
      echo "semu-shaders: hash mismatch for ${label}: $actualHash != ${expected}" >&2
      exit 1
    fi
  '';

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
      "slang_wrapper ${key} requires trees missing from shaders.json: ${toString missingRequires}";
    assert lib.assertMsg (validSha256 (recipe.reference.sha256 or null))
      "slang_wrapper ${key} must pin its referenced preset sha256";
    assert lib.assertMsg (validSha256 (recipe.output.sha256 or null))
      "slang_wrapper ${key} must pin its emitted preset sha256";
    ''
      ${verifyHash "source for ${key}"
        (stagedReference recipe.reference) recipe.reference.sha256}
      mkdir -p "$out/${root}/${wrapperDir}"
      printf '%s' '${text}' > "$out/${root}/semu/${relative}"
      ${verifyHash "output for ${key}"
        "$out/${root}/semu/${relative}" recipe.output.sha256}
    '';

  emitPipeline = key: recipe:
    let
      relative = lib.removePrefix "assets/shaders/" key;
      pipelineDir = dirOf "semu/${relative}";
      up = lib.concatStrings
        (lib.genList (_: "../") (lib.length (lib.splitString "/" pipelineDir)));
      referencedTrees = lib.unique (map (pass: pass.tree) recipe.passes);
      missingTrees = lib.filter (tree: !(sources.trees ? ${tree})) referencedTrees;
      missingRequires =
        lib.filter (tree: !(sources.trees ? ${tree})) (recipe.requires or [ ]);
      invalidPassHashes = lib.filter
        (pass: !validSha256 (pass.sha256 or null)) recipe.passes;
      renderBool = value: if value then "true" else "false";
      renderPass = index: pass:
        let
          tree = sources.trees.${pass.tree};
          suffix = toString index;
        in
        ''
          shader${suffix} = "${up}${tree.stage}/${pass.path}"
          filter_linear${suffix} = "${renderBool (pass.filter_linear or false)}"
        ''
        + lib.optionalString (pass ? scale_type)
          ''scale_type${suffix} = "${pass.scale_type}"
''
        + lib.optionalString (pass ? scale)
          ''scale${suffix} = "${renderParam pass.scale}"
'';
      text = ''shaders = "${toString (lib.length recipe.passes)}"

''
        + lib.concatStrings (lib.imap0 renderPass recipe.passes)
        + "\n"
        + lib.concatStrings (lib.mapAttrsToList
            (param: value: "${param} = \"${renderParam value}\"\n")
            (recipe.params or { }));
    in
    assert lib.assertMsg (recipe.passes != [ ])
      "slang_pipeline ${key} must declare at least one pass";
    assert lib.assertMsg (missingTrees == [ ])
      "slang_pipeline ${key} references trees missing from shaders.json: ${toString missingTrees}";
    assert lib.assertMsg (missingRequires == [ ])
      "slang_pipeline ${key} requires trees missing from shaders.json: ${toString missingRequires}";
    assert lib.assertMsg (invalidPassHashes == [ ])
      "slang_pipeline ${key} must pin every pass sha256";
    assert lib.assertMsg (validSha256 (recipe.output.sha256 or null))
      "slang_pipeline ${key} must pin its emitted preset sha256";
    ''
      ${lib.concatMapStrings (pass: verifyHash
        "source pass ${pass.path} for ${key}"
        (stagedReference pass) pass.sha256) recipe.passes}
      mkdir -p "$out/${root}/${pipelineDir}"
      printf '%s' '${text}' > "$out/${root}/semu/${relative}"
      ${verifyHash "output for ${key}"
        "$out/${root}/semu/${relative}" recipe.output.sha256}
    '';
in
assert lib.assertMsg (sources.schema_version == 2)
  "shaders.json schema_version must be 2";
stdenvNoCC.mkDerivation {
  pname = "semu-shaders";
  version = toString sources.schema_version;

  dontUnpack = true;

  installPhase = ''
    runHook preInstall
    ${lib.concatStrings (lib.mapAttrsToList stageTree sources.trees)}
    ${lib.concatStrings (lib.mapAttrsToList emitWrapper slangAssets)}
    ${lib.concatStrings (lib.mapAttrsToList emitPipeline pipelineAssets)}
    runHook postInstall
  '';

  passthru = {
    shaderAssetNames = lib.attrNames shaderAssets;
    treeNames = lib.attrNames sources.trees;
  };

  meta = {
    description = "Semu RetroArch shader tree rendered from the shaders.json manifest";
    platforms = lib.platforms.all;
  };
}
