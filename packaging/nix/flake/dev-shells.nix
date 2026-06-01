{ pkgs, btrc, system }:

{
  default = pkgs.mkShell {
    packages = [
      btrc.packages.${system}.btrcpy
      pkgs.bash
      pkgs.curl
      pkgs.git
      pkgs.gnumake
      pkgs.jq
      pkgs.ripgrep
      pkgs.stdenv.cc
    ];

    shellHook = ''
      export SEMU_BTRCPY="${btrc.packages.${system}.btrcpy}/bin/btrcpy"
    '';
  };
}
