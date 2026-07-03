# semu_bezels.nix — generic interpreter for the image half of
# src/semu/assets/sources.json (recipe types copy /
# local / flatten / recolor / glass) plus its "staging" section. sources.json
# is the only place upstream pins and asset names live; a new entry there is
# picked up here with zero nix edits.
#
# Output layout:
#   share/semu/<asset key>   canonical tree ($SEMU_ASSET_ROOT/share/semu is
#                            what BezelResolver joins "assets/..." onto)
#   <staging dest>           every staging file whose src names a sources.json
#                            asset (or an asset directory prefix); non-asset
#                            srcs (runtime build products such as the Deck tap
#                            .so under generated/) are skipped — their own
#                            pipelines stage them.
{ lib, stdenvNoCC, fetchFromGitHub, imagemagick }:

let
  sources = lib.importJSON ./sources.json;
  repoRoot = ../../..;

  githubTrees = lib.mapAttrs
    (_: spec: fetchFromGitHub {
      inherit (spec) owner repo rev;
      hash = spec.nar_hash;
    })
    (lib.filterAttrs (_: spec: spec.kind == "github") sources.upstreams);

  imageTypes = [ "copy" "local" "flatten" "recolor" "glass" ];
  imageAssets = lib.filterAttrs (_: recipe: lib.elem recipe.type imageTypes)
    sources.assets;

  outFile = key: ''"$out/share/semu/${key}"'';
  treeFile = recipe: path: ''"${githubTrees.${recipe.from}}/${path}"'';

  render = key: recipe:
    ''
      mkdir -p "$(dirname "$out/share/semu/${key}")"
    '' + {
      copy = ''
        cp ${treeFile recipe recipe.path} ${outFile key}
      '';
      local = ''
        cp "${repoRoot + "/${recipe.path}"}" ${outFile key}
      '';
      # -flatten merges the whole layer stack; pairwise -composite would only
      # merge the last two layers (the GBC LED-layer regression).
      flatten = ''
        magick ${lib.concatMapStringsSep " " (layer: treeFile recipe layer) recipe.layers} \
          -background none -flatten -resize '2048x2048>' ${outFile key}
      '';
      # glass keeps the live cutout mask in .a and reflections in .rgb;
      # PNG32 forces RGBA to survive the downscale.
      glass = ''
        magick ${treeFile recipe recipe.path} -resize '2048x2048>' \
          "PNG32:$out/share/semu/${key}"
      '';
      # desaturate then Multiply a solid tint: preserves plastic shading + alpha.
      recolor = ''
        magick ${outFile recipe.base} -modulate 100,0,100 \
          \( +clone -fill "${recipe.color}" -colorize 100% \) \
          -compose Multiply -composite -alpha on "PNG32:$out/share/semu/${key}"
      '';
    }.${recipe.type};

  # recolor reads its base from $out, so bases render first.
  phases = lib.partition (entry: entry.recipe.type != "recolor")
    (lib.mapAttrsToList (key: recipe: { inherit key recipe; }) imageAssets);
  renderScript = lib.concatMapStrings (entry: render entry.key entry.recipe)
    (phases.right ++ phases.wrong);

  assetKeys = lib.attrNames sources.assets;
  isAssetDir = src: lib.any (key: lib.hasPrefix "${src}/" key) assetKeys;

  stageFile = unitId: file:
    if imageAssets ? ${file.src} then ''
      mkdir -p "$(dirname "$out/${file.dest}")"
      cp ${outFile file.src} "$out/${file.dest}"
    '' else if isAssetDir file.src then ''
      mkdir -p "$(dirname "$out/${file.dest}")"
      cp -R "$out/share/semu/${file.src}" "$out/${file.dest}"
    '' else ''
      echo "semu-bezels: skipping staging ${unitId}: ${file.src} (not a sources.json asset)"
    '';
  stagingScript = lib.concatMapStrings
    (unit: lib.concatMapStrings (stageFile unit.id) unit.files)
    sources.staging;
in
stdenvNoCC.mkDerivation {
  pname = "semu-bezels";
  version = toString sources.schema_version;

  dontUnpack = true;
  nativeBuildInputs = [ imagemagick ];

  installPhase = ''
    runHook preInstall
    ${renderScript}
    ${stagingScript}
    runHook postInstall
  '';

  passthru = {
    imageAssetNames = lib.attrNames imageAssets;
  };

  meta = {
    description = "Semu bezel art rendered from the sources.json manifest";
    platforms = lib.platforms.all;
  };
}
