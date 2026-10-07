# A relocatable release: the bundle's whole Nix closure in a tarball plus a
# bubblewrap launcher that mounts it at /nix on hosts without a Nix store.
# The closure carries Mesa: a host's own GPU drivers (SteamOS keeps them in /usr/lib) are built
# against the host's libc, which the bundle's programs cannot load, so on any host but NixOS the
# launcher points GL, EGL, GBM and Vulkan at the bundled Mesa (what nixGL does).
{ lib, stdenvNoCC, closureInfo, zstd, gnutar, mesa, semu, repositoryRoot, version ? "0.2.0" }:

let
  closure = closureInfo { rootPaths = [ semu mesa ]; };
  launcher = ''
    #!/bin/sh
    # Runs a bundle program inside bubblewrap with this release mounted at /nix.
    set -eu
    # Steam preloads its overlay into every game. It needs the host's libGL.so.1, which the bundle's
    # programs cannot resolve, so the first of them died before ES-DE started, and every process
    # forked here paid for loading it. Game Mode draws Steam's own UI through gamescope without it.
    unset LD_PRELOAD
    here="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd -P)"
    program="$1"; shift
    export SEMU_TARGET="''${SEMU_TARGET:-steam-deck}"
    binds=""
    for entry in /usr /etc /home /run /var /opt /tmp /mnt /media /root /srv /boot /lib /lib64 /bin /sbin; do
      [ -e "$entry" ] && binds="$binds --bind $entry $entry"
    done
    store=""
    for path in "$here"/nix/store/*; do store="$store --ro-bind $path /nix/store/''${path##*/}"; done
    if [ -d /run/opengl-driver ] && command -v nix-store >/dev/null 2>&1; then  # NixOS host: its GPU drivers live in its own store
      for path in $(nix-store -qR /run/opengl-driver 2>/dev/null); do store="$store --ro-bind $path $path"; done
    else  # any other host: the bundled Mesa (lavapipe left out, so Vulkan programs pick the real GPU)
      export LIBGL_DRIVERS_PATH="${mesa}/lib/dri" LIBVA_DRIVERS_PATH="${mesa}/lib/dri" GBM_BACKENDS_PATH="${mesa}/lib/gbm"
      export __EGL_VENDOR_LIBRARY_DIRS="${mesa}/share/glvnd/egl_vendor.d" __GLX_VENDOR_LIBRARY_NAME=mesa
      export LD_LIBRARY_PATH="${mesa}/lib"  # replaces Steam's runtime paths, whose older libraries would shadow the bundle's
      icds=""
      for icd in "$here"${mesa}/share/vulkan/icd.d/*.json; do  # listed from the release, used at /nix inside the sandbox
        case "$icd" in *lvp_icd*|*'*'*) continue ;; esac
        icds="''${icds:+$icds:}${mesa}/share/vulkan/icd.d/$(basename "$icd")"
      done
      export VK_DRIVER_FILES="$icds" VK_ICD_FILENAMES="$icds"
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
  # Everything of a release except its store: launchers, installer, version, and the names of the
  # store paths it needs. A delta deploy builds only this and sends just the paths a Deck lacks.
  tree = stdenvNoCC.mkDerivation {
    pname = "semu-release-tree";
    inherit version;
    dontUnpack = true;
    buildPhase = ''
      mkdir -p stage/bin
      cp ${../LICENSES.md} stage/LICENSES.md
      cp ${../../LICENSE} stage/LICENSE
      printf '%s\n' 'Personal installation only; not cleared for redistribution. See LICENSES.md.' > stage/DISTRIBUTION.txt
      cat > stage/bin/semu-deck-run <<'RUN'
      ${launcher}
      RUN
      chmod +x stage/bin/semu-deck-run
      ${shim "semu-deck" "semu-es-de"}
      ${shim "semu-deck-cli" "semu"}
      install -Dm755 ${repositoryRoot + "/packaging/deck/install.sh"} stage/install.sh
      printf '%s\n' "${version}" > stage/VERSION
      sed 's|.*/||' ${closure}/store-paths | sort > stage/store-paths
    '';
    installPhase = "cp -a stage $out";
    dontFixup = true;  # patchShebangs would point the launchers at a store shell the host does not have
    doInstallCheck = true;
    installCheckPhase = ''
      for launcher in $out/bin/*; do
        [ "$(head -1 "$launcher")" = "#!/bin/sh" ] || { echo "$launcher must start with #!/bin/sh: it runs on the host"; exit 1; }
      done
    '';
  };
in
stdenvNoCC.mkDerivation {
  pname = "semu-release";
  inherit version;
  dontUnpack = true;
  nativeBuildInputs = [ zstd gnutar ];

  buildPhase = ''
    cp -a ${tree} stage
    chmod -R u+w stage
    mkdir -p stage/nix/store
    while read -r path; do
      cp -a "$path" stage/nix/store/
    done < ${closure}/store-paths
    chmod -R u+w stage
    tar --sort=name --mtime='@1' --owner=0 --group=0 --numeric-owner -C stage -cf - . | zstd -T0 -19 -o Semu-x86_64.tar.zst
  '';

  installPhase = ''
    mkdir -p "$out"
    cp Semu-x86_64.tar.zst "$out/"
    (cd "$out" && sha256sum Semu-x86_64.tar.zst > Semu-x86_64.tar.zst.sha256)
    cp ${repositoryRoot + "/packaging/deck/install.sh"} "$out/install.sh"
  '';

  passthru = { inherit tree; redistributable = false; };
  meta.description = "Semu personal-installation bundle for the Steam Deck";
}
