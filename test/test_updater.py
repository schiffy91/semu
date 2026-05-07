"""Tests for the version manifest comparator and atomic-swap helper."""

import os

from core import updater


def test_installed_versions_reads_version_files(tmp_path):
    (tmp_path / "Dolphin").mkdir()
    (tmp_path / "Dolphin" / "version.txt").write_text("2603a\n")
    (tmp_path / "PCSX2").mkdir()
    (tmp_path / "PCSX2" / "version.txt").write_text("2.6.3")
    (tmp_path / "NoVersion").mkdir()

    versions = updater.installed_versions(str(tmp_path))
    assert versions["dolphin"] == "2603a"
    assert versions["pcsx2"] == "2.6.3"
    assert "noversion" not in versions


def test_installed_versions_from_result_symlinks(tmp_path):
    """When result-<emu> points at /nix/store/<hash>-<name>-<version>/, derive
    the version from the store path."""
    fake_store = tmp_path / "nix-store" / "abc123-dolphin-emu-2603a"
    fake_store.mkdir(parents=True)
    (tmp_path / "result-dolphin").symlink_to(fake_store)

    versions = updater.installed_versions(str(tmp_path))
    assert versions["dolphin"] == "2603a"


def test_installed_versions_skips_prev_symlinks(tmp_path):
    """result-<emu>-prev should not pollute the installed list."""
    fake_store = tmp_path / "nix-store" / "abc123-dolphin-emu-2603a"
    fake_store.mkdir(parents=True)
    (tmp_path / "result-dolphin-prev").symlink_to(fake_store)

    versions = updater.installed_versions(str(tmp_path))
    assert "dolphin" not in versions
    assert "dolphin-prev" not in versions


def test_installed_versions_falls_back_to_installed_marker(tmp_path):
    """Non-versioned store paths should still report `installed` so the GUI
    knows it's there."""
    fake_store = tmp_path / "nix-store" / "weirdpath"
    fake_store.mkdir(parents=True)
    (tmp_path / "result-weird").symlink_to(fake_store)

    versions = updater.installed_versions(str(tmp_path))
    assert versions["weird"] == "installed"


def test_has_update_returns_diffs():
    manifest = updater.Manifest(
        schemulator_version="1.0.0",
        emulators={
            "dolphin": {"version": "2604"},
            "pcsx2":   {"version": "2.6.3"},
            "ryujinx": {"version": "1.3.4"},
        },
    )
    installed = {"dolphin": "2603a", "pcsx2": "2.6.3"}
    diffs = updater.has_update(installed, manifest)
    assert diffs == {"dolphin": "2604", "ryujinx": "1.3.4"}


def test_atomic_swap_moves_dirs(tmp_path):
    staging = tmp_path / "staging"
    staging.mkdir()
    (staging / "marker").write_text("new")

    current = tmp_path / "current"
    current.mkdir()
    (current / "marker").write_text("old")

    rollback = tmp_path / "rollback"

    assert updater.atomic_swap(str(staging), str(current), str(rollback)) is True
    assert (current / "marker").read_text() == "new"
    assert (rollback / "marker").read_text() == "old"
    assert not staging.exists()


def test_atomic_swap_rejects_missing_staging(tmp_path):
    assert updater.atomic_swap(
        str(tmp_path / "missing"),
        str(tmp_path / "current"),
        str(tmp_path / "rollback"),
    ) is False
