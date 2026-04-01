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


_EXECUTE_DISPATCH = {
    "unlink": lambda args, kw: os.unlink(*args),
    "remove": lambda args, kw: os.remove(*args),
    "symlink": lambda args, kw: os.symlink(args[0], args[1], **kw),
    "makedirs": lambda args, kw: os.makedirs(*args, exist_ok=True),
    "run": lambda args, kw: subprocess.run(*args, capture_output=True, text=True, check=False),
    "install": lambda args, kw: subprocess.run(*args, stdin=sys.stdin, stdout=sys.stdout, check=False),
    "chmod": lambda args, kw: os.chmod(*args),
    "rmdir": lambda args, kw: os.rmdir(*args),
}


def execute(name, *args, **kwargs):
    global NUM_ERRORS
    console_log(f"{name}({', '.join(repr(arg) for arg in args)})")
    if DRY_RUN:
        return None
    fn = _EXECUTE_DISPATCH.get(name)
    if fn is None:
        console_error(f"Unknown execute command: {name}")
        NUM_ERRORS += 1
        return None
    try:
        return fn(args, kwargs)
    except Exception as e:
        console_error(f"Failed to execute {name}({', '.join(repr(arg) for arg in args)})\n{e}\n")
        NUM_ERRORS += 1
        return None


def rmtree(path):
    """Remove directory tree without following symlinks."""
    for root, dirs, files in os.walk(path, topdown=False, followlinks=False):
        for name in files:
            filename = os.path.join(root, name)
            if os.path.islink(filename):
                execute("unlink", filename)
            else:
                execute("chmod", filename, stat.S_IRWXU)
                execute("remove", filename)
        for name in dirs:
            dirname = os.path.join(root, name)
            if os.path.islink(dirname):
                execute("unlink", dirname)
            else:
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
    result = execute("run", ["flatpak", "list", "--app"])
    if result and flatpak_id not in result.stdout:
        execute("install", ["flatpak", "install", "-y", flatpak_id])
    execute("run", ["sudo", "flatpak", "override", flatpak_id, f"--filesystem={path}"])
    execute("run", ["sudo", "flatpak", "override", flatpak_id, f"--filesystem={PORTABLE}"])


def _find_project_dir(config_path):
    """Resolve the project directory from the config file path."""
    if os.path.isabs(config_path):
        return os.path.dirname(config_path)
    return os.path.dirname(os.path.abspath(config_path)) or "."


def _resolve_config(args):
    """Get config file path and project directory."""
    config_file = args.config or os.path.join(os.path.dirname(os.path.abspath(__file__)), "setup.json")
    project_dir = _find_project_dir(config_file)
    return config_file, project_dir


def parse_config(file_path, path):
    global HOST, PORTABLE, NUM_ERRORS
    NUM_ERRORS = 0

    with open(file_path) as f:
        config_data = json.load(f)

    if PLATFORM not in config_data.get("host", {}):
        console_error(f"Platform '{PLATFORM}' not found in config host paths")
        return {}
    if PLATFORM not in config_data.get("portable", {}):
        console_error(f"Platform '{PLATFORM}' not found in config portable paths")
        return {}

    HOST = os.path.expanduser(config_data["host"][PLATFORM])
    PORTABLE = os.path.expanduser(config_data["portable"][PLATFORM])

    results = {}
    for emulator in os.listdir(path):
        symlinks_file = os.path.join(path, emulator, "symlinks.json")
        if not os.path.isfile(symlinks_file):
            continue
        with open(symlinks_file) as f:
            try:
                symlinks_config = json.load(f)
            except json.JSONDecodeError as e:
                console_error(f"Invalid JSON in {symlinks_file}: {e}")
                continue

        flatpak_id = symlinks_config.get("flatpak", "")
        for entry, links in symlinks_config.items():
            if entry == "flatpak":
                continue
            if not isinstance(links, dict):
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


def _filter_emulators(emulators_dict, filter_list):
    """Filter emulators dict by name list (case-insensitive). Empty list = all."""
    if not filter_list:
        return emulators_dict
    lower_filter = [e.lower() for e in filter_list]
    return {k: v for k, v in emulators_dict.items() if k.lower() in lower_filter}


def cmd_symlink(args):
    """Wire emulator configs into host filesystem via symlinks."""
    config_file, project_dir = _resolve_config(args)
    for emulator, entries in _filter_emulators(parse_config(config_file, project_dir), args.emulators).items():
        console_log(f"\n{emulator}")
        for flatpak, link_path, source_path in entries:
            setup_flatpak(flatpak, source_path)
            create_symlinks(link_path, source_path)
    console_log(f"\n{NUM_ERRORS} errors occurred during setup\n")


def cmd_status(args):
    """Show emulator symlink status."""
    config_file, project_dir = _resolve_config(args)
    for emulator, entries in _filter_emulators(parse_config(config_file, project_dir), args.emulators).items():
        for _, link_path, source_path in entries:
            exists = "OK" if os.path.exists(source_path) else "MISSING"
            linked = "linked" if os.path.islink(link_path) else "not linked"
            console_log(f"  {emulator}: {exists}, {linked}")
            console_log(f"    source: {source_path}")
            console_log(f"    link:   {link_path}")


BACKUP_EXCLUDE = {"ROMs", "downloaded_media", "n3ds-fixed", "n3ds-original"}


def cmd_backup(args):
    """Snapshot emulator configs to a timestamped zip."""
    config_file, project_dir = _resolve_config(args)
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup_dir = os.path.join(project_dir, "backups")
    os.makedirs(backup_dir, exist_ok=True)
    backup_path = os.path.join(backup_dir, f"{PLATFORM}-{timestamp}.zip")

    emulators = _filter_emulators(parse_config(config_file, project_dir), args.emulators)
    count = 0

    with zipfile.ZipFile(backup_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for emulator, entries in emulators.items():
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

    console_log(f"Backed up {count} files to {backup_path}")

    max_backups = 5
    existing = sorted(
        [f for f in os.listdir(backup_dir) if f.startswith(f"{PLATFORM}-") and f.endswith(".zip")]
    )
    while len(existing) > max_backups:
        old = existing.pop(0)
        os.remove(os.path.join(backup_dir, old))
        console_log(f"Removed old backup: {old}")


def _find_emulator_dir(project_dir, name):
    """Find emulator directory by case-insensitive name match."""
    for d in os.listdir(project_dir):
        if d.lower() == name.lower() and os.path.isdir(os.path.join(project_dir, d)):
            return d
    return None


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


def cmd_originals(args):
    """List captured original configs for an emulator."""
    _, project_dir = _resolve_config(args)
    emulator_dir = _find_emulator_dir(project_dir, args.emulator)
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
    """Capture current config as an original (immutable snapshot)."""
    _, project_dir = _resolve_config(args)
    emulator_dir = _find_emulator_dir(project_dir, args.emulator)
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

    for root, dirs, files in os.walk(snapshot_dir):
        for f in files:
            os.chmod(os.path.join(root, f), stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)

    manifest = _load_originals_manifest(project_dir, emulator_dir)
    manifest.append({
        "version": version,
        "captured": datetime.datetime.now().isoformat(),
    })
    _save_originals_manifest(project_dir, emulator_dir, manifest)

    console_log(f"Captured {copied} entries as original '{version}' for {emulator_dir}")


def cmd_revert(args):
    """Revert emulator config to a captured original."""
    config_file, project_dir = _resolve_config(args)
    emulator_dir = _find_emulator_dir(project_dir, args.emulator)
    if not emulator_dir:
        console_error(f"Emulator '{args.emulator}' not found")
        return

    manifest = _load_originals_manifest(project_dir, emulator_dir)
    if not manifest:
        console_error(f"No originals captured for {emulator_dir}")
        return

    version = args.version
    if not version:
        version = manifest[-1]["version"]

    snapshot_dir = os.path.join(_originals_dir(project_dir, emulator_dir), version)
    if not os.path.exists(snapshot_dir):
        console_error(f"Original '{version}' not found for {emulator_dir}")
        return

    console_log("Backing up current config before revert...")
    backup_args = argparse.Namespace(config=config_file, emulators=[emulator_dir])
    cmd_backup(backup_args)

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
    """Migrate configs from one emulator to another."""
    config_file, project_dir = _resolve_config(args)
    source_dir = _find_emulator_dir(project_dir, args.source)
    target_dir = _find_emulator_dir(project_dir, args.target)

    if not source_dir:
        console_error(f"Source emulator '{args.source}' not found")
        return
    if not target_dir:
        console_error(f"Target emulator '{args.target}' not found")
        return

    console_log("Backing up both emulators before migration...")
    backup_args = argparse.Namespace(config=config_file, emulators=[source_dir, target_dir])
    cmd_backup(backup_args)

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


def main():
    parser = argparse.ArgumentParser(
        prog="schemulator",
        description="Deterministic emulation environment manager",
    )
    parser.add_argument("--config", "-c", help="Path to config JSON (default: setup.json next to this script)")

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
        args.emulators = []
        cmd_symlink(args)


if __name__ == "__main__":
    main()
