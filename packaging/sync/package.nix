{ lib, writeTextDir }:

(writeTextDir
  "share/semu/sync/semu-syncthing.service.template"
  (builtins.readFile ./semu-syncthing.service.template)).overrideAttrs {
  pname = "semu-syncthing-template";
  version = "1";
  meta = {
    description = "Semu Syncthing hardened user-service template";
    license = lib.licenses.mit;
    platforms = lib.platforms.all;
  };
}
