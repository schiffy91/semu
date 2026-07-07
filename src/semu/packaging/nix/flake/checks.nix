# flake/checks.nix — fast, network-free gates:
#   tree-audit    the repo's own approved-shape audit (tests/Makefile), run
#                 against the flake source
#   assets-verify pure-eval walk of every per-system bezels.json/shaders.json
#                 asset reference (plus the global generic fallback table)
#                 against the bezels.json + shaders.json recipe sets
{ self, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
  pkgs = mkPkgs system;
  lib = pkgs.lib;

  semuRoot = ../../..;
  bezelManifest = lib.importJSON (semuRoot + "/assets/bezels.json");
  shaderManifest = lib.importJSON (semuRoot + "/assets/shaders.json");
  recipeKeys = lib.attrNames bezelManifest.assets
    ++ lib.attrNames shaderManifest.assets;

  systemsDir = semuRoot + "/systems";
  systemIds = lib.attrNames (lib.filterAttrs
    (name: type: type == "directory"
      && builtins.pathExists (systemsDir + "/${name}/system.json"))
    (builtins.readDir systemsDir));

  jsonOr = path: if builtins.pathExists path then lib.importJSON path else { };

  referencesOf = id: let
    bezels = jsonOr (systemsDir + "/${id}/bezels.json");
    shaders = jsonOr (systemsDir + "/${id}/shaders.json");
    widescreen = shaders.widescreen or { };
    variantRefs = lib.concatMap
      (variant: [ (variant.art or "") (variant.glass or "") ])
      (bezels.variants or [ ]);
    shaderRefs = [
      (shaders.screen or "") (shaders.composite or "")
      (widescreen.screen or "") (widescreen.composite or "")
    ];
  in lib.filter (ref: ref != "") (variantRefs ++ shaderRefs);

  # The generic fallback table's art (bezels.json "generic"."profiles") must
  # stay buildable too — derived from the manifest, never listed here.
  genericFallback = lib.unique (lib.mapAttrsToList (_: profile: profile.art)
    ((bezelManifest.generic or { }).profiles or { }));

  referenced = lib.unique (genericFallback ++ lib.concatMap referencesOf systemIds);
  missing = lib.filter (ref: !(lib.elem ref recipeKeys)) referenced;
  notCanonical = lib.filter (ref: !(lib.hasPrefix "assets/" ref)) referenced;
in {
  assets-verify = pkgs.runCommand "semu-assets-verify" {
    missing = toString missing;
    notCanonical = toString notCanonical;
    referencedCount = toString (lib.length referenced);
  } ''
    status=0
    if [ -n "$missing" ]; then
      echo "references without a bezels.json/shaders.json recipe: $missing" >&2
      status=1
    fi
    if [ -n "$notCanonical" ]; then
      echo "non-canonical (not assets/...) references: $notCanonical" >&2
      status=1
    fi
    [ "$status" = 0 ] || exit "$status"
    echo "OK: $referencedCount referenced assets all have manifest recipes" > $out
  '';

  tree-audit = pkgs.runCommand "semu-tree-audit" {
    nativeBuildInputs = [
      pkgs.gnumake pkgs.findutils pkgs.gnugrep pkgs.gnused pkgs.bash
    ];
  } ''
    # same audit the repo runs; only the scratch path moves into the sandbox
    sed "s|/tmp/|$TMPDIR/|g" ${self}/tests/Makefile > tree-audit.mk
    make -f "$PWD/tree-audit.mk" -C ${self} SHELL="$(type -p bash)" tree-audit
    touch $out
  '';
})
