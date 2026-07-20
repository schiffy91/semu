# Fast, network-free checks for the compiler architecture and declarative data.
{
  self,
  btrc,
  systems,
  forAllSystems,
  mkPkgs,
  ...
}:

forAllSystems (
  system:
  let
    pkgs = mkPkgs system;
    lib = pkgs.lib;
    btrcpy = btrc.packages.${system}.btrcpy;
    architectureContract = import ./architecture-contract.nix {
      inherit lib systems;
      repositoryRoot = ../../..;
    };
    productionPackages = self.packages.x86_64-linux;
    expectedRuntimeNames = map (id: "${id}-runtime") architectureContract.linuxEmulatorIds;
    actualRuntimeNames = lib.filter (
      name: lib.hasSuffix "-runtime" name && name != "steamdeck-runtime"
    ) (lib.attrNames productionPackages);
    publicPackageContract =
      assert lib.assertMsg (
        lib.sort builtins.lessThan actualRuntimeNames == lib.sort builtins.lessThan expectedRuntimeNames
      ) "the public Linux emulator package set does not match config/emulators";
      assert lib.assertMsg (
        productionPackages.default.outPath == productionPackages.steamdeck-runtime.outPath
      ) "the default x86_64-linux package must be the exact Steam Deck runtime";
      assert lib.assertMsg (
        !(productionPackages ? visualAssets)
      ) "the retired camelCase visualAssets package alias was restored";
      assert lib.assertMsg (!(self ? nixosModules)) "the unsupported NixOS module output was restored";
      {
        production_default = "steamdeck-runtime";
        emulator_runtime_packages = expectedRuntimeNames;
      };
    nixContract = architectureContract // {
      package_surface = publicPackageContract;
    };

    repositoryRoot = lib.fileset.toSource {
      root = ../../..;
      fileset = lib.fileset.unions [
        ../../../src
        ../../../config
        ../../../tests
        ../../../packaging
        ../../../docs
        ../../../Makefile
        ../../../flake.nix
        ../../../flake.lock
        ../../../.gitignore
      ];
    };
    # Evaluate declarative manifests directly; the filtered source is reserved
    # for derivations so evaluation never requires import-from-derivation.
    configRoot = ../../../config;
    bezelManifest = lib.importJSON (configRoot + "/assets/bezels.json");
    shaderManifest = lib.importJSON (configRoot + "/assets/shaders.json");
    recipeKeys = lib.attrNames bezelManifest.assets ++ lib.attrNames shaderManifest.assets;
    systemsDir = configRoot + "/systems";
    systemIds = lib.attrNames (
      lib.filterAttrs (
        name: type: type == "directory" && builtins.pathExists (systemsDir + "/${name}/system.json")
      ) (builtins.readDir systemsDir)
    );

    jsonOr = path: if builtins.pathExists path then lib.importJSON path else { };
    referencesOf =
      id:
      let
        bezels = jsonOr (systemsDir + "/${id}/bezels.json");
        shaders = jsonOr (systemsDir + "/${id}/shaders.json");
        widescreen = shaders.widescreen or { };
        variantRefs = lib.concatMap (variant: [
          (variant.art or "")
          (variant.glass or "")
        ]) (bezels.variants or [ ]);
        shaderRefs = [
          (shaders.screen or "")
          (widescreen.screen or "")
        ];
      in
      lib.filter (reference: reference != "") (variantRefs ++ shaderRefs);

    genericFallback = lib.unique (
      lib.mapAttrsToList (_: profile: profile.art) ((bezelManifest.generic or { }).profiles or { })
    );
    referenced = lib.unique (genericFallback ++ lib.concatMap referencesOf systemIds);
    missing = lib.filter (reference: !(lib.elem reference recipeKeys)) referenced;
    notCanonical = lib.filter (reference: !(lib.hasPrefix "assets/" reference)) referenced;

    strictCompile =
      {
        name,
        source,
        run ? null,
        nativeBuildInputs ? [ ],
      }:
      pkgs.runCommand name
        {
          nativeBuildInputs = [
            btrcpy
            pkgs.stdenv.cc
          ]
          ++ nativeBuildInputs;
        }
        ''
          btrcpy=${btrcpy}/bin/btrcpy
          "$btrcpy" ${source} -o program.c \
            --strict-imports --no-cache --no-stdlib
          cc program.c -std=c11 -o program -lm
          ${if run == null then "" else run}
          mkdir -p "$out/bin"
          cp program "$out/bin/${name}"
        '';
  in
  {
    nix-architecture = builtins.deepSeq nixContract (
      pkgs.writeText "semu-nix-architecture.json" (builtins.toJSON nixContract)
    );

    strict-main = strictCompile {
      name = "semu-strict-main";
      source = repositoryRoot + "/src/main.btrc";
    };

    compiler-contracts = strictCompile {
      name = "semu-compiler-contracts";
      source = repositoryRoot + "/tests/compiler/main.btrc";
      nativeBuildInputs = [
        pkgs.libxml2
        pkgs.nix
      ];
      run = ''
        export HOME="$PWD/home"
        export XDG_CACHE_HOME="$PWD/cache"
        mkdir -p "$HOME" "$XDG_CACHE_HOME"
        cp -R ${repositoryRoot} repository
        chmod -R u+w repository
        "$PWD/program" --project "$PWD/repository"
      '';
    };

    assets-verify =
      pkgs.runCommand "semu-assets-verify"
        {
          missing = toString missing;
          notCanonical = toString notCanonical;
          referencedCount = toString (lib.length referenced);
        }
        ''
          status=0
          if [ -n "$missing" ]; then
            echo "references without a bezels.json/shaders.json recipe: $missing" >&2
            status=1
          fi
          if [ -n "$notCanonical" ]; then
            echo "non-canonical asset references: $notCanonical" >&2
            status=1
          fi
          [ "$status" = 0 ] || exit "$status"
          echo "OK: $referencedCount referenced assets have recipes" > "$out"
        '';

    tree-audit =
      pkgs.runCommand "semu-tree-audit"
        {
          nativeBuildInputs = [
            btrcpy
            pkgs.stdenv.cc
            pkgs.gnumake
            pkgs.findutils
            pkgs.gnugrep
            pkgs.gnused
            pkgs.bash
            pkgs.perl
          ];
        }
        ''
          cp -R ${repositoryRoot} repository
          chmod -R u+w repository
          sed "s|/tmp/|$TMPDIR/|g" \
            "$PWD/repository/tests/Makefile" > tree-audit.mk
          make -f "$PWD/tree-audit.mk" -C "$PWD/repository" \
            BTRC_TRANSPILE="${btrcpy}/bin/btrcpy" \
            CC="${pkgs.stdenv.cc}/bin/cc" \
            SHELL="${pkgs.bash}/bin/bash" \
            tree-audit
          touch "$out"
        '';
  }
)
