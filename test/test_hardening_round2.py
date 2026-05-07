"""Tests covering the round-2 hardening fixes."""

import argparse
import json
import os

import pytest

from core import controllers, lifecycle, logger as logger_mod, state, syncthing
from core.steam import _BINARY_PREFERENCE


# ----- R2-1: Linux Ryujinx binary path is lowercase -----

def test_binary_preference_uses_lowercase_ryujinx():
    """Our nix wrapper at nix/ryujinx.nix:73 emits bin/ryujinx (lowercase).
    Picking by sort order would give 'Ryujinx' on a case-sensitive fs."""
    assert _BINARY_PREFERENCE["ryujinx"][0] == "ryujinx"


def test_generate_find_rules_linux_ryujinx_path(monkeypatch):
    """Linux find-rules must point at bin/ryujinx (lowercase)."""
    monkeypatch.setattr("generate_find_rules.PLATFORM", "linux")
    import importlib, generate_find_rules
    importlib.reload(generate_find_rules)
    paths = generate_find_rules._emulator_paths("/some/result")
    assert paths["RYUJINX"].endswith("/bin/ryujinx"), paths["RYUJINX"]


# ----- R2-3: Lime3DS fully removed -----

def test_lime3ds_not_in_save_paths():
    assert "Lime3DS" not in syncthing.SAVE_PATHS


def test_lime3ds_not_in_profile_targets():
    assert "Lime3DS" not in controllers.PROFILE_TARGETS


def test_lime3ds_dir_is_gone():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assert not os.path.isdir(os.path.join(repo_root, "Lime3DS"))


def test_lime3ds_controller_fragments_gone():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assert not os.path.exists(os.path.join(repo_root, "controllers", "xbox", "lime3ds.ini"))
    assert not os.path.exists(os.path.join(repo_root, "controllers", "dualsense", "lime3ds.ini"))


def test_lime3ds_not_in_gui_manifest():
    from gui.manifest import EMULATORS
    assert all(m.name != "Lime3DS" for m in EMULATORS)


# ----- R2-5: lifecycle recovery -----

def test_detect_interrupted_updates_finds_orphaned_staging(tmp_path):
    """When result-<emu>-staging exists but result-<emu> does not, that
    emulator needs recovery."""
    fake_store = tmp_path / "store"
    fake_store.mkdir()
    (tmp_path / "result-dolphin-staging").symlink_to(fake_store)
    pending = lifecycle.detect_interrupted_updates(str(tmp_path))
    assert pending == ["dolphin"]


def test_detect_interrupted_updates_ignores_complete_swap(tmp_path):
    """If both result-<emu> and result-<emu>-staging exist (build done but
    rotation hasn't started), don't claim recovery is needed — only the
    stranded case (staging without current) matters."""
    fake_store = tmp_path / "store"
    fake_store.mkdir()
    (tmp_path / "result-dolphin").symlink_to(fake_store)
    (tmp_path / "result-dolphin-staging").symlink_to(fake_store)
    pending = lifecycle.detect_interrupted_updates(str(tmp_path))
    assert pending == []


def test_recover_interrupted_update_promotes_staging(tmp_path):
    fake_store = tmp_path / "store"
    fake_store.mkdir()
    (tmp_path / "result-dolphin-staging").symlink_to(fake_store)
    assert lifecycle.recover_interrupted_update(str(tmp_path), "dolphin") is True
    assert os.path.islink(tmp_path / "result-dolphin")
    assert not os.path.lexists(tmp_path / "result-dolphin-staging")


def test_recover_interrupted_update_refuses_when_current_exists(tmp_path):
    fake_store = tmp_path / "store"
    fake_store.mkdir()
    (tmp_path / "result-dolphin").symlink_to(fake_store)
    (tmp_path / "result-dolphin-staging").symlink_to(fake_store)
    assert lifecycle.recover_interrupted_update(str(tmp_path), "dolphin") is False


# ----- R2-6: PII regex tightening -----

def test_pii_regex_does_not_redact_macos_shared(tmp_path, monkeypatch):
    """macOS /Users/Shared is a system path, not a user home — must not
    be scrubbed (would destroy useful diagnostics)."""
    monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path))
    monkeypatch.setattr(logger_mod, "_LOGGER", None)
    log = logger_mod.get_logger()
    log.info("touched /Users/Shared/com.apple.shared.txt")
    for h in log.handlers:
        if hasattr(h, "flush"):
            h.flush()
    contents = open(os.path.join(str(tmp_path), "schemulator", "schemulator.log")).read()
    assert "/Users/Shared/" in contents
    assert "<USER>" not in contents


def test_pii_regex_does_not_redact_ci_paths(tmp_path, monkeypatch):
    """/home/runner is GitHub-hosted CI; not user PII."""
    monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path))
    monkeypatch.setattr(logger_mod, "_LOGGER", None)
    log = logger_mod.get_logger()
    log.info("worked at /home/runner/work/schemulator/schemulator")
    for h in log.handlers:
        if hasattr(h, "flush"):
            h.flush()
    contents = open(os.path.join(str(tmp_path), "schemulator", "schemulator.log")).read()
    # `runner` should NOT have been replaced by <USER>.
    assert "/home/runner/" in contents


def test_pii_regex_does_redact_real_user_paths(tmp_path, monkeypatch):
    """A path that looks like /Users/<name>/Document or /home/<name>/.config
    should still be scrubbed."""
    monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path))
    monkeypatch.setattr(logger_mod, "_LOGGER", None)
    log = logger_mod.get_logger()
    log.info("loaded /home/jdoe/Documents/notes")
    for h in log.handlers:
        if hasattr(h, "flush"):
            h.flush()
    contents = open(os.path.join(str(tmp_path), "schemulator", "schemulator.log")).read()
    assert "<USER>" in contents
    assert "jdoe" not in contents


# ----- R2-9: atomic settings write -----

def test_settings_dialog_writes_atomically(tmp_path):
    """Crash mid-save must not leave an empty/truncated setup.json. We
    verify by patching json.dump to raise, then checking the file is intact.
    """
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    import sys
    from PySide6.QtWidgets import QApplication
    QApplication.instance() or QApplication(sys.argv)

    cfg_path = tmp_path / "setup.json"
    original_cfg = {"host": {state.PLATFORM: "/originally-set"},
                    "portable": {state.PLATFORM: "/portable-set"}}
    cfg_path.write_text(json.dumps(original_cfg, indent=4))

    from gui.dialogs.settings import SettingsDialog
    d = SettingsDialog(str(tmp_path))
    d._host_input[1].setText("/new-host")
    d._portable_input[1].setText("/new-portable")

    # Patch json.dump in settings module to raise mid-write.
    import gui.dialogs.settings as settings_mod
    real_dump = settings_mod.json.dump

    def boom(*a, **kw):
        raise OSError("simulated disk full")

    settings_mod.json.dump = boom
    try:
        # Suppress the QMessageBox the dialog would otherwise pop.
        from PySide6.QtWidgets import QMessageBox
        original_critical = QMessageBox.critical
        QMessageBox.critical = staticmethod(lambda *a, **kw: None)
        try:
            d._save()
        finally:
            QMessageBox.critical = original_critical
    finally:
        settings_mod.json.dump = real_dump

    # Original setup.json must still be readable + intact.
    parsed = json.loads(cfg_path.read_text())
    assert parsed["host"][state.PLATFORM] == "/originally-set"
    # And no .tmp left behind (we cleaned up).
    assert not (tmp_path / "setup.json.tmp").exists()


# ----- R2-7: Ares is now a managed emulator -----

def test_ares_has_symlinks_json():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assert os.path.isfile(os.path.join(repo_root, "Ares", "symlinks.json"))


def test_ares_in_gui_manifest():
    from gui.manifest import EMULATORS
    assert any(m.name == "Ares" for m in EMULATORS)


# ----- R2-8: steamdeck dropped from PROFILES -----

def test_steamdeck_not_in_profiles():
    assert "steamdeck" not in controllers.PROFILES
