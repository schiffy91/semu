# flake/checks.nix — fast, network-free gates:
#   tree-audit    the repo's own approved-shape audit (tests/Makefile), run
#                 against the flake source
#   assets-verify pure-eval walk of every bezels.json/shaders.json asset
#                 reference (plus the code-owned generic fallback trio)
#                 against the sources.json recipe set
{ self, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
  pkgs = mkPkgs system;
  lib = pkgs.lib;

  semuRoot = ../../..;
  sourcesJson = lib.importJSON (semuRoot + "/assets/sources.json");
  recipeKeys = lib.attrNames sourcesJson.assets;

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

  # BezelResolver.genericFallback's code-owned art must stay buildable too.
  genericFallback = [
    "assets/bezels/generic/4x3.png"
    "assets/bezels/generic/16x9.png"
    "assets/bezels/generic/dual.png"
  ];

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
      echo "references without a sources.json recipe: $missing" >&2
      status=1
    fi
    if [ -n "$notCanonical" ]; then
      echo "non-canonical (not assets/...) references: $notCanonical" >&2
      status=1
    fi
    [ "$status" = 0 ] || exit "$status"
    echo "OK: $referencedCount referenced assets all have sources.json recipes" > $out
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
