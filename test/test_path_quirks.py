"""Path quirks: spaces, unicode, case-insensitive lookups, weird user dirs.

These are the real-world things that break shipped software. Mac users have
"Application Support" with a space; Windows users have unicode in usernames;
case-insensitive HFS+ surprises Linux developers.
"""

import json
import os
import sys

import pytest

from core import state, symlinks


@pytest.fixture(autouse=True)
def reset_platform(monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")


def test_paths_with_spaces_round_trip(tmp_path):
    host = tmp_path / "Application Support"
    host.mkdir()
    cfg = {"host": {"linux": str(host)}, "portable": {"linux": str(tmp_path / "ES-DE")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    emu = tmp_path / "Test Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {"linux": "${host}/Test Emu/"}
    }))

    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert "TEST EMU" in parsed
    _, link, src = parsed["TEST EMU"][0]
    assert "Application Support" in link
    assert "Test Emu" in src


def test_paths_with_unicode(tmp_path):
    cfg = {"host": {"linux": str(tmp_path / "ホスト")}, "portable": {"linux": str(tmp_path / "便携")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg, ensure_ascii=False))

    emu = tmp_path / "TestEmu"
    (emu / "config").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {"linux": "${host}/TestEmu/"}
    }, ensure_ascii=False))

    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert "TESTEMU" in parsed
    _, link, _ = parsed["TESTEMU"][0]
    assert "ホスト" in link


def test_find_emulator_dir_is_case_insensitive(tmp_path):
    """Steam Deck uses ext4 (case-sensitive), macOS usually case-insensitive HFS+
    (or APFS case-sensitive variant). find_emulator_dir must work on both."""
    (tmp_path / "Dolphin").mkdir()
    assert symlinks.find_emulator_dir(str(tmp_path), "dolphin") == "Dolphin"
    assert symlinks.find_emulator_dir(str(tmp_path), "DOLPHIN") == "Dolphin"
    assert symlinks.find_emulator_dir(str(tmp_path), "Dolphin") == "Dolphin"
    assert symlinks.find_emulator_dir(str(tmp_path), "missing") is None


def test_setup_json_with_tilde_expanded(tmp_path, monkeypatch):
    """`~/...` in setup.json should expand to $HOME, not be taken literally."""
    monkeypatch.setenv("HOME", str(tmp_path))
    cfg = {"host": {"linux": "~/.config/"}, "portable": {"linux": "~/ES-DE/"}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {"linux": "${host}/emu/"}
    }))

    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    _, link, _ = parsed["EMU"][0]
    # link should contain the expanded $HOME, not literal ~
    assert str(tmp_path) in link
    assert "~" not in link


@pytest.mark.skipif(sys.platform == "win32", reason="symlinks need elevated privs on Windows")
def test_long_path(tmp_path):
    """Some users nest project dirs deeply. ~150 chars should still work."""
    deep = tmp_path
    for i in range(8):
        deep = deep / f"folder_with_a_reasonably_long_name_{i}"
        deep.mkdir()
    cfg = {"host": {"linux": str(deep / "host")}, "portable": {"linux": str(deep / "p")}}
    (deep / "setup.json").write_text(json.dumps(cfg))
    emu = deep / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({"config": {"linux": "${host}/Emu/"}}))
    parsed = symlinks.parse_config(str(deep / "setup.json"), str(deep))
    assert "EMU" in parsed
