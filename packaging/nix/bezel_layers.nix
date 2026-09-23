# Every upstream plate a bezel package draws, bundled where the launcher looks first
# (share/semu/assets/bezels/layers/<package>/<layer><ext>). Files are copied verbatim, once each,
# so the release carries only what is drawn and never the whole upstream repositories.
{ lib, runCommand, fetchFromGitHub, repositoryRoot }:

let
  bezelManifest = lib.importJSON (repositoryRoot + "/config/assets/bezels.json");
  upstreams = lib.mapAttrs (_: spec: fetchFromGitHub { inherit (spec) owner repo rev; hash = spec.nar_hash; })
    (lib.filterAttrs (_: spec: spec.kind == "github") bezelManifest.upstreams);
  packageIds = lib.attrNames (lib.filterAttrs (_: kind: kind == "directory") (builtins.readDir (repositoryRoot + "/config/bezels")));
  packageOf = id: lib.importJSON (repositoryRoot + "/config/bezels/${id}/bezel.json");
  upstreamRef = reference:
    let parts = builtins.match "([a-z_]+):(.+)" reference; in
    if parts == null || !(upstreams ? ${builtins.elemAt parts 0}) then null
    else { upstream = builtins.elemAt parts 0; path = builtins.elemAt parts 1; };
  extension = path: let m = builtins.match ".*(\\.[A-Za-z]+)" path; in if m == null then "" else builtins.head m;
  entriesOf = id:
    let
      package = packageOf id;
      layers = map (layer: { name = layer.id; ref = upstreamRef (layer.file or ""); }) (package.layers or [ ]);
      ambient = lib.optional (package ? ambient) { name = "ambient"; ref = upstreamRef package.ambient.file; };
    in map (entry: entry // { package = id; }) (lib.filter (entry: entry.ref != null) (layers ++ ambient));
  entries = lib.concatMap entriesOf packageIds;
  unique = lib.unique (map (entry: entry.ref) entries);
  root = "share/semu/assets/bezels/layers";
in
runCommand "semu-bezel-layers" { } ''
  ${lib.concatMapStrings (ref: ''
    install -Dm644 "${upstreams.${ref.upstream}}/${ref.path}" "$out/${root}/_files/${ref.upstream}/${ref.path}"
  '') unique}
  ${lib.concatMapStrings (entry: ''
    mkdir -p "$out/${root}/${entry.package}"
    ln -s "../_files/${entry.ref.upstream}/${entry.ref.path}" "$out/${root}/${entry.package}/${entry.name}${extension entry.ref.path}"
  '') entries}
''
