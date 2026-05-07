"""Tests for the high-level lifecycle (install/update/uninstall/rollback).

Nix isn't available in CI, so we monkeypatch _nix_build to fake the build.
That isolates the lifecycle wiring (symlinks, capture, swap, error recovery)
from the actual emulator builds.
"""

import argparse
import json
import os
import shutil

import pytest

from core import lifecycle, state


@pytest.fixture
def project(tmp_path, monkeypatch):
    """Build a minimal project tree with one emulator and a working setup.json."""
    # setup.json
    config = {
        "host":     {"linux": str(tmp_path / "host"), "macos": str(tmp_path / "host"), "windows": str(tmp_path / "host")},
        "portable": {"linux": str(tmp_path / "portable"), "macos": str(tmp_path / "portable"), "windows": str(tmp_path / "portable")},
    }
    (tmp_path / "setup.json").write_text(json.dumps(config))

    # One fake emulator
    emu = tmp_path / "TestEmu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "settings.ini").write_text("default=true\n")
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {
            "linux":   str(tmp_path / "host" / "TestEmu"),
            "macos":   str(tmp_path / "host" / "TestEmu"),
            "windows": str(tmp_path / "host" / "TestEmu"),
        }
    }))

    # Fake nix build: just create the result symlink target.
    fake_store = tmp_path / "nix-store-fake"
    fake_store.mkdir()
    (fake_store / "bin").mkdir()
    (fake_store / "bin" / "TestEmu").write_text("#!/bin/sh\nexit 0\n")

    def fake_build(emulator, project_dir):
        out_link = lifecycle._result_dir(project_dir, emulator)
        if os.path.lexists(out_link):
            os.unlink(out_link)
        os.symlink(str(fake_store), out_link)
        return True

    def fake_build_to(emulator, project_dir, out_link):
        # Used by update(): build directly to the requested path.
        if os.path.lexists(out_link):
            os.unlink(out_link)
        os.symlink(str(fake_store), out_link)
        return True

    monkeypatch.setattr(lifecycle, "_nix_build", fake_build)
    monkeypatch.setattr(lifecycle, "_nix_build_to", fake_build_to)
    monkeypatch.setattr(lifecycle, "_nix_available", lambda: True)

    return tmp_path


def _args(project, **kw):
    return argparse.Namespace(config=str(project / "setup.json"), emulators=kw.get("emulators", []))


def test_install_creates_symlinks_and_baseline(project):
    n = lifecycle.install(_args(project))
    assert n == 1
    assert os.path.islink(project / "result-testemu")
    # symlinks materialised under host/
    host_link = project / "host" / "TestEmu"
    assert host_link.exists()
    # baseline original captured
    manifest_path = project / "TestEmu" / "originals" / "manifest.json"
    assert manifest_path.exists()
    manifest = json.loads(manifest_path.read_text())
    assert any(e["version"] == "baseline" for e in manifest)


def test_install_baseline_only_first_time(project):
    lifecycle.install(_args(project))
    # Re-install should NOT add a second baseline entry.
    lifecycle.install(_args(project))
    manifest = json.loads((project / "TestEmu" / "originals" / "manifest.json").read_text())
    baselines = [e for e in manifest if e["version"] == "baseline"]
    assert len(baselines) == 1


def test_update_keeps_prev_for_rollback(project):
    lifecycle.install(_args(project))
    assert os.path.islink(project / "result-testemu")
    lifecycle.update(_args(project))
    # Old build moved aside; new build in place
    assert os.path.islink(project / "result-testemu")
    assert os.path.islink(project / "result-testemu-prev")
    # Staging link should be cleaned up (atomic-swap leaves nothing behind).
    assert not os.path.lexists(project / "result-testemu-staging")


def test_update_failure_keeps_current_install_intact(project, monkeypatch):
    """If the new build fails, the user's *current* install must keep working.
    The atomic update flow builds to result-<emu>-staging FIRST and only
    swaps on success — so a failed build never touches result-<emu> (critic
    finding #9)."""
    lifecycle.install(_args(project))
    original_target = os.readlink(project / "result-testemu")

    # Now make nix fail for the staging build.
    monkeypatch.setattr(lifecycle, "_nix_build_to", lambda *a, **kw: False)
    lifecycle.update(_args(project))

    # Current install untouched — same target path.
    assert os.readlink(project / "result-testemu") == original_target
    # No prev was created (the rotation never happened).
    assert not os.path.lexists(project / "result-testemu-prev")
    # No staging artifact lingering.
    assert not os.path.lexists(project / "result-testemu-staging")


def test_rollback_swaps_back(project):
    lifecycle.install(_args(project))
    lifecycle.update(_args(project))
    assert os.path.islink(project / "result-testemu-prev")
    lifecycle.rollback(_args(project))
    # prev should be gone, current restored
    assert not os.path.lexists(project / "result-testemu-prev")
    assert os.path.islink(project / "result-testemu")


def test_rollback_with_no_prev_is_noop(project):
    lifecycle.install(_args(project))
    n = lifecycle.rollback(_args(project))
    # No prev recorded - no rollback happened
    assert n == 0


def test_uninstall_removes_links_keeps_data(project):
    lifecycle.install(_args(project))
    settings = project / "TestEmu" / "config" / "settings.ini"
    assert settings.exists()
    n = lifecycle.uninstall(_args(project))
    assert n == 1
    # Project-dir data preserved
    assert settings.exists()
    # Host link gone
    assert not (project / "host" / "TestEmu" / "settings.ini").exists()


def test_install_filtered_to_specific_emulator(project, tmp_path):
    # Add a second emulator
    other = tmp_path / "OtherEmu"
    (other / "config").mkdir(parents=True)
    (other / "config" / "x").write_text("")
    (other / "symlinks.json").write_text(json.dumps({
        "config": {"linux": str(tmp_path / "host" / "OtherEmu"),
                   "macos": str(tmp_path / "host" / "OtherEmu"),
                   "windows": str(tmp_path / "host" / "OtherEmu")}
    }))

    args = argparse.Namespace(config=str(project / "setup.json"), emulators=["TestEmu"])
    n = lifecycle.install(args)
    assert n == 1
    assert os.path.islink(project / "result-testemu")
    assert not os.path.lexists(project / "result-otheremu")


def test_install_handles_no_matches(project):
    args = argparse.Namespace(config=str(project / "setup.json"), emulators=["NonExistent"])
    assert lifecycle.install(args) == 0


def test_update_refuses_to_overwrite_real_dir(project):
    """If `result-<emu>-prev` is a real directory (not a symlink), don't clobber."""
    lifecycle.install(_args(project))
    # Pretend something else dropped a real dir at the prev path
    prev = project / "result-testemu-prev"
    prev.mkdir()
    (prev / "important_file").write_text("don't delete me")

    lifecycle.update(_args(project))
    # The real directory should still be there
    assert (prev / "important_file").exists()
