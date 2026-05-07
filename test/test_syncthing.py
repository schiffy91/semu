"""Tests for the Syncthing sidecar wrapper.

The "offline" tests (config parsing, save-link population, binary discovery)
always run. The "live" tests exercise the real REST API and only run if a
syncthing binary is available on $SCHEMULATOR_SYNCTHING / bin/syncthing.
"""

import os
import subprocess
import tempfile

import pytest

from core import syncthing


def _have_syncthing() -> bool:
    return syncthing.find_binary() is not None


live = pytest.mark.skipif(not _have_syncthing(), reason="syncthing binary not bundled")


# Saves now sync via the whole project dir filtered by .stignore (round-6
# rework); the legacy saves/ shadow tree, _populate_save_links, saves_dir,
# and device_id_qr_payload were removed. Their old tests went with them.


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


# ----- live tests (skipped without a real syncthing binary) -----


@live
def test_init_generates_config(tmp_path):
    home = str(tmp_path / "syncthing-home")
    assert syncthing.init(home) is True
    assert os.path.exists(os.path.join(home, "config.xml"))
    assert syncthing.device_id(home)
    assert syncthing.api_key(home)


@live
def test_start_serves_rest_api(tmp_path):
    home = str(tmp_path / "syncthing-home")
    syncthing.init(home)
    proc = syncthing.start(home, wait_for_ready=20)
    try:
        assert proc is not None
        assert proc.poll() is None  # still running
        assert syncthing._ping(home, timeout=2) is True
    finally:
        syncthing.stop(proc)


@live
def test_add_folder_round_trip(tmp_path):
    home = str(tmp_path / "syncthing-home")
    syncthing.init(home)
    proc = syncthing.start(home, wait_for_ready=20)
    try:
        project = tmp_path / "project"
        (project / "RetroArch" / "config" / "saves").mkdir(parents=True)
        assert syncthing.add_folder(str(project), home) is True
    finally:
        syncthing.stop(proc)


@live
def test_add_device_round_trip(tmp_path):
    home = str(tmp_path / "syncthing-home")
    syncthing.init(home)
    proc = syncthing.start(home, wait_for_ready=20)
    try:
        # Generate a separate config to harvest a check-digit-valid peer ID.
        peer_home = tmp_path / "peer-home"
        binary = syncthing.find_binary()
        subprocess.run(
            [binary, "generate", f"--home={peer_home}", "--no-port-probing"],
            capture_output=True, check=True,
        )
        peer_id = syncthing.device_id(str(peer_home))
        assert peer_id, "peer config generation failed"
        assert syncthing.add_device(peer_id, "PeerDeck", home) is True
    finally:
        syncthing.stop(proc)
