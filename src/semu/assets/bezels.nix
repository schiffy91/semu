# semu_bezels.nix — generic interpreter for the global bezel manifest
# src/semu/assets/bezels.json (recipe types copy / local / flatten / recolor /
# glass) plus its "staging" section. bezels.json is the only place bezel
# upstream pins, art recipes, and the generic fallback table live; a new entry
# there is picked up here with zero nix edits.
#
# Output layout:
#   share/semu/<asset key>   canonical tree ($SEMU_ASSET_ROOT/share/semu is
#                            what BezelResolver joins "assets/..." onto)
#   <staging dest>           every staging file whose src names a bezels.json
#                            asset (or an asset directory prefix); non-asset
#                            srcs (runtime build products such as the Deck tap
#                            .so under generated/) are skipped — their own
#                            pipelines stage them.
{ lib, stdenvNoCC, fetchFromGitHub, imagemagick }:

let
  sources = lib.importJSON ./bezels.json;
  repoRoot = ../../..;

  githubTrees = lib.mapAttrs
    (_: spec: fetchFromGitHub {
      inherit (spec) owner repo rev;
      hash = spec.nar_hash;
    })
    (lib.filterAttrs (_: spec: spec.kind == "github") sources.upstreams);

  imageTypes = [ "copy" "local" "flatten" "recolor" "glass" "panel" ];
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
      # Optional "brightness" (percent, default 100) lifts the base first —
      # multiply can only darken, so dark shells need the lift to hit
      # saturated colorway hues.
      recolor = ''
        magick ${outFile recipe.base} -modulate ${toString (recipe.brightness or 100)},0,100 \
          \( +clone -fill "${recipe.color}" -colorize 100% \) \
          -compose Multiply -composite -alpha on "PNG32:$out/share/semu/${key}"
      '';
      # declarative drawn bezel: ordered plates (round_rect / circle) on a
      # transparent canvas — the manifest entry IS the drawing, no upstream.
      panel = let
        canvasWidth = recipe.size.w;
        canvasHeight = recipe.size.h;
        pixelX = fraction: toString (builtins.floor (fraction * canvasWidth + 0.5));
        pixelY = fraction: toString (builtins.floor (fraction * canvasHeight + 0.5));
        drawPlate = plate:
          if plate.kind == "circle" then
            ''-draw "fill ${plate.fill} circle ${pixelX plate.cx},${pixelY plate.cy} ${
              toString (builtins.floor (plate.cx * canvasWidth + 0.5) + plate.radius)},${pixelY plate.cy}" ''
          else
            ''-draw "${lib.optionalString (plate ? stroke)
                "stroke ${plate.stroke} stroke-width 2 "}fill ${plate.fill} roundrectangle ${
              pixelX plate.x},${pixelY plate.y} ${
              pixelX (plate.x + plate.w)},${pixelY (plate.y + plate.h)} ${
              toString plate.radius},${toString plate.radius}" '';
      in ''
        magick -size ${toString canvasWidth}x${toString canvasHeight} canvas:none \
          ${lib.concatMapStrings drawPlate recipe.plates} "PNG32:$out/share/semu/${key}"
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
      echo "semu-bezels: skipping staging ${unitId}: ${file.src} (not a bezels.json asset)"
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
    description = "Semu bezel art rendered from the bezels.json manifest";
    platforms = lib.platforms.all;
  };
}
