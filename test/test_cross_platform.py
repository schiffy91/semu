"""Cross-platform path resolution tests.

We can't run these on every host, but we can simulate by overriding
core.state.PLATFORM. This catches mistakes like "${host}" expansion forgetting
the macOS path or Windows path separators leaking through.
"""

import json
import os

import pytest

from core import state, symlinks


PLATFORMS = ["linux", "macos", "windows"]


@pytest.fixture(params=PLATFORMS)
def platform(request, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", request.param)
    return request.param


def _write_project(tmp_path, platform_paths):
    """Build a fake project with one emulator that has all-platform symlinks.json."""
    cfg = {
        "host":     {p: str(tmp_path / f"host-{p}") for p in PLATFORMS},
        "portable": {p: str(tmp_path / f"portable-{p}") for p in PLATFORMS},
    }
    (tmp_path / "setup.json").write_text(json.dumps(cfg))

    emu = tmp_path / "TestEmu"
    (emu / "config").mkdir(parents=True)
    (emu / "data").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "flatpak": "test.flatpak.id",
        "config": platform_paths["config"],
        "data":   platform_paths["data"],
    }))


def test_parse_resolves_platform_specific_paths(tmp_path, platform):
    _write_project(tmp_path, {
        "config": {
            "linux":   "${host}/test-linux/",
            "macos":   "${host}/Test Mac/",
            "windows": "${portable}/Emulators/Test/",
        },
        "data": {
            "linux":   "${host}/test-data-linux/",
            "macos":   "${host}/test-data-mac/",
            "windows": "${portable}/Emulators/Test/data/",
        },
    })
    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert "TESTEMU" in parsed
    entries = parsed["TESTEMU"]
    assert len(entries) == 2
    for flatpak_id, link, src in entries:
        assert flatpak_id == "test.flatpak.id"
        assert os.path.isabs(link)
        # Link must resolve under the platform's host or portable root.
        plat_host = str(tmp_path / f"host-{platform}")
        plat_portable = str(tmp_path / f"portable-{platform}")
        assert link.startswith(plat_host) or link.startswith(plat_portable), \
            f"link {link} doesn't match platform {platform}"


def test_parse_skips_emulators_without_platform_entry(tmp_path, monkeypatch):
    """If symlinks.json has no entry for the current platform, that emulator
    is silently skipped — not an error."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {
        "host":     {"linux": str(tmp_path / "host"), "macos": str(tmp_path / "host"), "windows": str(tmp_path / "host")},
        "portable": {"linux": str(tmp_path / "p"), "macos": str(tmp_path / "p"), "windows": str(tmp_path / "p")},
    }
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "MacOnlyEmu"
    (emu / "data").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "data": {"macos": "${host}/MacOnly/"}  # no linux entry
    }))
    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert "MACONLYEMU" not in parsed


def test_parse_handles_setup_json_missing_platform(tmp_path, monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    # Only macos and windows in setup.json
    cfg = {
        "host":     {"macos": "/m", "windows": "/w"},
        "portable": {"macos": "/m", "windows": "/w"},
    }
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    assert parsed == {}


def test_macos_paths_contain_application_support(monkeypatch, tmp_path):
    monkeypatch.setattr(state, "PLATFORM", "macos")
    cfg = {
        "host":     {"linux": "/l", "macos": "/Users/test/Library/Application Support", "windows": "/w"},
        "portable": {"linux": "/l", "macos": "/Users/test/ES-DE",                       "windows": "/w"},
    }
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Dolphin"
    (emu / "config").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {"macos": "${host}/Dolphin Emulator/"}
    }))
    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    _, link, _ = parsed["DOLPHIN"][0]
    assert "Library/Application Support" in link
    assert "Dolphin Emulator" in link
