{
  pkgs,
  lib,
  repositoryRoot,
}:

let
  bezels = pkgs.callPackage (repositoryRoot + "/config/assets/bezels.nix") { };
  shaders = pkgs.callPackage (repositoryRoot + "/config/assets/shaders.nix") { };
  bezelManifest = lib.importJSON (repositoryRoot + "/config/assets/bezels.json");
  shaderManifest = lib.importJSON (repositoryRoot + "/config/assets/shaders.json");
  declaredNames = lib.attrNames bezelManifest.assets ++ lib.attrNames shaderManifest.assets;
  builtNames = bezels.imageAssetNames ++ shaders.shaderAssetNames;
  missingRecipes = lib.filter (name: !(lib.elem name builtNames)) declaredNames;
in
assert lib.assertMsg (
  missingRecipes == [ ]
) "assets missing a Nix recipe: ${toString missingRecipes}";
{
  inherit bezels shaders;

  combined = pkgs.symlinkJoin {
    name = "semu-visual-assets";
    paths = [
      bezels
      shaders
    ];
    passthru = {
      assetNames = declaredNames;
      assetCount = lib.length declaredNames;
    };
  };
}
