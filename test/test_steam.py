"""Tests for the binary-VDF shortcuts.vdf encoder/decoder."""

import os

from core import steam


def test_encode_decode_roundtrip():
    s = steam.Shortcut(
        appname="ES-DE",
        exe="/usr/bin/es-de",
        start_dir="/home/deck",
        launch_options="--fullscreen",
        tags=["Emulation", "Schemulator"],
    )
    blob = steam.encode_shortcuts([s])
    decoded = steam.decode_shortcuts(blob)
    assert len(decoded) == 1
    assert decoded[0].appname == "ES-DE"
    assert decoded[0].exe == "/usr/bin/es-de"
    assert decoded[0].launch_options == "--fullscreen"
    assert decoded[0].tags == ["Emulation", "Schemulator"]


def test_decode_empty():
    assert steam.decode_shortcuts(b"\x00shortcuts\x00\x08\x08") == []


def test_upsert_inserts_new(tmp_path):
    path = str(tmp_path / "shortcuts.vdf")
    s = steam.Shortcut(appname="Schemulator", exe="/opt/schemulator/bin/schemulator")
    steam.upsert_shortcut(path, s)
    with open(path, "rb") as f:
        decoded = steam.decode_shortcuts(f.read())
    assert any(d.appname == "Schemulator" for d in decoded)


def test_upsert_updates_existing(tmp_path):
    path = str(tmp_path / "shortcuts.vdf")
    steam.upsert_shortcut(path, steam.Shortcut(appname="ES-DE", exe="/old/path"))
    steam.upsert_shortcut(path, steam.Shortcut(appname="ES-DE", exe="/new/path"))
    with open(path, "rb") as f:
        decoded = steam.decode_shortcuts(f.read())
    es_de = [d for d in decoded if d.appname == "ES-DE"]
    assert len(es_de) == 1
    assert es_de[0].exe == "/new/path"


def test_remove_shortcut(tmp_path):
    path = str(tmp_path / "shortcuts.vdf")
    steam.upsert_shortcut(path, steam.Shortcut(appname="A", exe="/a"))
    steam.upsert_shortcut(path, steam.Shortcut(appname="B", exe="/b"))
    assert steam.remove_shortcut(path, "A") is True
    with open(path, "rb") as f:
        decoded = steam.decode_shortcuts(f.read())
    assert [d.appname for d in decoded] == ["B"]


def test_appid_is_deterministic():
    s = steam.Shortcut(appname="Test", exe="/bin/test")
    blob = steam.encode_shortcuts([s])
    blob2 = steam.encode_shortcuts([s])
    assert blob == blob2


def test_shortcuts_path_returns_none_when_no_steam(tmp_path, monkeypatch):
    monkeypatch.setenv("HOME", str(tmp_path))
    assert steam.shortcuts_path() is None


def test_shortcuts_path_finds_userdata(tmp_path, monkeypatch):
    steam_root = tmp_path / "steam"
    user_dir = steam_root / "userdata" / "12345" / "config"
    user_dir.mkdir(parents=True)
    result = steam.shortcuts_path(steam_root=str(steam_root))
    assert result is not None
    assert result.endswith(os.path.join("12345", "config", "shortcuts.vdf"))
