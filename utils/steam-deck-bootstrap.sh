#!/usr/bin/env bash
set -Eeuo pipefail

SEMU_REPO="${SEMU_REPO:-https://github.com/schiffy91/semu.git}"
SEMU_BRANCH="${SEMU_BRANCH:-main}"
PROJECT="${SEMU_PROJECT:-$HOME/semu}"
ROMS="${SEMU_ROMS:-}"
BUILD_MODE="${SEMU_BUILD_MODE:-full}"
ACTION="install"
ASSUME_YES="${SEMU_YES:-0}"
ENABLE_SSH="${SEMU_ENABLE_SSH:-1}"
INSTALL_NIX="${SEMU_INSTALL_NIX:-1}"
OPEN_SYNC="${SEMU_OPEN_SYNC:-0}"

usage() {
  cat <<'EOF'
Semu Steam Deck bootstrap

Usage:
  steam-deck-bootstrap.sh [install|update|status|gc] [options]

Options:
  --yes                 Do not prompt before host mutations.
  --project PATH        Semu checkout/config root (default: ~/semu).
  --roms PATH           ROM directory (default: detected SD card, else project ROMs).
  --repo URL            Git repo to clone (default: https://github.com/schiffy91/semu.git).
  --branch NAME         Branch to checkout (default: main).
  --build full|cli|none Build full emulator bundle, CLI only, or skip Nix build.
  --cli-only            Shortcut for --build cli.
  --no-build            Shortcut for --build none.
  --enable-ssh          Enable sshd for remote verification (default).
  --no-ssh              Do not enable sshd.
  --no-nix-install      Fail instead of installing Nix when missing.
  --open-sync           Open Syncthing UI after setup/status.
  --help                Show this help.

Example:
  curl -fsSL https://raw.githubusercontent.com/schiffy91/semu/refs/heads/main/utils/steam-deck-bootstrap.sh \
    | SEMU_ROMS=/run/media/deck/SD bash -s -- install --yes
EOF
}

log() { printf ':: %s\n' "$*"; }
warn() { printf 'WARN: %s\n' "$*" >&2; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

confirm() {
  [ "$ASSUME_YES" = "1" ] && return 0
  printf '%s [y/N] ' "$1"
  read -r answer
  case "$answer" in y|Y|yes|YES) ;; *) die 'aborted' ;; esac
}

parse_args() {
  case "${1:-}" in install|update|status|gc) ACTION="$1"; shift ;; esac
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --yes|-y) ASSUME_YES=1 ;;
      --project) PROJECT="${2:?missing value for --project}"; shift ;;
      --roms) ROMS="${2:?missing value for --roms}"; shift ;;
      --repo) SEMU_REPO="${2:?missing value for --repo}"; shift ;;
      --branch) SEMU_BRANCH="${2:?missing value for --branch}"; shift ;;
      --build) BUILD_MODE="${2:?missing value for --build}"; shift ;;
      --cli-only) BUILD_MODE=cli ;;
      --no-build) BUILD_MODE=none ;;
      --enable-ssh) ENABLE_SSH=1 ;;
      --no-ssh) ENABLE_SSH=0 ;;
      --no-nix-install) INSTALL_NIX=0 ;;
      --open-sync) OPEN_SYNC=1 ;;
      --help|-h) usage; exit 0 ;;
      *) die "unknown argument: $1" ;;
    esac
    shift
  done
  case "$BUILD_MODE" in full|cli|none) ;; *) die '--build must be full, cli, or none' ;; esac
}

load_nix_profile() {
  if [ -e /nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh ]; then
    # shellcheck disable=SC1091
    . /nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh
  fi
}

is_steamos() {
  [ -d /home/.steamos ] || grep -qi 'steamos' /etc/os-release 2>/dev/null
}

ensure_steamos_nix_mount() {
  is_steamos || return 0
  have sudo || die 'sudo is required to prepare the persistent SteamOS Nix mount'

  local source=/home/.steamos/offload/nix
  sudo mkdir -p "$source" /nix

  if ! mountpoint -q /nix && [ -n "$(find /nix -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null || true)" ]; then
    warn '/nix is not a mountpoint and is not empty; leaving the existing Nix store in place'
    return 0
  fi

  sudo tee /etc/systemd/system/nix.mount >/dev/null <<EOF
[Unit]
Description=Persistent Nix store bind mount for SteamOS
RequiresMountsFor=/home

[Mount]
What=$source
Where=/nix
Type=none
Options=bind

[Install]
WantedBy=multi-user.target
EOF

  sudo systemctl daemon-reload || true
  sudo systemctl enable nix.mount >/dev/null 2>&1 || true
  mountpoint -q /nix || sudo mount --bind "$source" /nix
}

ensure_nix() {
  ensure_steamos_nix_mount
  load_nix_profile
  if have nix; then
    log "Nix: $(nix --version)"
    return 0
  fi
  [ "$INSTALL_NIX" = "1" ] || die 'nix is missing and --no-nix-install was passed'
  confirm 'Install Nix using the Determinate Systems installer?'
  curl --proto '=https' --tlsv1.2 -fsSL https://install.determinate.systems/nix \
    | sh -s -- install --determinate --no-confirm
  load_nix_profile
  have systemctl && sudo systemctl enable --now nix-daemon.service || true
  have nix || die 'nix did not become available; open a new shell and rerun status'
}

ensure_ssh() {
  [ "$ENABLE_SSH" = "1" ] || return 0
  if have systemctl; then
    log 'enable sshd'
    sudo systemctl enable --now sshd.service || sudo systemctl enable --now ssh.service || true
  fi
}

ensure_repo() {
  if [ -d "$PROJECT/.git" ]; then
    log "update repo: $PROJECT"
    git -C "$PROJECT" fetch origin "$SEMU_BRANCH"
    git -C "$PROJECT" checkout "$SEMU_BRANCH"
    git -C "$PROJECT" pull --ff-only --autostash origin "$SEMU_BRANCH"
  elif [ -e "$PROJECT" ] && [ -n "$(find "$PROJECT" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null || true)" ]; then
    warn "$PROJECT exists and is not empty; using it as-is"
  else
    log "clone repo: $SEMU_REPO#$SEMU_BRANCH"
    git clone --branch "$SEMU_BRANCH" "$SEMU_REPO" "$PROJECT"
  fi
}

default_roms_dir() {
  [ -n "$ROMS" ] && { normalize_roms_dir "$ROMS"; return; }
  local candidate
  for candidate in \
    /run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs \
    /mnt/SD/Emulation/ES-DE/ES-DE/ROMs \
    /run/media/deck/SD \
    /mnt/SD \
    /run/media/deck/*/Emulation/ROMs \
    /run/media/deck/*/Emulation/ES-DE/ES-DE/ROMs \
    /run/media/deck/*/ROMs \
    "$PROJECT/ES-DE/ES-DE/ROMs"; do
    [ -d "$candidate" ] && { printf '%s\n' "$candidate"; return; }
  done
  printf '%s\n' "$PROJECT/ES-DE/ES-DE/ROMs"
}

normalize_roms_dir() {
  local root="$1"
  local candidate
  for candidate in \
    "$root/Emulation/ES-DE/ES-DE/ROMs" \
    "$root/ES-DE/ES-DE/ROMs" \
    "$root/Emulation/ROMs" \
    "$root/ROMs"; do
    [ -d "$candidate" ] && { printf '%s\n' "$candidate"; return; }
  done
  printf '%s\n' "$root"
}

build_semu() {
  [ "$BUILD_MODE" = "none" ] && return 0
  ensure_nix
  (
    cd "$PROJECT"
    log 'build BTRC CLI'
    nix develop --command make btrc-build
    if [ "$BUILD_MODE" = "full" ]; then
      log 'build full Semu bundle'
      nix build .#default
    else
      log 'build Semu CLI package'
      nix build .#semu-cli
    fi
  )
}

semu_bin() {
  for candidate in "$PROJECT/result/bin/semu" "$PROJECT/build/semu"; do
    [ -x "$candidate" ] && { printf '%s\n' "$candidate"; return 0; }
  done
  die "Semu binary not found under $PROJECT; rerun with --build full or --cli-only"
}

run_install() {
  local semu roms
  semu="$(semu_bin)"
  roms="$(default_roms_dir)"
  log "configure Semu project: $PROJECT"
  log "ROMs: $roms"
  "$semu" lifecycle install --project "$PROJECT" --roms "$roms"
  "$semu" sync setup --project "$PROJECT"
  "$semu" sync autostart enable --project "$PROJECT" || true
  "$semu" steam-input install --project "$PROJECT" || true
}

run_status() {
  local semu
  semu="$(semu_bin)"
  "$semu" config show --project "$PROJECT"
  "$semu" doctor --project "$PROJECT"
  "$semu" sync status --project "$PROJECT"
  [ "$OPEN_SYNC" = "1" ] && "$semu" sync open --project "$PROJECT" || true
}

main() {
  parse_args "$@"
  [ "$(uname -s)" = Linux ] || die 'this bootstrap is for Steam Deck/Linux hosts'
  ensure_ssh
  ensure_repo
  case "$ACTION" in
    install) build_semu; run_install; run_status ;;
    update) build_semu; "$(semu_bin)" lifecycle upgrade --project "$PROJECT"; run_install; run_status ;;
    status) run_status ;;
    gc) ensure_nix; nix-collect-garbage -d ;;
  esac
}

main "$@"
