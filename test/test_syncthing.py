"""Tests for the Syncthing sidecar wrapper.

Network calls are intentionally not exercised — we test the offline pieces
(folder discovery, save-link population, config XML parsing) which is what
breaks most often during refactors.
"""

import os

from core import syncthing


def test_saves_dir(tmp_path):
    assert syncthing.saves_dir(str(tmp_path)) == str(tmp_path / "saves")


def test_populate_save_links_creates_symlinks(tmp_path):
    project = tmp_path / "project"
    (project / "RetroArch" / "config" / "saves").mkdir(parents=True)
    (project / "RetroArch" / "config" / "states").mkdir(parents=True)
    (project / "Dolphin" / "data" / "GC").mkdir(parents=True)

    saves = project / "saves"
    syncthing._populate_save_links(str(project), str(saves))

    assert (saves / "RetroArch" / "saves").is_symlink()
    assert (saves / "RetroArch" / "states").is_symlink()
    assert (saves / "Dolphin" / "GC").is_symlink()


def test_populate_skips_missing_emulators(tmp_path):
    project = tmp_path / "project"
    project.mkdir()
    syncthing._populate_save_links(str(project), str(project / "saves"))
    # Saves root not even created since nothing to link.
    assert not (project / "saves").exists() or not list((project / "saves").iterdir())


def test_qr_payload_format():
    payload = syncthing.device_id_qr_payload("ABCDEFG-HIJKLMN")
    assert payload.startswith("syncthing://")
    assert "ABCDEFG" in payload


def test_device_id_returns_none_when_no_config(tmp_path):
    assert syncthing.device_id(str(tmp_path)) is None


def test_device_id_parses_xml(tmp_path):
    cfg = tmp_path / "config.xml"
    cfg.write_text(
        '<configuration><device id="ABCDEF" name="local"></device></configuration>'
    )
    assert syncthing.device_id(str(tmp_path)) == "ABCDEF"


def test_api_key_parses_xml(tmp_path):
    cfg = tmp_path / "config.xml"
    cfg.write_text(
        '<configuration><gui><apikey>secret123</apikey></gui></configuration>'
    )
    assert syncthing.api_key(str(tmp_path)) == "secret123"


def test_find_binary_respects_env(tmp_path, monkeypatch):
    fake = tmp_path / "syncthing"
    fake.write_text("#!/bin/sh\nexit 0")
    fake.chmod(0o755)
    monkeypatch.setenv("SCHEMULATOR_SYNCTHING", str(fake))
    assert syncthing.find_binary() == str(fake)
