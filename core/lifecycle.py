"""High-level install / update / uninstall / rollback operations.

Each function emits structured progress events the GUI can stream. The CLI
shim in setup.py just prints them; the GUI workers route them to the log panel.
"""

import argparse
import os
import shutil
import subprocess

from core import flatpak, state
from core.backup import cmd_backup
from core.console import console_error, console_log
from core.exec import delete, execute
from core.symlinks import (
    create_symlinks,
    parse_config,
    filter_emulators,
    resolve_config,
)


def _nix_build_target(emulator):
    """Map an emulator name onto its `nix build` flake attribute."""
    name = emulator.lower()
    if name == "es-de" and state.PLATFORM == "linux":
        # Steam Deck uses the dedicated Deck variant — picked up automatically
        # at build time by flake.nix when the host is detected as a Deck. The
        # generic `es-de` attr stays correct here.
        return "es-de"
    return name


def _result_dir(project_dir, emulator):
    return os.path.join(project_dir, f"result-{emulator.lower()}")


def _previous_result(project_dir, emulator):
    """Return the previous `result-<emulator>-prev` symlink path used for rollback."""
    return os.path.join(project_dir, f"result-{emulator.lower()}-prev")


def _nix_build(emulator, project_dir):
    """Run `nix build .#<emulator> --out-link result-<emulator>`. Returns True on success."""
    target = _nix_build_target(emulator)
    out_link = _result_dir(project_dir, emulator)
    cmd = ["nix", "build", f"{project_dir}#{target}", "--out-link", out_link]
    console_log(f"$ {' '.join(cmd)}")
    if state.DRY_RUN:
        return True
    try:
        result = subprocess.run(cmd, check=False)
        return result.returncode == 0
    except FileNotFoundError:
        console_error("nix not found on PATH; install Nix or use the native installer")
        return False


def install(args):
    """Build, link, and capture-original an emulator (or all emulators)."""
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators)
    for emulator, entries in emulators.items():
        console_log(f"\n=== Installing {emulator} ===")
        if not _nix_build(emulator, project_dir):
            console_error(f"nix build failed for {emulator}")
            continue
        for flatpak_id, link_path, source_path in entries:
            flatpak.setup_flatpak(flatpak_id, source_path)
            create_symlinks(link_path, source_path)


def update(args):
    """Update an emulator: backup → atomic rebuild → swap → re-link."""
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators)

    if not emulators:
        console_error("No matching emulators")
        return

    console_log("Backing up before update...")
    cmd_backup(argparse.Namespace(config=config_file, emulators=list(emulators.keys())))

    for emulator, entries in emulators.items():
        console_log(f"\n=== Updating {emulator} ===")
        old_link = _result_dir(project_dir, emulator)
        prev_link = _previous_result(project_dir, emulator)

        if os.path.islink(old_link):
            if os.path.islink(prev_link):
                os.unlink(prev_link)
            os.rename(old_link, prev_link)

        if not _nix_build(emulator, project_dir):
            console_error(f"Update build failed for {emulator}; rolling back")
            if os.path.islink(prev_link):
                os.rename(prev_link, old_link)
            continue

        for flatpak_id, link_path, source_path in entries:
            flatpak.setup_flatpak(flatpak_id, source_path)
            create_symlinks(link_path, source_path)


def uninstall(args):
    """Remove the symlinks and Flatpak overrides for an emulator. The
    project-dir config (saves, etc.) is preserved so other devices that share
    the cloud-synced dir don't lose data."""
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators)
    for emulator, entries in emulators.items():
        console_log(f"\n=== Uninstalling {emulator} ===")
        for flatpak_id, link_path, _ in entries:
            delete(link_path)
            flatpak.remove_overrides(flatpak_id)
        out_link = _result_dir(project_dir, emulator)
        if os.path.islink(out_link):
            execute("unlink", out_link)


def rollback(args):
    """Swap to the previous `result-<emulator>-prev` symlink and revert configs."""
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators)
    for emulator, entries in emulators.items():
        console_log(f"\n=== Rolling back {emulator} ===")
        old_link = _result_dir(project_dir, emulator)
        prev_link = _previous_result(project_dir, emulator)
        if not os.path.islink(prev_link):
            console_error(f"No previous version recorded for {emulator}")
            continue
        if os.path.islink(old_link):
            os.unlink(old_link)
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
            console_error(f"Config revert failed for {emulator}: {e}")

        for flatpak_id, link_path, source_path in entries:
            flatpak.setup_flatpak(flatpak_id, source_path)
            create_symlinks(link_path, source_path)
