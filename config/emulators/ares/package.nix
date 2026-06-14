{ pkgs
, semuPackage ? pkgs.callPackage ../../../build/packaging/nix/lib/source-package.nix {}
}:

let
  sourceHook = {
    status = "not-required";
    requiresSourceBuild = false;
    productionPatchable = false;
    sourcePackagePath = null;
    sourcePlatforms = [];
    binaryPackagePaths = [];
    remainingGap = "No Semu source/content hook is planned for ares; it remains native-output only.";
  };
in
semuPackage.withSourceHookMetadata {
  id = "ares";
  package = pkgs.ares;
  inherit sourceHook;
}
