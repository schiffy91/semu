# The semu CLI: the compiled program plus its configuration payload.
{ lib, symlinkJoin, makeWrapper, semuProgram, semuSource }:

symlinkJoin {
  name = "semu-cli";
  paths = [ semuProgram semuSource ];
  nativeBuildInputs = [ makeWrapper ];

  postBuild = ''
    mkdir -p "$out/bin"
    makeWrapper "$out/lib/semu/semu-btrc" "$out/bin/semu" \
      --set SEMU_SOURCE_ROOT "$out/share/semu/config"
  '';

  meta = {
    description = "Semu CLI";
    license = lib.licenses.mit;
    mainProgram = "semu";
  };
}
