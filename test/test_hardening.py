"""Tests covering the hardening changes from critic round 1.

Each test corresponds to a numbered finding in the critic review.
"""

import argparse
import json
import os
import re

import pytest

from core import backup, lifecycle, state, steam, syncthing
from core.symlinks import _flatpak_remap, create_symlink
from core import updater


# ----- #7: flatpak no longer uses sudo -----

def test_flatpak_setup_uses_user_scope(monkeypatch):
    """No `sudo` token should appear in any subprocess invocation."""
    from core import flatpak
    monkeypatch.setattr(state, "PLATFORM", "linux")
    state.PORTABLE = "/tmp/portable"
    monkeypatch.setattr(flatpak, "is_available", lambda: True)
    seen = []
    monkeypatch.setattr(flatpak, "execute", lambda name, *a, **kw: seen.append((name, a)) or
                        type("R", (), {"stdout": "info.cemu.Cemu"})())
    flatpak.setup_flatpak("info.cemu.Cemu", "/some/path")
    for _, args in seen:
        assert "sudo" not in args[0], f"sudo found in {args}"


# ----- #8: path traversal in version label -----

def test_capture_rejects_path_traversal_version(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": str(tmp_path / "h" / "Emu")}}))

    args = argparse.Namespace(
        config=str(tmp_path / "setup.json"),
        emulator="Emu",
        version="../../etc/passwd",
    )
    backup.cmd_capture(args)
    # Nothing should have been written outside the originals dir.
    assert not (tmp_path / "etc").exists()
    # And no snapshot for the bogus version.
    assert not (emu / "originals" / "../../etc/passwd").exists()


def test_revert_rejects_path_traversal_version(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": "/x"}}))
    backup.cmd_capture(argparse.Namespace(
        config=str(tmp_path / "setup.json"), emulator="Emu", version="v1",
    ))

    # Bogus version should be rejected.
    backup.cmd_revert(argparse.Namespace(
        config=str(tmp_path / "setup.json"), emulator="Emu", version="../../etc/passwd",
    ))
    # Real config (pre-revert) untouched.
    assert (emu / "config").exists()


# ----- #12: delete() refuses to clobber populated real dirs -----

def test_create_symlink_refuses_to_clobber_real_dir(tmp_path, capsys):
    """If the user has accumulated saves at the link location, refuse to
    delete them when wiring schemulator's symlink."""
    target_source = tmp_path / "source"
    target_source.mkdir()
    (target_source / "marker").write_text("from-schemulator")

    # Pretend the user has existing data in the link path.
    link = tmp_path / "user_data"
    link.mkdir()
    (link / "precious-save.bin").write_text("user data we must NOT delete")

    create_symlink(str(link), str(target_source))

    # User's data still there.
    assert (link / "precious-save.bin").exists()
    assert (link / "precious-save.bin").read_text() == "user data we must NOT delete"
    # Error was reported.
    assert "Refusing to replace existing data" in capsys.readouterr().out + capsys.readouterr().err


# ----- #13: revert restores files writable -----

def test_revert_restores_files_writable(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    settings = emu / "config" / "settings.ini"
    settings.write_text("default")
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": "/x"}}))

    cfg_path = str(tmp_path / "setup.json")
    backup.cmd_capture(argparse.Namespace(config=cfg_path, emulator="Emu", version="v1"))

    # Manually mark the snapshot file 444 (mimicking the old buggy behaviour).
    snapshot_file = emu / "originals" / "v1" / "config" / "settings.ini"
    os.chmod(emu / "originals" / "v1", 0o555)
    os.chmod(snapshot_file, 0o444)

    # Modify, then revert.
    settings.write_text("modified")
    backup.cmd_revert(argparse.Namespace(config=cfg_path, emulator="Emu", version="v1"))

    # Restored file must be user-writable so the emulator can rewrite settings.
    mode = settings.stat().st_mode & 0o777
    assert mode & 0o200, oct(mode)
    assert settings.read_text() == "default"


# ----- #16: timestamp microsecond precision -----

def test_backup_filenames_include_microseconds(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "x").write_text("x")
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": "/x"}}))

    args = argparse.Namespace(config=str(tmp_path / "setup.json"), emulators=[])
    p = backup.cmd_backup(args)
    # Format: linux-YYYYMMDD-HHMMSS-UUUUUU.zip
    name = os.path.basename(p)
    assert re.match(r"linux-\d{8}-\d{6}-\d{6}\.zip", name), name


# ----- #18: capture is atomic (no partial snapshot dir) -----

def test_capture_is_atomic(tmp_path, monkeypatch):
    """If capture fails partway through, no snapshot dir is visible — only
    the .tmp staging dir, which crashes leave behind for inspection."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "x").write_text("x")
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": "/x"}}))

    # Patch shutil.copytree inside the capture loop to raise.
    import shutil
    original_copytree = shutil.copytree
    calls = {"n": 0}

    def boom(src, dst, *a, **kw):
        calls["n"] += 1
        if calls["n"] >= 1:
            raise RuntimeError("simulated crash")
        return original_copytree(src, dst, *a, **kw)

    monkeypatch.setattr(shutil, "copytree", boom)
    with pytest.raises(RuntimeError):
        backup.cmd_capture(argparse.Namespace(
            config=str(tmp_path / "setup.json"), emulator="Emu", version="v1",
        ))

    # No visible snapshot dir.
    assert not (emu / "originals" / "v1").exists()
    # No .tmp staging dir leaked.
    assert not (emu / "originals" / ".v1.tmp").exists()


# ----- #19: discover prefers exact-name binary -----

def test_discover_prefers_known_emulator_binary(tmp_path):
    """RetroArch ships several executables in bin/ (retroarch,
    retroarch-cg2glsl, retroarch-glslangvalidator). Picking by sort order
    would launch the converter; we must pick `retroarch` exactly."""
    result = tmp_path / "result-retroarch"
    bin_dir = result / "bin"
    bin_dir.mkdir(parents=True)
    for name in ("retroarch", "retroarch-cg2glsl", "retroarch-glslangvalidator"):
        f = bin_dir / name
        f.write_text("#!/bin/sh\n")
        f.chmod(0o755)

    found = steam.discover_installed_emulators(str(tmp_path))
    assert len(found) == 1
    assert found[0].exe.endswith("/retroarch")


# ----- #19/related: discover skips staging symlinks -----

def test_discover_skips_staging_symlinks(tmp_path):
    """result-<emu>-staging is a transient build artifact; never expose it."""
    for variant in ("result-dolphin-staging",):
        bin_dir = tmp_path / variant / "bin"
        bin_dir.mkdir(parents=True)
        (bin_dir / "dolphin-emu").write_text("#!/bin/sh\n")
        (bin_dir / "dolphin-emu").chmod(0o755)

    found = steam.discover_installed_emulators(str(tmp_path))
    assert found == []


# ----- #26: Linux Flatpak path remap -----

def test_flatpak_remap_xdg_to_sandbox(monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    home = os.path.expanduser("~")
    out = _flatpak_remap(os.path.join(home, ".config", "Cemu"), "info.cemu.Cemu")
    assert out == os.path.join(home, ".var", "app", "info.cemu.Cemu", "config", "Cemu")

    out = _flatpak_remap(os.path.join(home, ".local", "share", "dolphin-emu"), "org.DolphinEmu.dolphin-emu")
    assert out == os.path.join(home, ".var", "app", "org.DolphinEmu.dolphin-emu", "data", "dolphin-emu")


def test_flatpak_remap_passthrough_for_non_xdg(monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    out = _flatpak_remap("/opt/somewhere/else", "any.id")
    assert out == "/opt/somewhere/else"


def test_flatpak_remap_passes_through_on_macos(monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "macos")
    home = os.path.expanduser("~")
    out = _flatpak_remap(os.path.join(home, ".config", "Cemu"), "info.cemu.Cemu")
    assert out == os.path.join(home, ".config", "Cemu")  # unchanged


# ----- #11: nix store version mining handles hyphenated package names -----

def test_installed_versions_handles_hyphenated_packages(tmp_path):
    """Nix store paths like `<hash>-dolphin-emu-2603a` should yield 2603a,
    `<hash>-pcsx2-2.6.3-456-g4a5b6c` should yield the full version chunk."""
    for store_name, expected in [
        ("aaaaaaaaaaaaaaaa-dolphin-emu-2603a", "2603a"),
        ("bbbbbbbbbbbbbbbb-cemu-2.0-105", "2.0-105"),
        ("cccccccccccccccc-pcsx2-2.6.3-456-g4a5b6c", "2.6.3-456-g4a5b6c"),
    ]:
        store_dir = tmp_path / "store" / store_name
        store_dir.mkdir(parents=True)
        emu = store_name.split("-", 1)[1].split("-")[0]  # rough: dolphin / cemu / pcsx2
        link = tmp_path / f"result-{emu}"
        if link.exists():
            link.unlink()
        link.symlink_to(store_dir)

    versions = updater.installed_versions(str(tmp_path))
    assert versions["dolphin"] == "2603a"
    assert versions["cemu"] == "2.0-105"
    assert versions["pcsx2"] == "2.6.3-456-g4a5b6c"


def test_installed_versions_version_txt_wins_over_store_path(tmp_path):
    """Critic finding #10: explicit version.txt should NOT be overwritten by
    store-path mining."""
    store = tmp_path / "store" / "aaaa-dolphin-2603a"
    store.mkdir(parents=True)
    (tmp_path / "result-dolphin").symlink_to(store)
    (tmp_path / "Dolphin").mkdir()
    (tmp_path / "Dolphin" / "version.txt").write_text("explicit-9999\n")

    out = updater.installed_versions(str(tmp_path))
    assert out["dolphin"] == "explicit-9999"


# ----- #2: device-id Luhn validation accepts real, rejects garbage -----

def test_luhn_check_digit_matches_syncthing_reference():
    """Sanity-check our Luhn impl: a digit fed back through luhn_check_digit
    of (chunk[:-1]) must equal chunk[-1] for any 14-char chunk in a real ID."""
    real_id = "YBGSNWW-6WAU53O-K6P7FDU-DG3DJIM-4PXIL5K-RKDHFXU-SFYNU3X-QMRJCQD"
    raw = real_id.replace("-", "")
    for i in range(4):
        chunk = raw[i * 14:(i + 1) * 14]
        assert syncthing._luhn_check_digit(chunk[:13]) == chunk[13]


# ----- #27: has_run_before is conservative -----

def test_has_run_before_ignores_directories_named_result(tmp_path):
    """A user folder coincidentally named result-something must not block
    the first-run wizard."""
    (tmp_path / "result-oriented-things").mkdir()
    from gui.dialogs.first_run import has_run_before
    assert has_run_before(str(tmp_path)) is False


def test_has_run_before_ignores_dangling_result_symlinks(tmp_path):
    """A result-<emu> symlink whose target doesn't exist (orphaned by a
    failed install) shouldn't claim 'has run before'."""
    nonexistent = tmp_path / "nope"
    (tmp_path / "result-dolphin").symlink_to(nonexistent)
    from gui.dialogs.first_run import has_run_before
    assert has_run_before(str(tmp_path)) is False


def test_has_run_before_true_after_real_install(tmp_path):
    real_target = tmp_path / "fake-store"
    real_target.mkdir()
    (tmp_path / "result-dolphin").symlink_to(real_target)
    from gui.dialogs.first_run import has_run_before
    assert has_run_before(str(tmp_path)) is True


# ----- #28: logger PII scrubbing -----

def test_logger_scrubs_home_dir(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path))
    from core import logger as logger_mod
    monkeypatch.setattr(logger_mod, "_LOGGER", None)
    log = logger_mod.get_logger()
    home = os.path.expanduser("~")
    log.info(f"path is {home}/secret/file.txt")
    for h in log.handlers:
        if hasattr(h, "flush"):
            h.flush()
    log_file = os.path.join(str(tmp_path), "schemulator", "schemulator.log")
    contents = open(log_file).read()
    assert "<HOME>" in contents
    assert home not in contents


def test_logger_scrubs_device_ids(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path))
    from core import logger as logger_mod
    monkeypatch.setattr(logger_mod, "_LOGGER", None)
    log = logger_mod.get_logger()
    real_id = "YBGSNWW-6WAU53O-K6P7FDU-DG3DJIM-4PXIL5K-RKDHFXU-SFYNU3X-QMRJCQD"
    log.info(f"paired with {real_id}")
    for h in log.handlers:
        if hasattr(h, "flush"):
            h.flush()
    log_file = os.path.join(str(tmp_path), "schemulator", "schemulator.log")
    contents = open(log_file).read()
    assert "<DEVICE-ID>" in contents
    assert real_id not in contents


# ----- #35: golden-file VDF roundtrip via canonical lib -----

def test_steam_vdf_roundtrips_through_canonical_library():
    """Encode with our codec, decode with the canonical `vdf` package."""
    pytest.importorskip("vdf")
    import vdf
    s = steam.Shortcut(
        appname="Test",
        exe="/usr/bin/test",
        launch_options="--foo bar",
        tags=["a", "b"],
    )
    blob = steam.encode_shortcuts([s])
    parsed = vdf.binary_loads(blob)
    assert "shortcuts" in parsed
    entry = parsed["shortcuts"]["0"]
    assert entry["AppName"] == "Test"
    assert entry["Exe"] == "/usr/bin/test"
    assert entry["LaunchOptions"] == "--foo bar"
    assert entry["tags"]["0"] == "a"
    assert entry["tags"]["1"] == "b"


# ----- #32: parse_config validates HOST/PORTABLE are absolute -----

def test_parse_config_rejects_relative_host(tmp_path, monkeypatch):
    """Empty / relative host paths must be rejected to prevent symlinks
    landing in random CWD-relative locations."""
    from core import symlinks
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": ""}, "portable": {"linux": str(tmp_path)}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert parsed == {}
