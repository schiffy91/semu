# A relocatable release: the bundle's whole Nix closure in a tarball plus a
# bubblewrap launcher that mounts it at /nix on hosts without a Nix store.
{ lib, stdenvNoCC, closureInfo, zstd, gnutar, semu, repositoryRoot, version ? "0.2.0" }:

let
  closure = closureInfo { rootPaths = [ semu ]; };
  launcher = ''
    #!/bin/sh
    # Runs a bundle program inside bubblewrap with this release mounted at /nix.
    set -eu
    here="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd -P)"
    program="$1"; shift
    export SEMU_TARGET="''${SEMU_TARGET:-steam-deck}"
    binds=""
    for entry in /usr /etc /home /run /var /opt /tmp /mnt /media /root /srv /boot /lib /lib64 /bin /sbin; do
      [ -e "$entry" ] && binds="$binds --bind $entry $entry"
    done
    store=""
    for path in "$here"/nix/store/*; do store="$store --ro-bind $path /nix/store/$(basename "$path")"; done
    if [ -d /run/opengl-driver ] && command -v nix-store >/dev/null 2>&1; then  # NixOS host: its GPU drivers live in its own store
      for path in $(nix-store -qR /run/opengl-driver 2>/dev/null); do store="$store --ro-bind $path $path"; done
    fi
    bwrap --tmpfs / --dev-bind /dev /dev --proc /proc --bind /sys /sys $binds \
      --tmpfs /nix $store --die-with-parent -- "${semu}/bin/$program" "$@" &
    sandbox=$!
    trap 'pkill -TERM -P "$sandbox" 2>/dev/null; wait "$sandbox"' TERM INT HUP  # bwrap does not forward signals
    wait "$sandbox"
  '';
  shim = name: program: ''
    cat > "stage/bin/${name}" <<'SHIM'
    #!/bin/sh
    exec "$(dirname "$(readlink -f "$0")")/semu-deck-run" ${program} "$@"
    SHIM
    chmod +x "stage/bin/${name}"
  '';
in
stdenvNoCC.mkDerivation {
  pname = "semu-release";
  inherit version;
  dontUnpack = true;
  nativeBuildInputs = [ zstd gnutar ];

  buildPhase = ''
    mkdir -p stage/bin stage/nix/store
    while read -r path; do
      cp -a "$path" stage/nix/store/
    done < ${closure}/store-paths
    cat > stage/bin/semu-deck-run <<'RUN'
    ${launcher}
    RUN
    chmod +x stage/bin/semu-deck-run
    ${shim "semu-deck" "semu-es-de"}
    ${shim "semu-deck-cli" "semu"}
    install -Dm755 ${repositoryRoot + "/packaging/deck/install.sh"} stage/install.sh
    printf '%s\n' "${version}" > stage/VERSION
    chmod -R u+w stage
    tar --sort=name --mtime='@1' --owner=0 --group=0 --numeric-owner -C stage -cf - . | zstd -T0 -19 -o Semu-x86_64.tar.zst
  '';

  installPhase = ''
    mkdir -p "$out"
    cp Semu-x86_64.tar.zst "$out/"
    (cd "$out" && sha256sum Semu-x86_64.tar.zst > Semu-x86_64.tar.zst.sha256)
    cp ${repositoryRoot + "/packaging/deck/install.sh"} "$out/install.sh"
  '';

  meta.description = "Semu relocatable release for the Steam Deck";
}
