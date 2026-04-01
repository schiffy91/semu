import json
import os
import sys

import setup

PLATFORM = {"win32": "windows", "darwin": "macos", "linux": "linux"}[sys.platform]


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


def test_create_symlink(tmp_path):
    source = tmp_path / "source_file.txt"
    source.write_text("hello")
    link = tmp_path / "link_dir" / "source_file.txt"

    setup.create_symlinks(str(link.parent), str(source))
    assert link.is_symlink()
    assert link.read_text() == "hello"


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
