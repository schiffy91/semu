# RetroArch with exactly the cores the linux system bindings select (see packaging/nix/retroarch.nix).
{ callPackage }: callPackage ../../../packaging/nix/retroarch.nix { repositoryRoot = ../../..; }
