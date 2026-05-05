"""Command-line entry point that wires argparse to core.* operations."""

import argparse

from core import lifecycle
from core.backup import (
    cmd_backup,
    cmd_capture,
    cmd_migrate,
    cmd_originals,
    cmd_revert,
)
from core.console import console_log
from core.symlinks import (
    create_symlinks,
    filter_emulators,
    parse_config,
    resolve_config,
)
from core import flatpak as flatpak_module
from core import state


def cmd_symlink(args):
    config_file, project_dir = resolve_config(args)
    emulators = filter_emulators(parse_config(config_file, project_dir), args.emulators)
    for emulator, entries in emulators.items():
        console_log(f"\n{emulator}")
        for flatpak_id, link_path, source_path in entries:
            flatpak_module.setup_flatpak(flatpak_id, source_path)
            create_symlinks(link_path, source_path)
    console_log(f"\n{state.NUM_ERRORS} errors occurred during setup\n")


def cmd_status(args):
    import os
    config_file, project_dir = resolve_config(args)
    for emulator, entries in filter_emulators(parse_config(config_file, project_dir), args.emulators).items():
        for _, link_path, source_path in entries:
            exists = "OK" if os.path.exists(source_path) else "MISSING"
            linked = "linked" if os.path.islink(link_path) else "not linked"
            console_log(f"  {emulator}: {exists}, {linked}")
            console_log(f"    source: {source_path}")
            console_log(f"    link:   {link_path}")


def cmd_gui(_args):
    """Launch the PySide6 GUI."""
    from gui.app import main as gui_main
    gui_main()


def build_parser() -> argparse.ArgumentParser:
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

    p_install = sub.add_parser("install", help="Build emulators and wire configs")
    p_install.add_argument("emulators", nargs="*", help="Specific emulators (default: all)")

    p_update = sub.add_parser("update", help="Atomic update of emulators (with rollback)")
    p_update.add_argument("emulators", nargs="*", help="Specific emulators (default: all)")

    p_uninstall = sub.add_parser("uninstall", help="Remove symlinks and Flatpak overrides")
    p_uninstall.add_argument("emulators", nargs="*", help="Specific emulators (default: all)")

    p_rollback = sub.add_parser("rollback", help="Roll back to the previous build")
    p_rollback.add_argument("emulators", nargs="*", help="Specific emulators (default: all)")

    sub.add_parser("gui", help="Launch the desktop GUI")

    return parser


COMMANDS = {
    "symlink":   cmd_symlink,
    "status":    cmd_status,
    "backup":    cmd_backup,
    "originals": cmd_originals,
    "capture":   cmd_capture,
    "revert":    cmd_revert,
    "migrate":   cmd_migrate,
    "install":   lifecycle.install,
    "update":    lifecycle.update,
    "uninstall": lifecycle.uninstall,
    "rollback":  lifecycle.rollback,
    "gui":       cmd_gui,
}


def main():
    parser = build_parser()
    args = parser.parse_args()
    if args.command in COMMANDS:
        COMMANDS[args.command](args)
    elif args.command is None:
        args.emulators = []
        cmd_symlink(args)
