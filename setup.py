#!/usr/bin/env python
import argparse
import json
import os
import shutil
import stat
import subprocess
import sys

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
    """Show bundled emulator versions."""
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

    args = parser.parse_args()

    if args.command == "symlink":
        cmd_symlink(args)
    elif args.command == "status":
        cmd_status(args)
    elif args.command is None:
        # Backward compatibility: no subcommand = symlink all
        args.emulators = []
        args.config = args.config or "setup.json"
        cmd_symlink(args)


if __name__ == "__main__":
    main()
