#!/usr/bin/env bash
set -Eeuo pipefail

SEMU_REPO="${SEMU_REPO:-https://github.com/schiffy91/semu.git}"
SEMU_BRANCH="${SEMU_BRANCH:-main}"
SOURCE="${SEMU_SOURCE:-$HOME/semu}"
PROJECT="${SEMU_PROJECT:-$HOME/.local/share/semu}"
TARGET="${SEMU_TARGET:-steam-deck}"
ROMS="${SEMU_ROMS:-}"
BUILD_MODE="${SEMU_BUILD_MODE:-full}"
ACTION="install"
ASSUME_YES="${SEMU_YES:-0}"
ENABLE_SSH="${SEMU_ENABLE_SSH:-1}"
INSTALL_NIX="${SEMU_INSTALL_NIX:-1}"

usage() {
  cat <<'EOF'
Semu Steam Deck compiler bootstrap

Usage:
  steam-deck-bootstrap.sh [install|update|status|gc] [options]

Options:
  --yes                 Do not prompt before host mutations.
  --source PATH         Semu source checkout (default: ~/semu).
  --project PATH        Semu project/data root (default: ~/.local/share/semu).
  --target NAME         Compiler target to build/verify (default: steam-deck).
  --roms PATH           ROM directory hint exported for compiler/generator use.
  --repo URL            Git repo to clone (default: https://github.com/schiffy91/semu.git).
  --branch NAME         Branch to checkout (default: main).
  --build full|cli|none Build full flake, CLI only, or skip Nix build.
  --cli-only            Shortcut for --build cli.
  --no-build            Shortcut for --build none.
  --enable-ssh          Enable sshd for remote verification (default).
  --no-ssh              Do not enable sshd.
  --no-nix-install      Fail instead of installing Nix when missing.
  --open-sync           Accepted for old invocations; no-op in compiler mode.
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
      --source|--checkout) SOURCE="${2:?missing value for --source}"; shift ;;
      --project) PROJECT="${2:?missing value for --project}"; shift ;;
      --target) TARGET="${2:?missing value for --target}"; shift ;;
      --roms) ROMS="${2:?missing value for --roms}"; shift ;;
      --repo) SEMU_REPO="${2:?missing value for --repo}"; shift ;;
      --branch) SEMU_BRANCH="${2:?missing value for --branch}"; shift ;;
      --build) BUILD_MODE="${2:?missing value for --build}"; shift ;;
      --cli-only) BUILD_MODE=cli ;;
      --no-build) BUILD_MODE=none ;;
      --enable-ssh) ENABLE_SSH=1 ;;
      --no-ssh) ENABLE_SSH=0 ;;
      --no-nix-install) INSTALL_NIX=0 ;;
      --open-sync) warn '--open-sync is ignored by the compiler bootstrap' ;;
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
    if have systemctl; then
      sudo systemctl daemon-reload || true
      sudo systemctl enable --now nix-daemon.service || true
    fi
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
  have git || die 'git is required'
  if [ -d "$SOURCE/.git" ]; then
    log "update repo: $SOURCE"
    git -C "$SOURCE" fetch origin "$SEMU_BRANCH"
    git -C "$SOURCE" checkout "$SEMU_BRANCH"
    git -C "$SOURCE" pull --ff-only --autostash origin "$SEMU_BRANCH"
  elif [ -e "$SOURCE" ] && [ -n "$(find "$SOURCE" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null || true)" ]; then
    warn "$SOURCE exists and is not empty; using it as-is"
  else
    log "clone repo: $SEMU_REPO#$SEMU_BRANCH"
    git clone --branch "$SEMU_BRANCH" "$SEMU_REPO" "$SOURCE"
  fi
}

build_semu() {
  [ "$BUILD_MODE" = "none" ] && return 0
  ensure_nix
  (
    cd "$SOURCE"
    log 'build BTRC CLI'
    nix develop --command make btrc-build
    if [ "$BUILD_MODE" = "full" ]; then
      log 'build Semu flake default'
      nix build --impure .#default
    else
      log 'build Semu CLI package'
      nix build .#semu-cli
    fi
  )
}

semu_bin() {
  for candidate in "$SOURCE/result/bin/semu" "$SOURCE/build/out/semu"; do
    [ -x "$candidate" ] && { printf '%s\n' "$candidate"; return 0; }
  done
  die "Semu binary not found under $SOURCE; rerun with --build full or --cli-only"
}

run_semu() {
  local semu
  semu="$(semu_bin)"
  SEMU_ASSET_ROOT="$SOURCE" \
  SEMU_PROJECT_DIR="$PROJECT" \
  SEMU_ROMS="$ROMS" \
  SEMU_ROMS_DIR="$ROMS" \
    "$semu" "$@"
}

run_build() {
  mkdir -p "$PROJECT"
  log "compiler build: target=$TARGET project=$PROJECT source=$SOURCE"
  run_semu build target "$TARGET" --project "$PROJECT"
}

run_verify() {
  log "compiler verify: target=$TARGET project=$PROJECT source=$SOURCE"
  run_semu verify target "$TARGET" --project "$PROJECT"
}

main() {
  parse_args "$@"
  [ "$(uname -s)" = Linux ] || die 'this bootstrap is for Steam Deck/Linux hosts'
  ensure_ssh
  ensure_repo
  case "$ACTION" in
    install|update) build_semu; run_build; run_verify ;;
    status) run_verify ;;
    gc) ensure_nix; nix-collect-garbage -d ;;
  esac
}

main "$@"
