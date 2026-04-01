import json
import os
import sys

import pytest
import setup

PLATFORM = {"win32": "windows", "darwin": "macos", "linux": "linux"}.get(sys.platform, "linux")

symlink_unsupported = pytest.mark.skipif(
    sys.platform == "win32",
    reason="Symlinks require elevated privileges on Windows"
)


def test_parse_config_returns_emulators(mock_project):
    results = setup.parse_config(str(mock_project / "setup.json"), str(mock_project))
    assert "TESTEMU" in results
    assert len(results["TESTEMU"]) > 0


def test_parse_config_resolves_platform_paths(mock_project):
    results = setup.parse_config(str(mock_project / "setup.json"), str(mock_project))
    entries = results["TESTEMU"]
    for _flatpak, link_path, source_path in entries:
        assert os.path.isabs(link_path)
        assert os.path.isabs(source_path)
        assert os.path.exists(source_path)


def test_parse_config_handles_missing_platform(tmp_path):
    config = {"host": {"windows": str(tmp_path)}, "portable": {"windows": str(tmp_path)}}
    (tmp_path / "setup.json").write_text(json.dumps(config))
    if PLATFORM != "windows":
        results = setup.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
        assert results == {}


def test_parse_config_handles_invalid_json(tmp_path):
    emu = tmp_path / "BadEmu"
    emu.mkdir()
    (emu / "symlinks.json").write_text("{invalid json")
    config = {
        "host": {PLATFORM: str(tmp_path)},
        "portable": {PLATFORM: str(tmp_path)},
    }
    (tmp_path / "setup.json").write_text(json.dumps(config))
    results = setup.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert "BADEMU" not in results


@symlink_unsupported
def test_create_symlink(tmp_path):
    source = tmp_path / "source_file.txt"
    source.write_text("hello")
    link = tmp_path / "link_dir" / "source_file.txt"

    setup.create_symlinks(str(link.parent), str(source))
    assert link.is_symlink()
    assert link.read_text() == "hello"


@symlink_unsupported
def test_create_symlink_to_directory(tmp_path):
    source_dir = tmp_path / "source_dir"
    source_dir.mkdir()
    (source_dir / "a.txt").write_text("a")
    (source_dir / "b.txt").write_text("b")

    link_dir = tmp_path / "link_dir"
    setup.create_symlinks(str(link_dir), str(source_dir))

    assert (link_dir / "a.txt").is_symlink()
    assert (link_dir / "b.txt").is_symlink()
    assert (link_dir / "a.txt").read_text() == "a"


def test_platform_detection():
    assert PLATFORM in ("windows", "macos", "linux")


def test_resolve_config_defaults_to_script_dir():
    """Config should resolve relative to setup.py, not CWD."""
    import argparse
    args = argparse.Namespace(config=None)
    config_file, project_dir = setup._resolve_config(args)
    assert os.path.isabs(config_file)
    assert os.path.isabs(project_dir)
