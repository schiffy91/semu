"""Tests for round-5 security fixes."""

import json
import os
import struct

import pytest

from core import flatpak, state, steam, symlinks


# ----- R5-2: saves/ symlink-following defence -----

def test_populate_save_links_refuses_target_outside_project(tmp_path, capsys, monkeypatch):
    """A peer who can write into a synced project dir might pre-place a
    symlink at <project>/Dolphin/data/GC pointing at ~/.ssh. The next
    add_folder call must refuse to share it (round-5 critic finding #2)."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    from core import syncthing

    project = tmp_path / "project"
    project.mkdir()
    (project / "Dolphin" / "data").mkdir(parents=True)
    # Hostile pre-placement: GC links outside the project.
    outside = tmp_path / "secret-host-dir"
    outside.mkdir()
    (outside / "private.txt").write_text("not-for-peers")
    (project / "Dolphin" / "data" / "GC").symlink_to(outside)

    saves = project / "saves"
    syncthing._populate_save_links(str(project), str(saves))

    # The hostile target must NOT have been linked into saves/.
    if (saves / "Dolphin" / "GC").exists():
        # If it does exist, the materialised target must also be inside the
        # project — i.e. realpath under project_dir. The defence resolved it.
        real = os.path.realpath(saves / "Dolphin" / "GC")
        proj_real = os.path.realpath(project)
        assert os.path.commonpath([real, proj_real]) == proj_real
    # Error logged.
    assert "Refusing to share" in capsys.readouterr().out


def test_add_folder_writes_stignore(tmp_path, monkeypatch):
    """The stignore file must be present after add_folder (defence in
    depth on top of the symlink check)."""
    from core import syncthing
    monkeypatch.setattr(syncthing, "api_key", lambda *a, **kw: None)
    project = tmp_path / "project"
    project.mkdir()
    syncthing.add_folder(str(project))
    stignore = project / "saves" / ".stignore"
    assert stignore.exists()
    contents = stignore.read_text()
    assert "Schemulator" in contents
    # Excludes dotfiles and tmp/swp churn.
    assert "*.tmp" in contents and "*.swp" in contents
    assert "/.??*" in contents


# ----- R5-3: expandvars dropped + Flatpak filesystem gated -----

def test_parse_config_does_not_expandvars(tmp_path, monkeypatch):
    """A symlinks.json with $HOME / ${ANY_VAR} must be left literal so a
    peer-controlled manifest can't redirect symlinks via env (round-5 #3)."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    monkeypatch.setenv("MALICIOUS", "/etc")
    cfg = {"host": {"linux": str(tmp_path / "host")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Emu"
    (emu / "config").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {"linux": "${host}/$MALICIOUS/x/"}
    }))
    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    _, link, _ = parsed["EMU"][0]
    # The literal $MALICIOUS must remain (not be expanded to /etc).
    assert "$MALICIOUS" in link or "MALICIOUS" in link
    assert "/etc" not in link


def test_setup_flatpak_refuses_path_outside_project(tmp_path, monkeypatch, capsys):
    """If parse_config (or a hostile manifest) yields a source_path outside
    the project_dir, setup_flatpak must refuse to grant filesystem access
    to that path (round-5 #3)."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    monkeypatch.setattr(flatpak, "is_available", lambda: True)
    seen = []
    monkeypatch.setattr(flatpak, "execute",
                        lambda *a, **kw: seen.append(a) or
                        type("R", (), {"stdout": ""})())

    project = tmp_path / "project"
    project.mkdir()
    outside = tmp_path / "outside"
    outside.mkdir()

    flatpak.setup_flatpak("org.libretro.RetroArch", str(outside),
                          project_dir=str(project))
    # No flatpak override should have been issued for the outside path.
    overrides = [a for a in seen if a and a[0] == "run"
                 and isinstance(a[1], list) and "override" in a[1]]
    assert overrides == []
    out = capsys.readouterr().out
    assert "outside the project dir" in out


def test_setup_flatpak_allows_path_under_project(tmp_path, monkeypatch):
    """The legitimate case: source_path is under project_dir."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    monkeypatch.setattr(flatpak, "is_available", lambda: True)
    seen = []
    monkeypatch.setattr(flatpak, "execute",
                        lambda *a, **kw: seen.append(a) or
                        type("R", (), {"stdout": "org.libretro.RetroArch"})())

    project = tmp_path / "project"
    (project / "RetroArch" / "config").mkdir(parents=True)

    flatpak.setup_flatpak("org.libretro.RetroArch",
                          str(project / "RetroArch" / "config"),
                          project_dir=str(project))
    overrides = [a for a in seen if a and a[0] == "run"
                 and isinstance(a[1], list) and "override" in a[1]]
    assert overrides, "expected an override call for an in-project path"


# ----- R5-4: subprocess env scrubbing -----

def test_safe_env_drops_credential_vars(monkeypatch):
    from core.lifecycle import _safe_env, _CREDENTIAL_VARS
    for name in ("GITHUB_TOKEN", "AWS_SECRET_ACCESS_KEY", "ANTHROPIC_API_KEY"):
        monkeypatch.setenv(name, "leak-me")
    monkeypatch.setenv("HOME", "/home/test")  # untouched
    env = _safe_env()
    for name in _CREDENTIAL_VARS:
        assert name not in env
    assert env.get("HOME") == "/home/test"


# ----- R5-6: VDF decoder bounds check -----

def test_decode_handles_truncated_int32():
    """A VDF blob that ends mid-int32 must NOT raise — return what we have."""
    blob = (
        b"\x00shortcuts\x00"      # outer object header
        b"\x00" + b"0" + b"\x00"  # entry 0
        b"\x02key\x00"             # int32 marker + key
        b"\x01\x02\x03"            # only 3 bytes — truncated mid-int
    )
    # Should not raise.
    result = steam.decode_shortcuts(blob)
    assert isinstance(result, list)


def test_decode_handles_missing_terminator():
    """A VDF blob with an unterminated cstring must not raise."""
    blob = b"\x01key_with_no_terminator"
    result = steam.decode_shortcuts(blob)
    assert isinstance(result, list)


def test_decode_handles_garbage():
    """Random garbage must not raise."""
    result = steam.decode_shortcuts(b"\xff\x00\x01\x02\x03\x04")
    assert isinstance(result, list)
