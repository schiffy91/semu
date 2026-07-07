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
{ lib, stdenvNoCC, fetchFromGitHub, fetchurl, imagemagick, dejavu_fonts }:

let
  sources = lib.importJSON ./bezels.json;
  repoRoot = ../../..;

  githubTrees = lib.mapAttrs
    (_: spec: fetchFromGitHub {
      inherit (spec) owner repo rev;
      hash = spec.nar_hash;
    })
    (lib.filterAttrs (_: spec: spec.kind == "github") sources.upstreams);

  urlFiles = lib.mapAttrs
    (_: spec: fetchurl {
      inherit (spec) url name;
      sha256 = spec.sha256_base32;
    })
    (lib.filterAttrs (_: spec: spec.kind == "url") sources.upstreams);

  imageTypes = [ "copy" "local" "flatten" "recolor" "glass" "panel" "shell" "photo" ];
  imageAssets = lib.filterAttrs (_: recipe: lib.elem recipe.type imageTypes)
    sources.assets;

  outFile = key: ''"$out/share/semu/${key}"'';
  treeFile = recipe: path: ''"${githubTrees.${recipe.from}}/${path}"'';
  urlFile = recipe: ''"${urlFiles.${recipe.from}}"'';

  # Shared plate renderer for "panel" (whole drawing) and "shell" (overlay
  # plates on a device render): ordered round_rect / circle in canvas
  # fractions of the given size.
  drawPlateFor = canvasWidth: canvasHeight: plate:
    let
      pixelX = fraction: toString (builtins.floor (fraction * canvasWidth + 0.5));
      pixelY = fraction: toString (builtins.floor (fraction * canvasHeight + 0.5));
    in
    if plate.kind == "circle" then
      ''-draw "fill ${plate.fill} circle ${pixelX plate.cx},${pixelY plate.cy} ${
        toString (builtins.floor (plate.cx * canvasWidth + 0.5) + plate.radius)},${pixelY plate.cy}" ''
    else
      ''-draw "${lib.optionalString (plate ? stroke)
          "stroke ${plate.stroke} stroke-width 2 "}fill ${plate.fill} roundrectangle ${
        pixelX plate.x},${pixelY plate.y} ${
        pixelX (plate.x + plate.w)},${pixelY (plate.y + plate.h)} ${
        toString plate.radius},${toString plate.radius}" '';

  # Draw wordmark "labels" the upstream art omits (e.g. GBC "GAME BOY color").
  # Coordinates are fractions of the given canvas; applied as the LAST step so
  # colors survive any prior recolor tint. Shared by flatten and recolor.
  labelPassFor = recipe: width: height:
    let
      drawLabel = label:
        ''-fill "${label.fill}" -pointsize ${
          toString (builtins.floor (label.size * height + 0.5))} ${
          lib.optionalString (label ? weight) "-weight ${toString label.weight} "}-annotate ${
          lib.optionalString (label ? skew) "${toString label.skew}x0"}+${
          toString (builtins.floor (label.x * width + 0.5))}+${
          toString (builtins.floor (label.y * height + 0.5))} "${label.text}" '';
    in
    lib.optionalString (recipe ? labels)
      (" -gravity NorthWest -font ${
        dejavu_fonts}/share/fonts/truetype/DejaVuSans-Bold.ttf "
       + lib.concatMapStrings drawLabel recipe.labels);

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
      # Photoreal device photograph (fetchurl-pinned, e.g. Wikimedia Commons):
      # optional deskew rotation, trim to the device silhouette, cap size.
      photo = ''
        magick ${urlFile recipe} -background none \
          ${lib.optionalString (recipe ? rotate) "-rotate ${toString recipe.rotate} "} \
          -trim +repage -resize '2560x2560>' "PNG32:$out/share/semu/${key}"
      '';
      # -flatten merges the whole layer stack; pairwise -composite would only
      # merge the last two layers (the GBC LED-layer regression). Optional
      # "labels" draw wordmark text (fractions of the post-crop canvas) that
      # the upstream art omits, e.g. the GBC "GAME BOY color" logo.
      flatten =
        if recipe ? crop then
          let
            parts = builtins.match "([0-9]+)x([0-9]+).*" recipe.crop;
            cw = lib.toInt (builtins.elemAt parts 0);
            ch = lib.toInt (builtins.elemAt parts 1);
          in ''
            magick ${lib.concatMapStringsSep " " (layer: treeFile recipe layer) recipe.layers} \
              -background none -flatten -resize '2048x2048>' \
              -crop ${recipe.crop} +repage ${labelPassFor recipe cw ch}${outFile key}
          ''
        else ''
          magick ${lib.concatMapStringsSep " " (layer: treeFile recipe layer) recipe.layers} \
            -background none -flatten -resize '2048x2048>' ${outFile key}
        '';
      # glass keeps the live cutout mask in .a and reflections in .rgb;
      # PNG32 forces RGBA to survive the downscale.
      glass = ''
        magick ${treeFile recipe recipe.path} -resize '2048x2048>' \
          ${lib.optionalString (recipe ? crop) "-crop ${recipe.crop} +repage "}"PNG32:$out/share/semu/${key}"
      '';
      # desaturate then Multiply a solid tint: preserves plastic shading + alpha.
      # Optional "brightness" (percent, default 100) lifts the base first —
      # multiply can only darken, so dark shells need the lift to hit
      # saturated colorway hues.
      recolor = ''
        magick ${outFile recipe.base} -modulate ${toString (recipe.brightness or 100)},0,100 \
          \( +clone -fill "${recipe.color}" -colorize 100% \) \
          -compose Multiply -composite -alpha on ${
            lib.optionalString (recipe ? labels)
              ("-compose Over "
               + labelPassFor recipe
                   (builtins.elemAt recipe.label_canvas 0)
                   (builtins.elemAt recipe.label_canvas 1))
          }"PNG32:$out/share/semu/${key}"
      '';
      # photoreal device shell from a Duimon layer set. Their device base is
      # black RGB with the shape in alpha, so: colorize + top-down light the
      # silhouette, cut with its own alpha, lay the decal (modeled buttons),
      # glass (control markings) and top (branding) Over, blend the LED plate
      # additively (Screen — it is an opaque black plate with lit diodes),
      # then cut again with the silhouette alpha.
      shell = let
        layer = path: treeFile recipe path;
        overLayer = attribute:
          lib.optionalString (recipe ? ${attribute})
            " ${layer recipe.${attribute}} -compose Over -composite";
        cut = " \\( ${layer recipe.silhouette} -alpha extract \\)"
          + " -alpha off -compose CopyOpacity -composite";
      in ''
        shellDims=$(magick identify -format "%wx%h" ${layer recipe.silhouette})
        magick ${layer recipe.silhouette} -fill "${recipe.color}" -colorize 100 \( -size "$shellDims" gradient:"${recipe.light or "#ffffff-#7e7e7e"}" \) -compose Multiply -composite${cut}${overLayer "decal"}${overLayer "glass_markings"}${overLayer "top"}${
          lib.optionalString (recipe ? led)
            " \\( ${layer recipe.led} -alpha off \\) -compose Screen -composite"
        }${cut}${lib.optionalString (recipe ? plates) (" -compose Over " + lib.concatMapStrings (drawPlateFor recipe.size.w recipe.size.h) recipe.plates)}${cut} -resize '2048x2048>' "PNG32:$out/share/semu/${key}"${
          lib.optionalString (recipe ? grain) ''

        magick ${outFile key} -channel RGB -attenuate ${toString recipe.grain} +noise Gaussian +channel "PNG32:$out/share/semu/${key}"''
        }
      '';
      # declarative drawn bezel: ordered plates (round_rect / circle) on a
      # transparent canvas — the manifest entry IS the drawing, no upstream.
      panel = ''
        magick -size ${toString recipe.size.w}x${toString recipe.size.h} canvas:none \
          ${lib.concatMapStrings (drawPlateFor recipe.size.w recipe.size.h) recipe.plates} "PNG32:$out/share/semu/${key}"${
            lib.optionalString (recipe ? grain) ''

        magick ${outFile key} -channel RGB -attenuate ${toString recipe.grain} +noise Gaussian +channel "PNG32:$out/share/semu/${key}"''
          }
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
