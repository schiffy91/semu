#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_VERSION="0.1.0"
SEMU_REPO="${SEMU_REPO:-https://github.com/schiffy91/semu.git}"
SEMU_BRANCH="${SEMU_BRANCH:-main}"
PROJECT="${SEMU_PROJECT:-$HOME/semu}"
ROMS="${SEMU_ROMS:-}"
BUILD_MODE="${SEMU_BUILD_MODE:-full}"
ACTION="install"
ASSUME_YES="${SEMU_YES:-0}"
DRY_RUN="${SEMU_DRY_RUN:-0}"
ENABLE_SSH="${SEMU_ENABLE_SSH:-1}"
INSTALL_NIX="${SEMU_INSTALL_NIX:-1}"
FORCE_NOT_DECK="${SEMU_FORCE_NOT_DECK:-0}"
OPEN_SYNC="${SEMU_OPEN_SYNC:-0}"
NIX_PERSIST_ROOT="${SEMU_NIX_PERSIST_ROOT:-/home/.steamos/offload/nix}"

usage() {
  cat <<'EOF'
Semu Steam Deck bootstrap

Usage:
  steam-deck-bootstrap.sh [install|update|status|nix-upgrade|gc] [options]

Actions:
  install       Prepare Nix, clone/update Semu, build, configure Deck defaults.
  update        Pull Semu, rebuild, run Semu upgrade/reconfigure checks.
  status        Print Nix, SSH, Semu, sync, and Deck doctor status.
  nix-upgrade   Upgrade the Nix installation, then restart nix-daemon.
  gc            Run Nix garbage collection.

Options:
  --yes                 Do not prompt before host mutations.
  --dry-run             Print commands without executing mutating steps.
  --project PATH        Semu checkout/config root (default: ~/semu).
  --roms PATH           ROM directory (default: detected microSD path, else project ROMs).
  --repo URL            Git repo to clone (default: https://github.com/schiffy91/semu.git).
  --branch NAME         Branch to checkout (default: main).
  --build full|cli|none Build full emulator bundle, CLI only, or skip Nix build.
  --cli-only            Shortcut for --build cli.
  --no-build            Shortcut for --build none.
  --enable-ssh          Enable sshd for remote verification (default).
  --no-ssh              Do not enable sshd.
  --no-nix-install      Fail instead of installing Nix when nix is missing.
  --nix-persist PATH    Persistent backing directory for /nix on SteamOS.
  --open-sync           Open Syncthing UI after setup.
  --force-not-deck      Allow running on non-Steam Deck Linux hosts.
  --help                Show this help.

Examples:
  curl -fsSL https://raw.githubusercontent.com/schiffy91/semu/main/utils/steam-deck-bootstrap.sh \
    | bash -s -- install --yes

  ~/semu/utils/steam-deck-bootstrap.sh update --roms /run/media/mmcblk0p1/Emulation/ROMs
EOF
}

log() {
  printf ':: %s\n' "$*"
}

warn() {
  printf 'WARN: %s\n' "$*" >&2
}

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

have() {
  command -v "$1" >/dev/null 2>&1
}

run() {
  log "$*"
  if [ "$DRY_RUN" = "1" ]; then
    return 0
  fi
  "$@"
}

sudo_run() {
  log "sudo $*"
  if [ "$DRY_RUN" = "1" ]; then
    return 0
  fi
  sudo "$@"
}

confirm() {
  if [ "$ASSUME_YES" = "1" ]; then
    return 0
  fi
  printf '%s [y/N] ' "$1"
  read -r answer
  case "$answer" in
    y|Y|yes|YES) return 0 ;;
    *) die "aborted" ;;
  esac
}

parse_args() {
  if [ "${1:-}" = "install" ] || [ "${1:-}" = "update" ] || [ "${1:-}" = "status" ] \
    || [ "${1:-}" = "nix-upgrade" ] || [ "${1:-}" = "gc" ]; then
    ACTION="$1"
    shift
  fi

  while [ "$#" -gt 0 ]; do
    case "$1" in
      --yes|-y) ASSUME_YES=1 ;;
      --dry-run) DRY_RUN=1 ;;
      --project) PROJECT="${2:?missing value for --project}"; shift ;;
      --roms) ROMS="${2:?missing value for --roms}"; shift ;;
      --repo) SEMU_REPO="${2:?missing value for --repo}"; shift ;;
      --branch) SEMU_BRANCH="${2:?missing value for --branch}"; shift ;;
      --build) BUILD_MODE="${2:?missing value for --build}"; shift ;;
      --cli-only) BUILD_MODE="cli" ;;
      --no-build) BUILD_MODE="none" ;;
      --enable-ssh) ENABLE_SSH=1 ;;
      --no-ssh) ENABLE_SSH=0 ;;
      --no-nix-install) INSTALL_NIX=0 ;;
      --nix-persist) NIX_PERSIST_ROOT="${2:?missing value for --nix-persist}"; shift ;;
      --open-sync) OPEN_SYNC=1 ;;
      --force-not-deck) FORCE_NOT_DECK=1 ;;
      --help|-h) usage; exit 0 ;;
      *) die "unknown argument: $1" ;;
    esac
    shift
  done

  case "$BUILD_MODE" in
    full|cli|none) ;;
    *) die "--build must be full, cli, or none" ;;
  esac
}

need_sudo() {
  if ! have sudo; then
    die "sudo is required on SteamOS"
  fi
  sudo -v
}

load_nix_profile() {
  if [ -e /nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh ]; then
    # shellcheck disable=SC1091
    . /nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh
  fi
}

is_steam_deck() {
  local product=""
  if [ -r /sys/devices/virtual/dmi/id/product_name ]; then
    product="$(cat /sys/devices/virtual/dmi/id/product_name)"
  fi
  if printf '%s' "$product" | grep -qi 'Jupiter\|Galileo\|Steam Deck'; then
    return 0
  fi
  if [ -r /etc/os-release ] && grep -Eqi '^(ID|VARIANT_ID)=.*(steamos|holo)' /etc/os-release; then
    return 0
  fi
  return 1
}

check_host() {
  if [ "$(uname -s)" != "Linux" ]; then
    die "this script is for Steam Deck/Linux; got $(uname -s)"
  fi
  if ! is_steam_deck && [ "$FORCE_NOT_DECK" != "1" ]; then
    die "host does not look like Steam Deck/SteamOS; pass --force-not-deck to override"
  fi
}

readonly_enabled() {
  have steamos-readonly && steamos-readonly status 2>/dev/null | grep -qi enabled
}

ensure_root_nix_dir() {
  if [ -e /nix ]; then
    return 0
  fi

  local restore_readonly=0
  if readonly_enabled; then
    confirm "SteamOS readonly root must be disabled briefly to create /nix. Continue?"
    sudo_run steamos-readonly disable
    restore_readonly=1
  fi

  sudo_run mkdir -p /nix

  if [ "$restore_readonly" = "1" ]; then
    sudo_run steamos-readonly enable
  fi
}

write_root_file() {
  local path="$1"
  local mode="$2"
  local tmp
  tmp="$(mktemp)"
  cat > "$tmp"
  if [ "$DRY_RUN" = "1" ]; then
    log "would install $path"
    rm -f "$tmp"
    return 0
  fi
  sudo install -D -m "$mode" -o root -g root "$tmp" "$path"
  rm -f "$tmp"
}

prepare_nix_mount() {
  if [ -d /nix/store ] && ! mountpoint -q /nix; then
    warn "existing /nix/store is not a mountpoint; leaving it untouched"
    return 0
  fi

  sudo_run mkdir -p "$NIX_PERSIST_ROOT"
  ensure_root_nix_dir

  if [ -d /nix ] && [ -n "$(find /nix -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null || true)" ] \
    && ! mountpoint -q /nix; then
    warn "/nix is non-empty and not a mountpoint; not bind-mounting over it"
    return 0
  fi

  write_root_file /etc/systemd/system/nix.mount 0644 <<EOF
[Unit]
Description=Persistent Nix store bind mount for SteamOS
RequiresMountsFor=/home
After=local-fs.target

[Mount]
What=$NIX_PERSIST_ROOT
Where=/nix
Type=none
Options=bind

[Install]
WantedBy=multi-user.target
EOF

  sudo_run systemctl daemon-reload
  sudo_run systemctl enable --now nix.mount
  if ! mountpoint -q /nix && [ "$DRY_RUN" != "1" ]; then
    sudo_run mount --bind "$NIX_PERSIST_ROOT" /nix
  fi
}

ensure_nix_user_config() {
  local conf="$HOME/.config/nix/nix.conf"
  run mkdir -p "$HOME/.config/nix"
  if [ ! -e "$conf" ]; then
    run touch "$conf"
  fi
  if ! grep -Eq '^[[:space:]]*experimental-features[[:space:]]*=' "$conf"; then
    log "enable flakes in $conf"
    if [ "$DRY_RUN" != "1" ]; then
      printf '%s\n' 'experimental-features = nix-command flakes' >> "$conf"
    fi
  elif ! grep -Eq '^[[:space:]]*experimental-features[[:space:]]*=.*flakes' "$conf"; then
    warn "$conf has experimental-features but does not mention flakes"
  fi
}

ensure_nix_daemon_config() {
  sudo_run mkdir -p /etc/nix
  if [ "$DRY_RUN" != "1" ]; then
    sudo touch /etc/nix/nix.conf
  fi
  if ! sudo grep -Eq '^[[:space:]]*experimental-features[[:space:]]*=' /etc/nix/nix.conf 2>/dev/null; then
    log "enable daemon flakes in /etc/nix/nix.conf"
    if [ "$DRY_RUN" != "1" ]; then
      printf '%s\n' 'experimental-features = nix-command flakes' | sudo tee -a /etc/nix/nix.conf >/dev/null
    fi
  fi
  if ! sudo grep -Eq '^[[:space:]]*trusted-users[[:space:]]*=' /etc/nix/nix.conf 2>/dev/null; then
    log "trust root and $USER for local flake builds"
    if [ "$DRY_RUN" != "1" ]; then
      printf 'trusted-users = root %s\n' "$USER" | sudo tee -a /etc/nix/nix.conf >/dev/null
    fi
  elif ! sudo grep -Eq "^[[:space:]]*trusted-users[[:space:]]*=.*(^|[[:space:]])$USER([[:space:]]|$)" /etc/nix/nix.conf 2>/dev/null; then
    warn "/etc/nix/nix.conf has trusted-users but does not include $USER"
  fi
}

install_nix_if_needed() {
  load_nix_profile
  if have nix; then
    log "Nix already installed: $(nix --version)"
    return 0
  fi
  if [ "$INSTALL_NIX" != "1" ]; then
    die "nix is missing and --no-nix-install was passed"
  fi

  confirm "Install official multi-user Nix now?"
  local installer
  installer="$(mktemp)"
  run curl --proto '=https' --tlsv1.2 -fsSL https://nixos.org/nix/install -o "$installer"
  if [ "$DRY_RUN" != "1" ]; then
    sh "$installer" --daemon
  else
    log "would run $installer --daemon"
  fi
  rm -f "$installer"
  load_nix_profile
  if have systemctl; then
    sudo_run systemctl enable --now nix-daemon.service
  fi
  have nix || die "nix did not become available; open a new terminal and rerun status"
}

ensure_nix_daemon_mount_dependency() {
  if [ "$DRY_RUN" != "1" ] && ! sudo test -f /etc/systemd/system/nix.mount; then
    return 0
  fi
  if have systemctl && systemctl list-unit-files nix-daemon.service >/dev/null 2>&1; then
    write_root_file /etc/systemd/system/nix-daemon.service.d/semu-nix-mount.conf 0644 <<'EOF'
[Unit]
Requires=nix.mount
After=nix.mount
EOF
    sudo_run systemctl daemon-reload
  fi
}

default_roms_dir() {
  if [ -n "$ROMS" ]; then
    printf '%s\n' "$ROMS"
    return 0
  fi
  local candidate
  for candidate in \
    /run/media/mmcblk0p1/Emulation/ROMs \
    /run/media/deck/*/Emulation/ROMs \
    "$PROJECT/ES-DE/ES-DE/ROMs"; do
    if [ -d "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  printf '%s\n' "$PROJECT/ES-DE/ES-DE/ROMs"
}

download_repo_archive() {
  local tmp archive
  tmp="$(mktemp -d)"
  archive="$tmp/semu.tar.gz"
  run mkdir -p "$PROJECT"
  case "$SEMU_REPO" in
    https://github.com/schiffy91/semu.git|git@github.com:schiffy91/semu.git)
      run curl -fsSL "https://github.com/schiffy91/semu/archive/refs/heads/$SEMU_BRANCH.tar.gz" -o "$archive"
      if [ "$DRY_RUN" != "1" ]; then
        tar -xzf "$archive" --strip-components=1 -C "$PROJECT"
      fi
      ;;
    *)
      die "git is unavailable and archive fallback only supports schiffy91/semu"
      ;;
  esac
  rm -rf "$tmp"
}

ensure_repo() {
  if [ -d "$PROJECT/.git" ]; then
    run git -C "$PROJECT" fetch origin "$SEMU_BRANCH"
    run git -C "$PROJECT" checkout "$SEMU_BRANCH"
    run git -C "$PROJECT" pull --ff-only --autostash origin "$SEMU_BRANCH"
    return 0
  fi

  if [ -e "$PROJECT" ] && [ -n "$(find "$PROJECT" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null || true)" ]; then
    warn "$PROJECT exists and is not a git checkout; using it as-is"
    return 0
  fi

  if have git; then
    run git clone --branch "$SEMU_BRANCH" "$SEMU_REPO" "$PROJECT"
  else
    warn "git not found; downloading GitHub branch archive"
    download_repo_archive
  fi
}

semu_bin() {
  if [ -x "$PROJECT/result/bin/semu" ]; then
    printf '%s\n' "$PROJECT/result/bin/semu"
  elif [ -x "$PROJECT/build/semu" ]; then
    printf '%s\n' "$PROJECT/build/semu"
  else
    return 1
  fi
}

build_semu() {
  if [ "$BUILD_MODE" = "none" ]; then
    semu_bin >/dev/null || die "no Semu binary found and --no-build was passed"
    return 0
  fi

  load_nix_profile
  have nix || die "nix is required to build Semu"
  (
    cd "$PROJECT"
    if [ "$BUILD_MODE" = "cli" ]; then
      run nix --extra-experimental-features "nix-command flakes" build .#semu-cli
    else
      run nix --extra-experimental-features "nix-command flakes" build .#default
    fi
  )
  semu_bin >/dev/null || die "Semu build finished but result/bin/semu is missing"
}

run_semu_setup() {
  local semu roms
  semu="$(semu_bin)"
  roms="$(default_roms_dir)"
  run mkdir -p "$roms"

  run "$semu" deck install --project "$PROJECT" --roms "$roms"
  run "$semu" steam-input install --project "$PROJECT"
  run "$semu" keymap validate --project "$PROJECT"
  run "$semu" screenshot setup --project "$PROJECT"
  run "$semu" screenshot status --project "$PROJECT" || true
  run "$semu" sync status --project "$PROJECT" || true
  run "$semu" doctor --project "$PROJECT"

  if [ "$OPEN_SYNC" = "1" ]; then
    run "$semu" sync open --project "$PROJECT" || true
  fi
}

run_semu_update() {
  local semu roms
  semu="$(semu_bin)"
  roms="$(default_roms_dir)"
  run "$semu" deck upgrade --project "$PROJECT" --roms "$roms"
  run "$semu" steam-input install --project "$PROJECT"
  run "$semu" keymap validate --project "$PROJECT"
  run "$semu" doctor --project "$PROJECT"
}

enable_ssh() {
  if [ "$ENABLE_SSH" != "1" ]; then
    return 0
  fi
  if ! have systemctl; then
    warn "systemctl missing; cannot enable sshd"
    return 0
  fi
  if systemctl list-unit-files sshd.service >/dev/null 2>&1; then
    sudo_run systemctl enable --now sshd.service
  elif systemctl list-unit-files ssh.service >/dev/null 2>&1; then
    sudo_run systemctl enable --now ssh.service
  else
    warn "no sshd.service/ssh.service found"
  fi
  if have passwd && passwd -S "$USER" 2>/dev/null | grep -Eq ' (L|NP) '; then
    warn "set a password with 'passwd' or install an SSH public key before remote login"
  fi
}

print_status() {
  local semu=""
  load_nix_profile
  printf '\nSemu Steam Deck status\n'
  printf '  script: %s\n' "$SCRIPT_VERSION"
  printf '  host: %s %s\n' "$(uname -s)" "$(uname -m)"
  printf '  project: %s\n' "$PROJECT"
  printf '  roms: %s\n' "$(default_roms_dir)"
  if have nix; then
    printf '  nix: %s\n' "$(nix --version)"
  else
    printf '  nix: missing\n'
  fi
  if mountpoint -q /nix; then
    printf '  /nix: mounted\n'
  elif [ -e /nix ]; then
    printf '  /nix: exists, not a mountpoint\n'
  else
    printf '  /nix: missing\n'
  fi
  if semu="$(semu_bin 2>/dev/null)"; then
    printf '  semu: %s\n' "$semu"
  else
    printf '  semu: missing\n'
  fi
  if have systemctl; then
    if systemctl is-active --quiet sshd.service 2>/dev/null || systemctl is-active --quiet ssh.service 2>/dev/null; then
      printf '  ssh: active\n'
    else
      printf '  ssh: inactive\n'
    fi
  fi
  if have hostname; then
    printf '  ip: %s\n' "$(hostname -I 2>/dev/null | xargs || true)"
  fi
  if [ -n "$semu" ]; then
    "$semu" sync status --project "$PROJECT" || true
    "$semu" doctor --project "$PROJECT" || true
  fi
}

upgrade_nix() {
  load_nix_profile
  have nix || die "nix is not installed"
  confirm "Upgrade Nix and restart nix-daemon?"
  if nix --extra-experimental-features "nix-command flakes" help upgrade-nix >/dev/null 2>&1; then
    sudo_run nix --extra-experimental-features "nix-command flakes" upgrade-nix
  else
    sudo_run nix-env --install --file '<nixpkgs>' --attr nix cacert -I nixpkgs=channel:nixpkgs-unstable
  fi
  if have systemctl; then
    sudo_run systemctl daemon-reload
    sudo_run systemctl restart nix-daemon.service
  fi
}

nix_gc() {
  load_nix_profile
  have nix || die "nix is not installed"
  run nix --extra-experimental-features "nix-command flakes" store gc
}

main() {
  parse_args "$@"
  case "$ACTION" in
    status)
      print_status
      ;;
    nix-upgrade)
      need_sudo
      prepare_nix_mount
      ensure_nix_user_config
      ensure_nix_daemon_config
      install_nix_if_needed
      ensure_nix_daemon_mount_dependency
      upgrade_nix
      print_status
      ;;
    gc)
      nix_gc
      ;;
    install|update)
      check_host
      need_sudo
      confirm "This will configure Nix/Semu under $PROJECT and may enable sshd. Continue?"
      prepare_nix_mount
      ensure_nix_user_config
      ensure_nix_daemon_config
      install_nix_if_needed
      ensure_nix_daemon_mount_dependency
      ensure_repo
      build_semu
      if [ "$ACTION" = "install" ]; then
        run_semu_setup
      else
        run_semu_update
      fi
      enable_ssh
      print_status
      ;;
    *)
      die "unknown action: $ACTION"
      ;;
  esac
}

main "$@"
