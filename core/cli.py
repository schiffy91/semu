"""Command-line entry point that wires argparse to core.* operations."""

import argparse
import os

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


def cmd_controllers(args):
    """Apply a bundled controller profile fragment to one or all emulators."""
    from core import controllers
    _, project_dir = resolve_config(args)
    if args.emulator:
        ok = controllers.apply(project_dir, args.emulator, args.profile)
        console_log("applied" if ok else "no fragment found / unknown profile")
        return 0 if ok else 1
    n = controllers.apply_all(project_dir, args.profile)
    console_log(f"Applied '{args.profile}' to {n} emulators")
    return 0 if n else 1


def cmd_sd_scan(args):
    """Scan /run/media (and /Volumes on macOS) for SD cards with ROMs."""
    from core import sdcard
    cards = sdcard.list_sdcards()
    if not cards:
        console_log("No external storage detected.")
        return 1
    for c in cards:
        marker = " [EmuDeck]" if c.has_emudeck_layout else ""
        total = sum(len(v) for v in c.rom_systems.values())
        console_log(f"{c.label}{marker}  {c.mount_path}  ({total} ROMs across {len(c.rom_systems)} systems)")
        for system, roms in sorted(c.rom_systems.items()):
            console_log(f"  {system}: {len(roms)} ROMs")
    _, project_dir = resolve_config(args)
    missing = sdcard.check_firmware(project_dir)
    if missing:
        console_log("\nMissing firmware:")
        for emu, desc in missing.items():
            console_log(f"  {emu}: {desc}")
    return 0


def cmd_steam_shortcut(args):
    """Add (or remove) a non-Steam shortcut for ES-DE."""
    from core import steam
    path = steam.shortcuts_path()
    if not path:
        console_error("No Steam install detected. Open Steam at least once first.")
        return 1
    if args.remove:
        ok = steam.remove_shortcut(path, args.appname)
        console_log(f"Removed {args.appname}" if ok else f"{args.appname} not present")
        return 0 if ok else 1
    if not args.exe:
        console_error("--exe is required when adding a shortcut")
        return 1
    sc = steam.Shortcut(
        appname=args.appname,
        exe=args.exe,
        start_dir=os.path.dirname(args.exe) if os.path.isabs(args.exe) else "",
        launch_options=args.launch_options or "",
        tags=args.tags or [],
    )
    steam.upsert_shortcut(path, sc)
    console_log(f"Wrote shortcut '{args.appname}' to {path}")
    return 0


def cmd_sync(args):
    """Manage the bundled Syncthing sidecar."""
    from core import syncthing
    if args.action == "id":
        d = syncthing.device_id()
        if d:
            console_log(d)
            return 0
        console_error("No device ID — run `schemulator sync init` first")
        return 1
    if args.action == "init":
        ok = syncthing.init()
        console_log("Initialised" if ok else "Init failed (binary missing?)")
        return 0 if ok else 1
    if args.action == "start":
        proc = syncthing.start()
        ok = proc is not None and proc.poll() is None
        console_log("Started" if ok else "Start failed")
        return 0 if ok else 1
    if args.action == "status":
        s = syncthing.status()
        console_log(f"running:   {s.running}")
        console_log(f"device id: {s.device_id}")
        return 0
    if args.action == "share":
        _, project_dir = resolve_config(args)
        ok = syncthing.add_folder(project_dir)
        console_log("Folder shared" if ok else "Share failed (sidecar not running?)")
        return 0 if ok else 1
    if args.action == "pair":
        if not args.peer:
            console_error("--peer DEVICE_ID is required")
            return 1
        ok = syncthing.add_device(args.peer, args.name or "")
        console_log("Peer authorised" if ok else "Pairing failed (invalid ID or sidecar down)")
        return 0 if ok else 1
    console_error(f"Unknown sync action: {args.action}")
    return 1


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

    p_ctrl = sub.add_parser("controllers", help="Apply bundled controller-profile fragments")
    p_ctrl.add_argument("profile", choices=["xbox", "dualsense"], help="Profile name")
    p_ctrl.add_argument("emulator", nargs="?", help="Emulator name (default: all)")

    p_sd = sub.add_parser("sd-scan", help="Scan SD cards / external storage for ROMs")

    p_steam = sub.add_parser("steam-shortcut", help="Add or remove a non-Steam shortcut")
    p_steam.add_argument("--appname", default="ES-DE (Schemulator)", help="Display name")
    p_steam.add_argument("--exe", help="Absolute path to the launcher binary")
    p_steam.add_argument("--launch-options", default="", help="Optional CLI args")
    p_steam.add_argument("--tags", nargs="*", default=["Schemulator", "Emulation"])
    p_steam.add_argument("--remove", action="store_true", help="Remove the shortcut by appname")

    p_sync = sub.add_parser("sync", help="Manage the bundled Syncthing sidecar")
    p_sync.add_argument("action", choices=["init", "start", "status", "id", "share", "pair"])
    p_sync.add_argument("--peer", help="Peer device ID (for pair)")
    p_sync.add_argument("--name", help="Friendly name for the peer (for pair)")

    return parser


COMMANDS = {
    "symlink":         cmd_symlink,
    "status":          cmd_status,
    "backup":          cmd_backup,
    "originals":       cmd_originals,
    "capture":         cmd_capture,
    "revert":          cmd_revert,
    "migrate":         cmd_migrate,
    "install":         lifecycle.install,
    "update":          lifecycle.update,
    "uninstall":       lifecycle.uninstall,
    "rollback":        lifecycle.rollback,
    "gui":             cmd_gui,
    "controllers":     cmd_controllers,
    "sd-scan":         cmd_sd_scan,
    "steam-shortcut":  cmd_steam_shortcut,
    "sync":            cmd_sync,
}


def main():
    parser = build_parser()
    args = parser.parse_args()
    if args.command in COMMANDS:
        COMMANDS[args.command](args)
    elif args.command is None:
        args.emulators = []
        cmd_symlink(args)
