#!/usr/bin/env python
import argparse
import datetime
import json
import os
import shutil
import stat
import subprocess
import sys
import zipfile

DRY_RUN = False
PLATFORM = {"win32": "windows", "darwin": "macos", "linux": "linux"}.get(sys.platform, "linux")
NUM_ERRORS = 0
PORTABLE = ""
HOST = ""


def console_error(message):
    print(f"\n\033[91m{message}\n\033[0m")


def console_log(message):
    print(f"{message}")


def execute(name, *args, **kwargs):
    global NUM_ERRORS
    console_log(f"{name}({', '.join(repr(arg) for arg in args)})")
    if not DRY_RUN:
        try:
            if name == "unlink": return os.unlink(*args)
            if name == "remove": return os.remove(*args)
            if name == "symlink": return os.symlink(rf"{args[0]}", rf"{args[1]}", **kwargs)
            if name == "makedirs": return os.makedirs(*args, exist_ok=True)
            if name == "run": return subprocess.run(*args, capture_output=True, text=True, check=False)
            if name == "install": return subprocess.run(*args, stdin=sys.stdin, stdout=sys.stdout, check=False)
            if name == "chmod": return os.chmod(*args)
            if name == "rmdir": return os.rmdir(*args)
        except Exception as e:
            console_error(f"Failed to execute {name}({', '.join(repr(arg) for arg in args)})\n{e}\n")
            NUM_ERRORS += 1


def rmtree(path):
    for root, dirs, files in os.walk(path, topdown=False):
        for name in files:
            filename = os.path.join(root, name)
            execute("chmod", filename, stat.S_IRWXU)
            execute("remove", filename)
        for name in dirs:
            dirname = os.path.join(root, name)
            execute("chmod", dirname, stat.S_IRWXU)
            execute("rmdir", dirname)
    execute("rmdir", path)


def parent(path):
    return os.path.dirname(path)


def delete(path):
    if os.path.islink(path): execute("unlink", path)
    elif os.path.isdir(path): rmtree(path)
    elif os.path.exists(path): execute("remove", path)


def own(path):
    execute("chmod", path, stat.S_IRWXU)


def create_parent_dirs(path):
    execute("makedirs", parent(path))


def create_symlink(link, source):
    if os.path.lexists(parent(link)): own(parent(link))
    if os.path.lexists(link): own(link)
    delete(link)
    create_parent_dirs(link)
    execute("symlink", source, link, target_is_directory=os.path.isdir(source))


def create_symlinks(link, source):
    if os.path.isdir(source):
        for item in os.listdir(source):
            create_symlink(os.path.join(link, item), os.path.join(source, item))
    else:
        _, file_name = os.path.split(source)
        create_symlink(os.path.join(link, file_name), source)


def setup_flatpak(flatpak_id, path):
    if PLATFORM != "linux" or not flatpak_id or "flatpak" not in (shutil.which("flatpak") or ""):
        return
    if flatpak_id not in execute("run", ["flatpak", "list", "--app"]).stdout:
        execute("install", ["flatpak", "install", "-y", flatpak_id])
    execute("run", ["sudo", "flatpak", "override", flatpak_id, f"--filesystem={path}"])
    execute("run", ["sudo", "flatpak", "override", flatpak_id, f"--filesystem={PORTABLE}"])


def parse_config(file_path, path):
    file = json.load(open(file_path))
    global HOST, PORTABLE
    HOST = {k: os.path.expanduser(v) for k, v in file["host"].items()}[PLATFORM]
    PORTABLE = {k: os.path.expanduser(v) for k, v in file["portable"].items()}[PLATFORM]
    results = {}
    for emulator in os.listdir(path):
        symlinks_file = os.path.join(path, emulator, "symlinks.json")
        if not os.path.isfile(symlinks_file):
            continue
        config = json.load(open(symlinks_file))
        flatpak_id = config.get("flatpak", "")
        for entry, links in config.items():
            if entry == "flatpak":
                continue
            source = os.path.abspath(os.path.join(path, emulator, entry))
            if PLATFORM not in links:
                continue
            link = os.path.abspath(os.path.expanduser(os.path.expandvars(
                links[PLATFORM]
                .replace("${host}", HOST)
                .replace("${portable}", PORTABLE)
                .replace("${flatpak}", flatpak_id)
            )))
            results.setdefault(emulator.upper(), []).append(
                (flatpak_id, os.path.normpath(link), os.path.normpath(source))
            )
    return results


def cmd_symlink(args):
    """Wire emulator configs into host filesystem via symlinks."""
    config_file = args.config or "setup.json"
    for emulator, entries in parse_config(config_file, ".").items():
        if args.emulators and emulator.lower() not in [e.lower() for e in args.emulators]:
            continue
        console_log(f"\n{emulator}")
        for flatpak, link_path, source_path in entries:
            setup_flatpak(flatpak, source_path)
            create_symlinks(link_path, source_path)
    console_log(f"\n{NUM_ERRORS} errors occurred during setup\n")


def cmd_status(args):
    """Show emulator symlink status."""
    config_file = args.config or "setup.json"
    for emulator, entries in parse_config(config_file, ".").items():
        if args.emulators and emulator.lower() not in [e.lower() for e in args.emulators]:
            continue
        for _, link_path, source_path in entries:
            exists = "OK" if os.path.exists(source_path) else "MISSING"
            linked = "linked" if os.path.islink(link_path) else "not linked"
            console_log(f"  {emulator}: {exists}, {linked}")
            console_log(f"    source: {source_path}")
            console_log(f"    link:   {link_path}")


# Dirs to skip when backing up (large, not config)
BACKUP_EXCLUDE = {"ROMs", "downloaded_media", "n3ds-fixed", "n3ds-original"}


def cmd_backup(args):
    """Snapshot emulator configs to a timestamped zip."""
    config_file = args.config or "setup.json"
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup_dir = os.path.join(".", "backups")
    os.makedirs(backup_dir, exist_ok=True)
    backup_path = os.path.join(backup_dir, f"{PLATFORM}-{timestamp}.zip")

    emulators = parse_config(config_file, ".")
    count = 0

    with zipfile.ZipFile(backup_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for emulator, entries in emulators.items():
            if args.emulators and emulator.lower() not in [e.lower() for e in args.emulators]:
                continue
            for _, _, source_path in entries:
                if not os.path.exists(source_path):
                    continue
                if os.path.isfile(source_path):
                    arcname = os.path.relpath(source_path, ".")
                    zf.write(source_path, arcname)
                    count += 1
                elif os.path.isdir(source_path):
                    for root, dirs, files in os.walk(source_path):
                        dirs[:] = [d for d in dirs if d not in BACKUP_EXCLUDE]
                        for f in files:
                            filepath = os.path.join(root, f)
                            arcname = os.path.relpath(filepath, ".")
                            zf.write(filepath, arcname)
                            count += 1

    console_log(f"Backed up {count} files to {backup_path}")

    # Rotate old backups (keep last 5)
    max_backups = 5
    existing = sorted(
        [f for f in os.listdir(backup_dir) if f.startswith(f"{PLATFORM}-") and f.endswith(".zip")]
    )
    while len(existing) > max_backups:
        old = existing.pop(0)
        os.remove(os.path.join(backup_dir, old))
        console_log(f"Removed old backup: {old}")


def _originals_dir(emulator_dir):
    return os.path.join(emulator_dir, "originals")


def _originals_manifest(emulator_dir):
    return os.path.join(_originals_dir(emulator_dir), "manifest.json")


def _load_originals_manifest(emulator_dir):
    manifest_path = _originals_manifest(emulator_dir)
    if os.path.exists(manifest_path):
        return json.load(open(manifest_path))
    return []


def _save_originals_manifest(emulator_dir, manifest):
    manifest_path = _originals_manifest(emulator_dir)
    os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)


def cmd_originals(args):
    """List captured original configs for an emulator."""
    emulator_dir = None
    for d in os.listdir("."):
        if d.lower() == args.emulator.lower() and os.path.isdir(d):
            emulator_dir = d
            break
    if not emulator_dir:
        console_error(f"Emulator '{args.emulator}' not found")
        return

    manifest = _load_originals_manifest(emulator_dir)
    if not manifest:
        console_log(f"No originals captured for {emulator_dir}")
        return

    console_log(f"Originals for {emulator_dir}:")
    for entry in manifest:
        console_log(f"  {entry['version']}  captured {entry['captured']}")


def cmd_capture(args):
    """Capture current config as an original (immutable snapshot)."""
    emulator_dir = None
    for d in os.listdir("."):
        if d.lower() == args.emulator.lower() and os.path.isdir(d):
            emulator_dir = d
            break
    if not emulator_dir:
        console_error(f"Emulator '{args.emulator}' not found")
        return

    version = args.version
    originals_base = _originals_dir(emulator_dir)
    snapshot_dir = os.path.join(originals_base, version)

    if os.path.exists(snapshot_dir):
        console_error(f"Original '{version}' already exists for {emulator_dir}")
        return

    # Find config/data dirs for this emulator
    symlinks_file = os.path.join(emulator_dir, "symlinks.json")
    if not os.path.isfile(symlinks_file):
        console_error(f"No symlinks.json for {emulator_dir}")
        return

    config = json.load(open(symlinks_file))
    copied = 0
    for entry in config:
        if entry == "flatpak":
            continue
        source = os.path.join(emulator_dir, entry)
        if not os.path.exists(source):
            continue
        dest = os.path.join(snapshot_dir, entry)
        if os.path.isdir(source):
            shutil.copytree(source, dest, dirs_exist_ok=True)
        else:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(source, dest)
        copied += 1

    # Make snapshot read-only
    for root, dirs, files in os.walk(snapshot_dir):
        for f in files:
            os.chmod(os.path.join(root, f), stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)

    # Append to manifest
    manifest = _load_originals_manifest(emulator_dir)
    manifest.append({
        "version": version,
        "captured": datetime.datetime.now().isoformat(),
    })
    _save_originals_manifest(emulator_dir, manifest)

    console_log(f"Captured {copied} entries as original '{version}' for {emulator_dir}")


def cmd_revert(args):
    """Revert emulator config to a captured original."""
    emulator_dir = None
    for d in os.listdir("."):
        if d.lower() == args.emulator.lower() and os.path.isdir(d):
            emulator_dir = d
            break
    if not emulator_dir:
        console_error(f"Emulator '{args.emulator}' not found")
        return

    manifest = _load_originals_manifest(emulator_dir)
    if not manifest:
        console_error(f"No originals captured for {emulator_dir}")
        return

    version = args.version
    if not version:
        # Default to latest
        version = manifest[-1]["version"]

    snapshot_dir = os.path.join(_originals_dir(emulator_dir), version)
    if not os.path.exists(snapshot_dir):
        console_error(f"Original '{version}' not found for {emulator_dir}")
        return

    # Auto-backup before reverting
    console_log(f"Backing up current config before revert...")
    backup_args = argparse.Namespace(config=args.config, emulators=[emulator_dir])
    cmd_backup(backup_args)

    # Copy snapshot over current config
    symlinks_file = os.path.join(emulator_dir, "symlinks.json")
    config = json.load(open(symlinks_file))
    restored = 0
    for entry in config:
        if entry == "flatpak":
            continue
        src = os.path.join(snapshot_dir, entry)
        dst = os.path.join(emulator_dir, entry)
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
    """Migrate configs from one emulator to another."""
    source_dir = None
    target_dir = None
    for d in os.listdir("."):
        if d.lower() == args.source.lower() and os.path.isdir(d):
            source_dir = d
        if d.lower() == args.target.lower() and os.path.isdir(d):
            target_dir = d

    if not source_dir:
        console_error(f"Source emulator '{args.source}' not found")
        return
    if not target_dir:
        console_error(f"Target emulator '{args.target}' not found")
        return

    # Auto-backup both before migrating
    console_log("Backing up both emulators before migration...")
    backup_args = argparse.Namespace(config=args.config, emulators=[source_dir, target_dir])
    cmd_backup(backup_args)

    # Find matching directories to copy
    source_symlinks = os.path.join(source_dir, "symlinks.json")
    target_symlinks = os.path.join(target_dir, "symlinks.json")
    if not os.path.isfile(source_symlinks) or not os.path.isfile(target_symlinks):
        console_error("Both emulators must have symlinks.json")
        return

    source_config = json.load(open(source_symlinks))
    target_config = json.load(open(target_symlinks))

    migrated = 0
    for entry in source_config:
        if entry == "flatpak":
            continue
        if entry not in target_config:
            continue
        src = os.path.join(source_dir, entry)
        dst = os.path.join(target_dir, entry)
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


def main():
    parser = argparse.ArgumentParser(
        prog="schemulator",
        description="Deterministic emulation environment manager",
    )
    parser.add_argument("--config", "-c", help="Path to config JSON (default: setup.json)")

    sub = parser.add_subparsers(dest="command")

    p_symlink = sub.add_parser("symlink", help="Wire configs into host filesystem")
    p_symlink.add_argument("emulators", nargs="*", help="Specific emulators (default: all)")

    p_status = sub.add_parser("status", help="Show emulator status")
    p_status.add_argument("emulators", nargs="*", help="Specific emulators (default: all)")

    p_backup = sub.add_parser("backup", help="Snapshot configs to a zip")
    p_backup.add_argument("emulators", nargs="*", help="Specific emulators (default: all)")

    p_originals = sub.add_parser("originals", help="List captured originals")
    p_originals.add_argument("emulator", help="Emulator name")

    p_capture = sub.add_parser("capture", help="Capture current config as an original")
    p_capture.add_argument("emulator", help="Emulator name")
    p_capture.add_argument("version", help="Version label for this snapshot")

    p_revert = sub.add_parser("revert", help="Revert config to a captured original")
    p_revert.add_argument("emulator", help="Emulator name")
    p_revert.add_argument("version", nargs="?", help="Version to revert to (default: latest)")

    p_migrate = sub.add_parser("migrate", help="Migrate configs between emulators")
    p_migrate.add_argument("source", help="Source emulator")
    p_migrate.add_argument("target", help="Target emulator")

    args = parser.parse_args()

    commands = {
        "symlink": cmd_symlink,
        "status": cmd_status,
        "backup": cmd_backup,
        "originals": cmd_originals,
        "capture": cmd_capture,
        "revert": cmd_revert,
        "migrate": cmd_migrate,
    }

    if args.command in commands:
        commands[args.command](args)
    elif args.command is None:
        # Backward compatibility: no subcommand = symlink all
        args.emulators = []
        args.config = args.config or "setup.json"
        cmd_symlink(args)


if __name__ == "__main__":
    main()
