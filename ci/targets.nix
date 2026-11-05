# What CI and `make cache` build, and where the cache lives. Adding a system or an
# attribute here is the whole change: the Makefile and both workflows read this file.
{
  cache = {
    name = "semu-cache-1";  # the signing key's name; its secret never leaves the machine that signs
    publicKey = "semu-cache-1:GAvdDO5fCGcTJsr1yyVLgnVn7TTyFYnfEkb7fd2zUew=";
    cachix = "";  # a Cachix cache name; CI pushes there only when this and CACHIX_AUTH_TOKEN are set
  };

  # Each entry becomes one CI job per attribute, so a job's disk holds one closure.
  # GitHub's hosted runners give ~14 GB free; the composed bundle does not fit, which is
  # why `semu` and `release` are built on a machine of your own (make cache) instead.
  systems = [
    {
      system = "x86_64-linux";
      runner = "ubuntu-24.04";
      attrs = [ "semu-cli" "semu-renderer" "es-de" "retroarch" "bezel-layers" "visual-assets" ];
    }
    {
      system = "aarch64-darwin";
      runner = "macos-15";  # Apple Silicon, free for public repositories
      attrs = [ "semu-cli" "bezel-layers" "visual-assets" ];
    }
  ];

  # Built and attached to a GitHub Release on a v* tag, from the x86_64-linux job.
  release = {
    system = "x86_64-linux";
    attr = "release";
    files = [ "Semu-x86_64.tar.zst" "Semu-x86_64.tar.zst.sha256" "install.sh" ];
  };

  # What a machine of your own should build and cache for the others: the whole product.
  local = [ "semu" "release" ];
}
