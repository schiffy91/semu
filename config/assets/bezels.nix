# The bezel-art asset tree for Semu, driven by config/assets/bezels.json (recipe types copy,
# local, flatten, recolor, glass, panel, shell, photo, scene, plus the "staging" section).
#
# (default / `semu-bezels`) bakes every recipe here, on the machine that builds Semu: verbatim
#   `copy` files and Semu's own `local` files are byte-checked against their pinned hashes;
#   plates derived from the Duimon and Soqueroeu packs are rendered with imagemagick over the
#   pinned upstreams and checked for their declared size and PNG type only. The repository
#   carries no derived art (the packs' licences forbid sharing adapted material, see NOTICE.md).
# (passthru.generate) is the same render, kept as the named regenerator for inspection.
#
# Output layout: share/semu/<asset key> (what the launcher joins "assets/..." onto) plus every
# staging destination whose src names a bezels.json asset or asset directory.
{ lib, stdenvNoCC, fetchFromGitHub, fetchurl, imagemagick }:

let
  repoRoot = ../..;
  assetSource = lib.fileset.toSource {
    root = repoRoot;
    fileset = lib.fileset.unions [
      ./bezels.json
      ./bezels
      ./fonts
    ];
  };
  sources = lib.importJSON (assetSource + "/config/assets/bezels.json");

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

  imageTypes = [ "copy" "local" "flatten" "recolor" "glass" "panel" "shell" "photo" "scene" ];
  imageAssets = lib.filterAttrs (_: recipe: lib.elem recipe.type imageTypes)
    sources.assets;

  validOutputMetadata = recipe:
    let output = recipe.output or { };
    in recipe ? output
      && output ? width && builtins.isInt output.width && output.width > 0
      && output ? height && builtins.isInt output.height && output.height > 0
      && output ? bit_depth && builtins.isInt output.bit_depth
      && lib.elem output.bit_depth [ 8 16 ]
      && output ? color_type && builtins.isInt output.color_type
      && lib.elem output.color_type [ 2 6 ]
      && (!(output ? sha256) || (builtins.isString output.sha256
        && builtins.match "^[0-9a-f]{64}$" output.sha256 != null));  # generated art is baked here, so only its shape is pinned
  invalidOutputMetadata = lib.attrNames
    (lib.filterAttrs (_: recipe: recipe ? output && !validOutputMetadata recipe)
      imageAssets);
  outputMatchesCrop = recipe:
    let
      crop = builtins.match
        "^([0-9]+)x([0-9]+)[+][0-9]+[+][0-9]+$"
        (recipe.crop or "");
    in !(recipe ? output && recipe ? crop)
      || (crop != null
        && recipe.output.width == builtins.fromJSON (builtins.elemAt crop 0)
        && recipe.output.height == builtins.fromJSON (builtins.elemAt crop 1));
  outputCropMismatches = lib.attrNames
    (lib.filterAttrs (_: recipe: !outputMatchesCrop recipe) imageAssets);

  copyAssets = lib.filterAttrs (_: recipe: recipe.type == "copy") imageAssets;
  copyAssetsMissingHashes = lib.attrNames
    (lib.filterAttrs (_: recipe: !(recipe ? file_sha256)) copyAssets);

  policy = sources.policy or { };
  policyPresentations = policy.presentations or { };
  policyProfiles = policy.profiles or { };
  policySystems = policy.systems or { };

  policyAssetReferences = lib.concatMap
    (presentation: lib.filter (asset: asset != "") [
      (presentation.art or "")
      (presentation.glass or "")
    ])
    (lib.attrValues policyPresentations);
  policyProfileReferences = lib.concatMap
    (profile: lib.filter (variant: variant != "") [
      (profile.default_variant or "")
      (profile.widescreen_variant or "")
    ])
    (lib.attrValues policyProfiles);
  policySystemReferences = lib.concatMap
    (systemPolicy:
      lib.attrValues (systemPolicy.variants or { })
      ++ (systemPolicy.alternatives or [ ])
      ++ lib.filter (variant: variant != "") [
        (systemPolicy.default_variant or "")
        (systemPolicy.widescreen_variant or "")
      ])
    (lib.attrValues policySystems);
  missingPolicyAssets = lib.unique (lib.filter
    (asset: !(imageAssets ? ${asset})) policyAssetReferences);
  missingPolicyPresentations = lib.unique (lib.filter
    (variant: !(policyPresentations ? ${variant}))
    (policyProfileReferences ++ policySystemReferences));

  outFile = key: ''"$out/share/semu/${key}"'';
  treePath = recipe: path: "${githubTrees.${recipe.from}}/${path}";
  treeFile = recipe: path: ''"${treePath recipe path}"'';
  urlFile = recipe: ''"${urlFiles.${recipe.from}}"'';

  copyUpstreamFile = key: recipe:
    let source = treePath recipe recipe.path;
    in ''
      if [ ! -f "${source}" ]; then
        echo "semu-bezels: missing pinned upstream file for ${key}: ${source}" >&2
        exit 1
      fi
      actualHash="$(sha256sum "${source}" | cut -d ' ' -f 1)"
      if [ "$actualHash" != "${recipe.file_sha256}" ]; then
        echo "semu-bezels: file hash mismatch for ${key}: $actualHash != ${recipe.file_sha256}" >&2
        exit 1
      fi
      cp "${source}" ${outFile key}
      if ! cmp -s "${source}" ${outFile key}; then
        echo "semu-bezels: staged bytes differ from pinned upstream for ${key}" >&2
        exit 1
      fi
    '';

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

  # Composite an already-baked, pixel-aligned asset (e.g. the GBC glass layer,
  # which carries Duimon's authentic rainbow "GAME BOY COLOR" wordmark) over a
  # rendered device as the LAST step, so it survives any recolor tint. The
  # overlay asset shares the shell's crop, so no resize/placement is needed.
  overlayAssetPass = recipe:
    lib.optionalString (recipe ? overlay_asset)
      '' \( ${outFile recipe.overlay_asset} \) -compose Over -composite '';

  # Output metadata binds the checked-in package bytes to the deterministic
  # generator. Read the PNG IHDR directly so the normal stager stays free of
  # ImageMagick and cannot silently ship an old, differently cropped bitmap.
  verifyOutput = key: recipe:
    lib.optionalString (recipe ? output) ''
      set -- $(od -An -tu1 -N10 -j16 ${outFile key})
      actualWidth=$(( $1 * 16777216 + $2 * 65536 + $3 * 256 + $4 ))
      actualHeight=$(( $5 * 16777216 + $6 * 65536 + $7 * 256 + $8 ))
      actualBitDepth=$9
      actualColorType=''${10}
      if [ "$actualWidth" -ne ${toString recipe.output.width} ] \
          || [ "$actualHeight" -ne ${toString recipe.output.height} ] \
          || [ "$actualBitDepth" -ne ${toString recipe.output.bit_depth} ] \
          || [ "$actualColorType" -ne ${toString recipe.output.color_type} ]; then
        echo "semu-bezels: PNG metadata mismatch for ${key}: ''${actualWidth}x''${actualHeight} depth=''${actualBitDepth} color=''${actualColorType}" >&2
        exit 1
      fi
    '' + lib.optionalString (recipe ? output && recipe.output ? sha256) ''
      actualHash="$(sha256sum ${outFile key} | cut -d ' ' -f 1)"
      if [ "$actualHash" != "${recipe.output.sha256}" ]; then
        echo "semu-bezels: output hash mismatch for ${key}: $actualHash != ${recipe.output.sha256}" >&2
        exit 1
      fi
    '';

  render = key: recipe:
    ''
      mkdir -p "$(dirname "$out/share/semu/${key}")"
    '' + ({
      copy = copyUpstreamFile key recipe;
      local = ''
        cp "${assetSource + "/${recipe.path}"}" ${outFile key}
      '';
      # Pinned device photograph (fetchurl-pinned, e.g. Wikimedia Commons):
      # optional deskew rotation, trim to the device silhouette, cap size.
      photo = ''
        magick ${urlFile recipe} -background none \
          ${lib.optionalString (recipe ? rotate) "-rotate ${toString recipe.rotate} "} \
          -trim +repage -resize '2560x2560>' "PNG32:$out/share/semu/${key}"
      '';
      # -flatten merges the whole layer stack; pairwise -composite would only
      # merge the last two layers (the GBC LED-layer regression). A layer set
      # may include its own screen-lens/glass layer (e.g. GBC_Glass with the
      # authentic wordmark) as the topmost entry so the logo bakes in aligned.
      flatten =
        if recipe ? crop then ''
          magick ${lib.concatMapStringsSep " " (layer: treeFile recipe layer) recipe.layers} \
            -background none -flatten -resize '2048x2048>' \
            -crop ${recipe.crop} +repage ${outFile key}
        ''
        else ''
          magick ${lib.concatMapStringsSep " " (layer: treeFile recipe layer) recipe.layers} \
            -background none -flatten -resize '2048x2048>' ${outFile key}
        '';
      # scene: flatten pinned layers, multiply the ambient lighting plate over
      # them (opacity blends the plate toward white first, so 1.0 is the full
      # late-night darkening and 0.0 is daylight), then downscale.
      scene = let
        ambient = recipe.ambient or null;
        ambientFile = ''"${githubTrees.${ambient.from}}/${ambient.path}"'';
        lift = toString (builtins.floor ((1.0 - (ambient.opacity or 1.0)) * 100.0 + 0.5));
      in ''
        magick ${lib.concatMapStringsSep " " (layer: treeFile recipe layer) recipe.layers} \
          -background none -flatten "PNG32:$TMPDIR/scene.png"
        sceneDims=$(magick identify -format "%wx%h" "$TMPDIR/scene.png")
        magick "$TMPDIR/scene.png" ${lib.optionalString (ambient != null)
          ''\( ${ambientFile} -resize "$sceneDims!" -fill white -colorize ${lift}% \) -compose Multiply -composite ''} \
          ${lib.optionalString (recipe ? resize) "-resize '${recipe.resize}' "}"PNG32:$out/share/semu/${key}"
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
          -compose Multiply -composite -alpha on ${overlayAssetPass recipe}"PNG32:$out/share/semu/${key}"
      '';
      # layered device shell from a Duimon layer set. Their device base is
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
    }.${recipe.type}) + verifyOutput key recipe;

  # recolor reads its base from $out, so bases render first.
  phases = lib.partition (entry: entry.recipe.type != "recolor")
    (lib.mapAttrsToList (key: recipe: { inherit key recipe; }) imageAssets);
  renderScript = lib.concatMapStrings (entry: render entry.key entry.recipe)
    (phases.right ++ phases.wrong);

  # The stager bakes every derived plate from the pinned upstreams here, on the machine that
  # builds Semu: the repository carries recipes, never Duimon or Soqueroeu derivatives (their
  # licences forbid distributing adapted material). Verbatim copies stay byte-checked.

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

  imageAssetNames = lib.attrNames imageAssets;

  # The regenerator: fetch upstreams + render every recipe with imagemagick.
  # `nix build .#bezel-generate`, then copy its share/semu/assets/bezels/ back over the
  # committed tree. Kept as passthru.generate (and re-exported as the
  # `bezel-generate` package) so a plain app/asset build never forces the
  # fetch/render path.
  generate = stdenvNoCC.mkDerivation {
    pname = "semu-bezels-generate";
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
      inherit imageAssetNames;
    };

    meta = {
      description = "Regenerator for generated Semu bezel art from bezels.json";
      platforms = lib.platforms.all;
    };
  };
in
assert lib.assertMsg (copyAssetsMissingHashes == [ ])
  "upstream copy recipes missing file_sha256: ${toString copyAssetsMissingHashes}";
assert lib.assertMsg (invalidOutputMetadata == [ ])
  "bezel recipes have invalid output metadata: ${toString invalidOutputMetadata}";
assert lib.assertMsg (outputCropMismatches == [ ])
  "bezel output geometry disagrees with its crop: ${toString outputCropMismatches}";
assert lib.assertMsg (missingPolicyAssets == [ ])
  "central bezel presentations reference unknown assets: ${toString missingPolicyAssets}";
assert lib.assertMsg (missingPolicyPresentations == [ ])
  "central bezel policy references unknown presentations: ${toString missingPolicyPresentations}";
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
    inherit imageAssetNames generate;
  };

  meta = {
    description = "Semu bezel art baked from bezels.json recipes over the pinned upstreams";
    platforms = lib.platforms.all;
  };
}
