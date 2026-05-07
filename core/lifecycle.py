"""High-level install / update / uninstall / rollback operations.

Each function emits log lines via core.console so the GUI can stream them.
The CLI shim in setup.py just prints them; the GUI workers route them to the
log panel.

Lifecycle invariants:
  - `install` must be idempotent: re-running with no changes does nothing.
  - `update` must keep the previous build available as a one-shot rollback target.
  - `uninstall` must NEVER delete project-dir data — only the OS-level wiring
    (symlinks, Flatpak overrides). This is what protects users sharing a
    cloud-synced project dir.
  - `rollback` must succeed even if no captured original exists; it falls back
    to "swap result symlinks only" rather than failing the whole operation.
"""

import argparse
import os
import shlex
import shutil
import subprocess

from core import flatpak, state
from core.backup import cmd_backup, cmd_capture
from core.console import console_error, console_log
from core.exec import delete
from core.symlinks import (
    create_symlinks,
    parse_config,
    filter_emulators,
    find_emulator_dir,
    resolve_config,
)


# Environment variables we scrub before running `nix build`. Schemulator
# inherits the GUI/CLI parent's full env; cleaning known-credential vars
# before launching a long-running subprocess prevents accidental leakage
# into nix logs / build sandbox env (round-5 critic finding #4).
_CREDENTIAL_VARS = (
    "GITHUB_TOKEN", "GH_TOKEN",
    "AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY", "AWS_SESSION_TOKEN",
    "GOOGLE_APPLICATION_CREDENTIALS",
    "ANTHROPIC_API_KEY", "OPENAI_API_KEY",
    "DOCKER_AUTH_CONFIG",
    "NPM_TOKEN", "HF_TOKEN",
)


def _safe_env() -> dict:
    """Return a copy of os.environ with credential-shaped vars removed."""
    env = os.environ.copy()
    for k in _CREDENTIAL_VARS:
        env.pop(k, None)
    return env


def _nix_build_target(emulator: str) -> str:
    """Map an emulator name onto its `nix build` flake attribute."""
    return emulator.lower()


def _result_dir(project_dir: str, emulator: str) -> str:
    return os.path.join(project_dir, f"result-{emulator.lower()}")


def _previous_result(project_dir: str, emulator: str) -> str:
    """Return the previous `result-<emulator>-prev` symlink path used for rollback."""
    return os.path.join(project_dir, f"result-{emulator.lower()}-prev")


def _nix_available() -> bool:
    return shutil.which("nix") is not None


def _nix_build(emulator: str, project_dir: str) -> bool:
    """Run `nix build .#<emulator> --out-link result-<emulator>`. Returns True on success."""
    if not _nix_available():
        console_error("nix not found on PATH; install Nix or use the native installer")
        return False
    target = _nix_build_target(emulator)
    out_link = _result_dir(project_dir, emulator)
    cmd = ["nix", "build", f"{project_dir}#{target}", "--out-link", out_link]
    # shlex.join produces a faithful shell representation; the previous
    # ' '.join could mislead during incident triage if any arg had spaces
    # or shell metas (round-5 critic finding #5).
    console_log(f"$ {shlex.join(cmd)}")
    if state.DRY_RUN:
        return True
    try:
        result = subprocess.run(cmd, check=False, env=_safe_env())
        return result.returncode == 0
    except FileNotFoundError:
        console_error("nix not found on PATH; install Nix or use the native installer")
        return False


def _capture_original_if_first(emulator: str, project_dir: str, config_file: str) -> None:
    """On the very first install of an emulator, snapshot its config as 'baseline'
    so rollback always has something to revert to."""
    emu_dir = find_emulator_dir(project_dir, emulator)
    if not emu_dir:
        return
    originals_dir = os.path.join(project_dir, emu_dir, "originals")
    if os.path.isdir(originals_dir) and os.path.exists(os.path.join(originals_dir, "manifest.json")):
        return  # already has captured originals
    try:
        cmd_capture(argparse.Namespace(
            config=config_file,
            emulator=emulator,
            version="baseline",
        ))
    except Exception as e:
        console_error(f"Could not capture baseline for {emulator}: {e}")


def install(args) -> int:
    """Build, link, and capture-baseline for the listed emulators (or all). Returns
    the number of emulators successfully installed."""
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators or [])
    if not emulators:
        console_error("No emulators matched")
        return 0
    succeeded = 0
    for emulator, entries in emulators.items():
        console_log(f"\n=== Installing {emulator} ===")
        if not _nix_build(emulator, project_dir):
            console_error(f"nix build failed for {emulator}")
            continue
        for flatpak_id, link_path, source_path in entries:
            flatpak.setup_flatpak(flatpak_id, source_path, project_dir=project_dir)
            create_symlinks(link_path, source_path)
        _capture_original_if_first(emulator, project_dir, config_file)
        succeeded += 1
    console_log(f"\nInstalled {succeeded}/{len(emulators)} emulators.")
    return succeeded


def _staging_result(project_dir: str, emulator: str) -> str:
    return os.path.join(project_dir, f"result-{emulator.lower()}-staging")


def detect_interrupted_updates(project_dir: str) -> list:
    """Find emulators where an update appears to have been interrupted.

    Failure mode: between `os.rename(old, prev)` and `os.rename(staging, old)`
    a power loss or SIGKILL can leave us with a `-staging` and `-prev` link
    but no current `result-<emu>` link. Without recovery the GUI shows the
    emulator as not-installed even though both binaries are intact (round-2
    critic finding #5).

    Returns a list of emulator names (lowercase) that need recovery.
    """
    if not os.path.isdir(project_dir):
        return []
    pending = []
    for entry in os.listdir(project_dir):
        if not entry.startswith("result-") or not entry.endswith("-staging"):
            continue
        emu = entry[len("result-"):-len("-staging")]
        current = _result_dir(project_dir, emu)
        if not os.path.lexists(current):
            pending.append(emu)
    return pending


def recover_interrupted_update(project_dir: str, emulator: str) -> bool:
    """Complete an interrupted update by promoting the staging build to
    current. Safe to call only if `result-<emu>-staging` exists and
    `result-<emu>` does not — caller must verify (use detect_interrupted_updates).
    Returns True if recovery succeeded.
    """
    current = _result_dir(project_dir, emulator)
    staging = _staging_result(project_dir, emulator)
    if os.path.lexists(current):
        console_error(f"Won't recover {emulator}: result-{emulator} already exists")
        return False
    if not os.path.islink(staging):
        console_error(f"Won't recover {emulator}: no staging link present")
        return False
    os.rename(staging, current)
    console_log(f"Recovered interrupted update for {emulator} (staging -> current).")
    return True


def _nix_build_to(emulator: str, project_dir: str, out_link: str) -> bool:
    """Build to a specific out-link path. Used by update() to stage the new
    build to a temp path before atomically swapping it into place."""
    if not _nix_available():
        console_error("nix not found on PATH; install Nix or use the native installer")
        return False
    target = _nix_build_target(emulator)
    cmd = ["nix", "build", f"{project_dir}#{target}", "--out-link", out_link]
    console_log(f"$ {shlex.join(cmd)}")
    if state.DRY_RUN:
        return True
    try:
        result = subprocess.run(cmd, check=False, env=_safe_env())
        return result.returncode == 0
    except FileNotFoundError:
        console_error("nix not found on PATH; install Nix or use the native installer")
        return False


def update(args) -> int:
    """Update emulators with prev-build retention for rollback.

    Order of operations is critical for atomicity:
      1. Build the new version to `result-<emu>-staging` (the user's existing
         `result-<emu>` keeps working throughout this step — they can launch
         the emulator while the update downloads).
      2. Move the current `result-<emu>` to `result-<emu>-prev`.
      3. Move the staged build into place at `result-<emu>`.
      4. Wire symlinks.

    On any failure the original `result-<emu>` is preserved untouched.
    """
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators or [])
    if not emulators:
        console_error("No emulators matched")
        return 0

    console_log("Backing up before update...")
    cmd_backup(argparse.Namespace(config=config_file, emulators=list(emulators.keys())))

    succeeded = 0
    for emulator, entries in emulators.items():
        console_log(f"\n=== Updating {emulator} ===")
        old_link = _result_dir(project_dir, emulator)
        prev_link = _previous_result(project_dir, emulator)
        staging_link = _staging_result(project_dir, emulator)

        # Defensive: if `prev` exists as a real directory (not a symlink),
        # something else dropped it there. Refuse to clobber.
        if os.path.lexists(prev_link) and not os.path.islink(prev_link):
            console_error(f"Refusing to overwrite non-symlink at {prev_link}")
            continue

        # Step 1: build to staging. Old result keeps working.
        if os.path.lexists(staging_link):
            os.unlink(staging_link)
        if not _nix_build_to(emulator, project_dir, staging_link):
            console_error(f"Update build failed for {emulator}; current install untouched")
            if os.path.lexists(staging_link):
                try:
                    os.unlink(staging_link)
                except OSError:
                    pass
            continue

        # Step 2: rotate prev. The old result becomes the rollback target.
        if os.path.islink(old_link):
            if os.path.lexists(prev_link):
                os.unlink(prev_link)
            os.rename(old_link, prev_link)

        # Step 3: atomic swap-in.
        os.rename(staging_link, old_link)

        # Step 4: re-wire symlinks (configs may have moved within result/).
        for flatpak_id, link_path, source_path in entries:
            flatpak.setup_flatpak(flatpak_id, source_path, project_dir=project_dir)
            create_symlinks(link_path, source_path)
        succeeded += 1

    console_log(f"\nUpdated {succeeded}/{len(emulators)} emulators.")
    return succeeded


def uninstall(args) -> int:
    """Remove host symlinks and Flatpak overrides. Project-dir data is preserved.

    Note: this intentionally does NOT remove `result-<emulator>` symlinks. Those
    are how nix tracks the installed binary; removing them would orphan the
    build. Use `nix gc` for cleanup.
    """
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators or [])
    if not emulators:
        console_error("No emulators matched")
        return 0
    succeeded = 0
    for emulator, entries in emulators.items():
        console_log(f"\n=== Uninstalling {emulator} ===")
        for flatpak_id, link_path, _ in entries:
            delete(link_path)
            flatpak.remove_overrides(flatpak_id)
        succeeded += 1
    console_log(f"\nUninstalled {succeeded}/{len(emulators)} emulators.")
    return succeeded


def rollback(args) -> int:
    """Swap to the previous build symlink and revert the captured original config.

    If no previous build exists this is a no-op for that emulator (we can't roll
    back what isn't there). If no captured original exists, the symlink swap
    still proceeds so the user has a working previous binary.
    """
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators or [])
    if not emulators:
        console_error("No emulators matched")
        return 0
    succeeded = 0
    for emulator, entries in emulators.items():
        console_log(f"\n=== Rolling back {emulator} ===")
        old_link = _result_dir(project_dir, emulator)
        prev_link = _previous_result(project_dir, emulator)
        if not os.path.islink(prev_link):
            console_error(f"No previous version recorded for {emulator}")
            continue
        if os.path.lexists(old_link):
            if os.path.islink(old_link):
                os.unlink(old_link)
            else:
                console_error(f"Refusing to overwrite non-symlink at {old_link}")
                continue
        os.rename(prev_link, old_link)

        # Best-effort revert to the most recent captured original.
        try:
            from core.backup import cmd_revert
            cmd_revert(argparse.Namespace(
                config=config_file,
                emulator=emulator,
                version=None,
            ))
        except Exception as e:
            console_error(f"Config revert skipped for {emulator}: {e}")

        for flatpak_id, link_path, source_path in entries:
            flatpak.setup_flatpak(flatpak_id, source_path, project_dir=project_dir)
            create_symlinks(link_path, source_path)
        succeeded += 1
    console_log(f"\nRolled back {succeeded}/{len(emulators)} emulators.")
    return succeeded
