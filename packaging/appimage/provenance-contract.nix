let
  flake = builtins.getFlake (toString ../..);
  packages = flake.packages.x86_64-linux;
  emulatorRoot = flake.outPath + "/config/emulators";
  entries = builtins.readDir emulatorRoot;
  definition = id: builtins.fromJSON
    (builtins.readFile (emulatorRoot + "/" + id + "/package.json"));
  isLinux = id:
    let
      emulator = builtins.fromJSON
        (builtins.readFile (emulatorRoot + "/" + id + "/emulator.json"));
      package = definition id;
    in
      builtins.getAttr id entries == "directory"
      && emulator.platforms ? linux
      && (
        (package ? platforms
          && builtins.elem "x86_64-linux" package.platforms)
        || (!(package ? platforms)
          && package.outputs ? linux_main_program)
      );
  ids = builtins.filter isLinux (builtins.attrNames entries);
  localName = value:
    let
      base = builtins.baseNameOf value;
      length = builtins.stringLength base;
    in
      if length > 33 && builtins.substring 32 1 base == "-"
      then builtins.substring 33 (length - 33) base
      else base;
  patch = value:
    if builtins.isAttrs value then {
      kind = "fixed-output";
      name = value.name;
      sha256 = value.outputHash;
      urls = value.urls or [ value.url ];
    } else {
      kind = "local";
      name = localName value;
      sha256 = builtins.hashFile "sha256" value;
      urls = [ ];
    };
  source = id:
    let
      package = builtins.getAttr (id + "-runtime") packages;
      packageDefinition = definition id;
    in {
      recipe_sha256 = builtins.hashFile "sha256"
        (emulatorRoot + "/" + id + "/package.nix");
      source_revision = packageDefinition.source.revision;
      source_sha256 = packageDefinition.source.sha256;
      package_store_path = toString package;
      patches = map patch (package.patches or [ ]);
    };
  esDe = packages.es-de.semuSettings;
in {
  es_de = {
    recipe_sha256 = builtins.hashFile "sha256"
      (flake.outPath + "/packaging/esde/package.nix");
    source_revision = esDe.sourceRevision;
    source_sha256 = esDe.sourceHash;
    patches = map patch packages.es-de.patches;
    settings_protocol = esDe.protocol;
  };
  emulators = builtins.listToAttrs
    (map (id: { name = id; value = source id; }) ids);
}
