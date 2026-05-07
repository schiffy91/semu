"""Backup, capture-original, revert, and migrate operations."""

import argparse
import datetime
import json
import os
import shutil
import stat as stat_module
import zipfile

from core import state
from core.console import console_error, console_log
from core.symlinks import (
    BACKUP_EXCLUDE,
    filter_emulators,
    find_emulator_dir,
    parse_config,
    resolve_config,
)


def _originals_dir(project_dir, emulator_dir):
    return os.path.join(project_dir, emulator_dir, "originals")


def _originals_manifest(project_dir, emulator_dir):
    return os.path.join(_originals_dir(project_dir, emulator_dir), "manifest.json")


def _load_originals_manifest(project_dir, emulator_dir):
    manifest_path = _originals_manifest(project_dir, emulator_dir)
    if os.path.exists(manifest_path):
        with open(manifest_path) as f:
            return json.load(f)
    return []


def _save_originals_manifest(project_dir, emulator_dir, manifest):
    manifest_path = _originals_manifest(project_dir, emulator_dir)
    os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)


def cmd_backup(args):
    """Snapshot emulator configs to a timestamped zip.

    Writes atomically: zip is created at <name>.zip.tmp and renamed only after
    a complete write. A crashed/cancelled backup leaves the .tmp behind for
    inspection but never a half-valid .zip.
    """
    config_file, project_dir = resolve_config(args)
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup_dir = os.path.join(project_dir, "backups")
    os.makedirs(backup_dir, exist_ok=True)
    backup_path = os.path.join(backup_dir, f"{state.PLATFORM}-{timestamp}.zip")
    tmp_path = backup_path + ".tmp"

    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators)
    count = 0

    try:
        with zipfile.ZipFile(tmp_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for _, entries in emulators.items():
                for _, _, source_path in entries:
                    if not os.path.exists(source_path):
                        continue
                    if os.path.isfile(source_path):
                        arcname = os.path.relpath(source_path, project_dir)
                        zf.write(source_path, arcname)
                        count += 1
                    elif os.path.isdir(source_path):
                        for root, dirs, files in os.walk(source_path):
                            dirs[:] = [d for d in dirs if d not in BACKUP_EXCLUDE]
                            for f in files:
                                filepath = os.path.join(root, f)
                                arcname = os.path.relpath(filepath, project_dir)
                                zf.write(filepath, arcname)
                                count += 1
        os.replace(tmp_path, backup_path)
    except Exception:
        if os.path.exists(tmp_path):
            try:
                os.remove(tmp_path)
            except OSError:
                pass
        raise

    console_log(f"Backed up {count} files to {backup_path}")

    max_backups = 5
    existing = sorted(
        f for f in os.listdir(backup_dir)
        if f.startswith(f"{state.PLATFORM}-") and f.endswith(".zip")
    )
    while len(existing) > max_backups:
        old = existing.pop(0)
        os.remove(os.path.join(backup_dir, old))
        console_log(f"Removed old backup: {old}")

    return backup_path


def cmd_originals(args):
    """List captured original configs for an emulator."""
    _, project_dir = resolve_config(args)
    emulator_dir = find_emulator_dir(project_dir, args.emulator)
    if not emulator_dir:
        console_error(f"Emulator '{args.emulator}' not found")
        return

    manifest = _load_originals_manifest(project_dir, emulator_dir)
    if not manifest:
        console_log(f"No originals captured for {emulator_dir}")
        return

    console_log(f"Originals for {emulator_dir}:")
    for entry in manifest:
        console_log(f"  {entry['version']}  captured {entry['captured']}")


def cmd_capture(args):
    """Capture the current config as an immutable original snapshot."""
    _, project_dir = resolve_config(args)
    emulator_dir = find_emulator_dir(project_dir, args.emulator)
    if not emulator_dir:
        console_error(f"Emulator '{args.emulator}' not found")
        return

    version = args.version
    snapshot_dir = os.path.join(_originals_dir(project_dir, emulator_dir), version)

    if os.path.exists(snapshot_dir):
        console_error(f"Original '{version}' already exists for {emulator_dir}")
        return

    symlinks_file = os.path.join(project_dir, emulator_dir, "symlinks.json")
    if not os.path.isfile(symlinks_file):
        console_error(f"No symlinks.json for {emulator_dir}")
        return

    with open(symlinks_file) as f:
        config = json.load(f)

    copied = 0
    for entry in config:
        if entry == "flatpak":
            continue
        source = os.path.join(project_dir, emulator_dir, entry)
        if not os.path.exists(source):
            continue
        dest = os.path.join(snapshot_dir, entry)
        if os.path.isdir(source):
            shutil.copytree(source, dest, dirs_exist_ok=True)
        else:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(source, dest)
        copied += 1

    for root, _, files in os.walk(snapshot_dir):
        for f in files:
            os.chmod(
                os.path.join(root, f),
                stat_module.S_IRUSR | stat_module.S_IRGRP | stat_module.S_IROTH,
            )

    manifest = _load_originals_manifest(project_dir, emulator_dir)
    manifest.append({
        "version": version,
        "captured": datetime.datetime.now().isoformat(),
    })
    _save_originals_manifest(project_dir, emulator_dir, manifest)

    console_log(f"Captured {copied} entries as original '{version}' for {emulator_dir}")


def cmd_revert(args):
    """Revert an emulator's config to a captured original."""
    config_file, project_dir = resolve_config(args)
    emulator_dir = find_emulator_dir(project_dir, args.emulator)
    if not emulator_dir:
        console_error(f"Emulator '{args.emulator}' not found")
        return

    manifest = _load_originals_manifest(project_dir, emulator_dir)
    if not manifest:
        console_error(f"No originals captured for {emulator_dir}")
        return

    version = args.version or manifest[-1]["version"]
    snapshot_dir = os.path.join(_originals_dir(project_dir, emulator_dir), version)
    if not os.path.exists(snapshot_dir):
        console_error(f"Original '{version}' not found for {emulator_dir}")
        return

    console_log("Backing up current config before revert...")
    cmd_backup(argparse.Namespace(config=config_file, emulators=[emulator_dir]))

    symlinks_file = os.path.join(project_dir, emulator_dir, "symlinks.json")
    with open(symlinks_file) as f:
        config = json.load(f)

    restored = 0
    for entry in config:
        if entry == "flatpak":
            continue
        src = os.path.join(snapshot_dir, entry)
        dst = os.path.join(project_dir, emulator_dir, entry)
        if not os.path.exists(src):
            continue
        if os.path.isdir(src):
            if os.path.exists(dst):
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
        else:
            shutil.copy2(src, dst)
        restored += 1

    console_log(f"Reverted {emulator_dir} to original '{version}' ({restored} entries)")


def cmd_migrate(args):
    """Copy configs that exist in both source and target emulator manifests."""
    config_file, project_dir = resolve_config(args)
    source_dir = find_emulator_dir(project_dir, args.source)
    target_dir = find_emulator_dir(project_dir, args.target)

    if not source_dir:
        console_error(f"Source emulator '{args.source}' not found")
        return
    if not target_dir:
        console_error(f"Target emulator '{args.target}' not found")
        return

    console_log("Backing up both emulators before migration...")
    cmd_backup(argparse.Namespace(config=config_file, emulators=[source_dir, target_dir]))

    source_symlinks = os.path.join(project_dir, source_dir, "symlinks.json")
    target_symlinks = os.path.join(project_dir, target_dir, "symlinks.json")
    if not os.path.isfile(source_symlinks) or not os.path.isfile(target_symlinks):
        console_error("Both emulators must have symlinks.json")
        return

    with open(source_symlinks) as f:
        source_config = json.load(f)
    with open(target_symlinks) as f:
        target_config = json.load(f)

    migrated = 0
    for entry in source_config:
        if entry == "flatpak":
            continue
        if entry not in target_config:
            continue
        src = os.path.join(project_dir, source_dir, entry)
        dst = os.path.join(project_dir, target_dir, entry)
        if not os.path.exists(src):
            continue
        console_log(f"  Migrating {entry}: {source_dir} -> {target_dir}")
        if os.path.isdir(src):
            if os.path.exists(dst):
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
        else:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
        migrated += 1

    console_log(f"Migrated {migrated} entries from {source_dir} to {target_dir}")
