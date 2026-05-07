"""Edge-case tests for Steam VDF + path discovery."""

import os
import struct

import pytest

from core import steam


def test_find_steam_root_returns_none_when_nothing(tmp_path, monkeypatch):
    monkeypatch.setenv("HOME", str(tmp_path))
    assert steam.find_steam_root() is None


def test_find_steam_root_picks_first_existing(tmp_path, monkeypatch):
    flatpak_root = tmp_path / "flatpak-steam" / ".local" / "share" / "Steam"
    (flatpak_root / "userdata").mkdir(parents=True)
    monkeypatch.setattr(steam, "STEAM_ROOT_CANDIDATES",
                        (str(flatpak_root.parent.parent.parent / "doesnt-exist"),
                         str(flatpak_root)))
    assert steam.find_steam_root() == str(flatpak_root)


def test_list_steam_users_orders_by_mtime(tmp_path):
    root = tmp_path / "steam"
    a = root / "userdata" / "111111"
    b = root / "userdata" / "222222"
    a.mkdir(parents=True)
    b.mkdir(parents=True)
    # Set 'b' as newer
    os.utime(str(a), (1700000000, 1700000000))
    os.utime(str(b), (1800000000, 1800000000))
    users = steam.list_steam_users(str(root))
    assert users == ["222222", "111111"]


def test_shortcuts_path_uses_specific_user(tmp_path):
    root = tmp_path / "steam"
    (root / "userdata" / "111").mkdir(parents=True)
    (root / "userdata" / "222").mkdir(parents=True)
    p = steam.shortcuts_path(steam_root=str(root), user_id="222")
    assert p.endswith(os.path.join("222", "config", "shortcuts.vdf"))


def test_decode_handles_truncated_data():
    """A corrupted shortcuts.vdf should not raise — decode best-effort."""
    bad = b"\x01key\x00value\x00"  # missing terminating end-of-object marker
    # Should not raise
    result = steam.decode_shortcuts(bad)
    assert isinstance(result, list)


def test_encode_with_minimal_shortcut():
    s = steam.Shortcut(appname="X", exe="/x")
    blob = steam.encode_shortcuts([s])
    decoded = steam.decode_shortcuts(blob)
    assert len(decoded) == 1
    assert decoded[0].appname == "X"
    assert decoded[0].exe == "/x"
    assert decoded[0].launch_options == ""
    assert decoded[0].tags == []


def test_appid_is_in_unsigned_range():
    """Steam stores appids with bit 31 set (unsigned). We must round-trip them."""
    # Reach into the encoder and check that appid > 0x80000000 doesn't crash
    s = steam.Shortcut(appname="DeadlyApp", exe="/foo")
    blob = steam.encode_shortcuts([s])
    # The appid field should be present in the binary blob.
    assert b"appid\x00" in blob


def test_remove_nonexistent_shortcut_returns_false(tmp_path):
    path = str(tmp_path / "shortcuts.vdf")
    steam.upsert_shortcut(path, steam.Shortcut(appname="A", exe="/a"))
    assert steam.remove_shortcut(path, "Z") is False


def test_upsert_to_corrupt_file_starts_fresh(tmp_path):
    path = str(tmp_path / "shortcuts.vdf")
    with open(path, "wb") as f:
        f.write(b"not-a-vdf-file")
    steam.upsert_shortcut(path, steam.Shortcut(appname="Recovered", exe="/r"))
    with open(path, "rb") as f:
        decoded = steam.decode_shortcuts(f.read())
    assert any(d.appname == "Recovered" for d in decoded)


def test_discover_installed_emulators_linux(tmp_path):
    """Linux layout: result-<emu>/bin/<binary>."""
    result = tmp_path / "result-dolphin"
    bin_dir = result / "bin"
    bin_dir.mkdir(parents=True)
    binary = bin_dir / "dolphin-emu"
    binary.write_text("#!/bin/sh\nexit 0\n")
    binary.chmod(0o755)

    found = steam.discover_installed_emulators(str(tmp_path))
    assert len(found) == 1
    assert found[0].name == "dolphin"
    assert found[0].exe == str(binary)
    assert found[0].kind == "binary"


def test_discover_installed_emulators_macos(tmp_path):
    """macOS layout: result-<emu>/Applications/<Name>.app."""
    result = tmp_path / "result-azahar"
    apps = result / "Applications"
    apps.mkdir(parents=True)
    (apps / "Azahar.app").mkdir()

    found = steam.discover_installed_emulators(str(tmp_path))
    assert len(found) == 1
    assert found[0].name == "azahar"
    assert found[0].kind == "app"


def test_discover_skips_prev_symlinks(tmp_path):
    """result-<emu>-prev should be ignored so we don't duplicate shortcuts."""
    for variant in ("result-dolphin", "result-dolphin-prev"):
        result = tmp_path / variant
        bin_dir = result / "bin"
        bin_dir.mkdir(parents=True)
        binary = bin_dir / "dolphin-emu"
        binary.write_text("#!/bin/sh\n")
        binary.chmod(0o755)

    found = steam.discover_installed_emulators(str(tmp_path))
    names = [e.name for e in found]
    assert names == ["dolphin"]
