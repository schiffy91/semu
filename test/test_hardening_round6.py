"""Tests for round-6 UX hardening fixes."""

import argparse
import json
import os
import xml.etree.ElementTree as ET

import pytest

from core import lifecycle, sdcard, state, symlinks


# ----- R6-2: SteamDeck Apply wires ES-DE -----

def test_wire_es_de_to_card_writes_settings(tmp_path, monkeypatch):
    """The Apply button must actually point ES-DE at the chosen card's
    Emulation/roms/ root. Without this the Steam Deck flow ended in a
    Game Mode session with an empty ROM library."""
    monkeypatch.setenv("HOME", str(tmp_path))
    card_root = tmp_path / "sdcard"
    (card_root / "Emulation" / "roms" / "snes").mkdir(parents=True)
    (card_root / "Emulation" / "roms" / "snes" / "x.sfc").write_bytes(b"\0")
    card = sdcard._inspect_mount(str(card_root), "sdcard")

    written = sdcard.wire_es_de_to_card(card, str(tmp_path))
    assert written
    assert os.path.exists(written)

    tree = ET.parse(written)
    root = tree.getroot()
    rom_dir = next(
        (el for el in root.findall("string") if el.get("name") == "ROMDirectory"),
        None,
    )
    assert rom_dir is not None
    assert rom_dir.get("value") == str(card_root / "Emulation" / "roms")


def test_wire_es_de_to_card_preserves_other_settings(tmp_path, monkeypatch):
    """If the user already has es_settings.xml with custom values, the
    ROMDirectory upsert must not blow them away."""
    monkeypatch.setenv("HOME", str(tmp_path))
    settings_dir = tmp_path / "ES-DE" / "settings"
    settings_dir.mkdir(parents=True)
    settings = settings_dir / "es_settings.xml"
    existing = ET.Element("settings")
    ET.SubElement(existing, "string", {"name": "Theme", "value": "linea"})
    ET.SubElement(existing, "bool",   {"name": "FullscreenMode", "value": "true"})
    ET.ElementTree(existing).write(settings)

    card_root = tmp_path / "sdcard"
    (card_root / "Emulation" / "roms").mkdir(parents=True)
    card = sdcard._inspect_mount(str(card_root), "sdcard")
    sdcard.wire_es_de_to_card(card, str(tmp_path))

    parsed = ET.parse(settings).getroot()
    theme = next((el for el in parsed.findall("string") if el.get("name") == "Theme"), None)
    fullscreen = next((el for el in parsed.findall("bool") if el.get("name") == "FullscreenMode"), None)
    assert theme is not None and theme.get("value") == "linea"
    assert fullscreen is not None and fullscreen.get("value") == "true"


# ----- R6-3: existing-saves migration -----

def test_create_symlink_migrate_copies_user_data(tmp_path):
    """When migrate=True, pre-existing data at the link path is copied into
    the project source dir before the symlink replaces it."""
    source = tmp_path / "source"
    source.mkdir()
    # Project already has its own newer save — must NOT be overwritten.
    (source / "shared.bin").write_text("project-version")

    link = tmp_path / "link"
    link.mkdir()
    (link / "shared.bin").write_text("local-version")  # should be skipped
    (link / "unique-save.dat").write_text("user-only-save")

    symlinks.create_symlink(str(link), str(source), migrate=True)

    # Link is now a symlink.
    assert os.path.islink(link)
    # Project's shared.bin wins.
    assert (source / "shared.bin").read_text() == "project-version"
    # User's unique save migrated in.
    assert (source / "unique-save.dat").read_text() == "user-only-save"


def test_detect_existing_user_data_finds_orphan_dir(tmp_path, monkeypatch):
    """detect_existing_user_data should surface emulators where the OS link
    path has accumulated user data (so the GUI can prompt to migrate)."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Dolphin"
    (emu / "data").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "data": {"linux": str(tmp_path / "h" / "Dolphin")}
    }))

    # Pretend the user had a prior install at the OS path.
    (tmp_path / "h" / "Dolphin").mkdir(parents=True)
    (tmp_path / "h" / "Dolphin" / "save1.bin").write_text("legacy")

    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    detected = symlinks.detect_existing_user_data(parsed)
    assert "DOLPHIN" in detected


def test_detect_existing_user_data_ignores_dir_of_symlinks(tmp_path, monkeypatch):
    """A directory of symlinks (i.e. our own wiring from a previous run) is
    NOT user data — schemulator can replace it freely."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Dolphin"
    (emu / "data").mkdir(parents=True)
    (emu / "data" / "x").write_text("x")
    (emu / "symlinks.json").write_text(json.dumps({
        "data": {"linux": str(tmp_path / "h" / "Dolphin")}
    }))

    target_dir = tmp_path / "h" / "Dolphin"
    target_dir.mkdir(parents=True)
    # A previous schemulator run left only symlinks here:
    (target_dir / "x").symlink_to(emu / "data" / "x")

    parsed = symlinks.parse_config(str(tmp_path / "setup.json"), str(tmp_path))
    detected = symlinks.detect_existing_user_data(parsed)
    assert "DOLPHIN" not in detected


# ----- R6-4: sync architecture shares project_dir directly -----

def test_add_folder_targets_project_root(tmp_path, monkeypatch):
    """The shared folder is now <project>, not <project>/saves/. The legacy
    saves/ path entry must NOT be created (the .stignore even excludes
    `saves` so any legacy artifact doesn't ship to peers)."""
    from core import syncthing
    monkeypatch.setattr(syncthing, "api_key", lambda *a, **kw: None)
    project = tmp_path / "project"
    project.mkdir()
    syncthing.add_folder(str(project))
    # No saves/ dir should be auto-created.
    assert not (project / "saves").exists()


# ----- R6-5: running-process check -----

def test_running_emulators_returns_empty_when_none(monkeypatch):
    """When ps shows no emulator processes, return []."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    import subprocess as _sp
    monkeypatch.setattr(
        _sp, "run",
        lambda *a, **kw: type("R", (), {"stdout": "bash\nps\n"})(),
    )
    assert lifecycle.running_emulators(["Dolphin", "PCSX2"]) == []


def test_running_emulators_detects_dolphin(monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "linux")
    import subprocess as _sp
    monkeypatch.setattr(
        _sp, "run",
        lambda *a, **kw: type("R", (), {"stdout": "bash\ndolphin-emu\nps\n"})(),
    )
    assert "dolphin" in lifecycle.running_emulators(["Dolphin"])


def test_running_emulators_skips_on_unsupported_platform(monkeypatch):
    monkeypatch.setattr(state, "PLATFORM", "windows")
    assert lifecycle.running_emulators(["Dolphin"]) == []


# ----- R6-6: uninstall refuses real-data dirs -----

def test_uninstall_refuses_to_delete_real_user_data(tmp_path, monkeypatch, capsys):
    """If the OS link path turned into a real dir with real files (e.g. the
    user's symlink broke and the emulator created data there directly),
    uninstall must skip + warn, not rmtree."""
    monkeypatch.setattr(state, "PLATFORM", "linux")
    cfg = {"host": {"linux": str(tmp_path / "h")}, "portable": {"linux": str(tmp_path / "p")}}
    (tmp_path / "setup.json").write_text(json.dumps(cfg))
    emu = tmp_path / "Dolphin"
    (emu / "data").mkdir(parents=True)
    (emu / "symlinks.json").write_text(json.dumps({
        "data": {"linux": str(tmp_path / "h" / "Dolphin")}
    }))

    # Real data accumulated at the link path:
    target = tmp_path / "h" / "Dolphin"
    target.mkdir(parents=True)
    save = target / "important.bin"
    save.write_text("don't lose me")

    args = argparse.Namespace(config=str(tmp_path / "setup.json"), emulators=["Dolphin"])
    lifecycle.uninstall(args)

    # The user's data must still be there.
    assert save.exists()
    # And there's a warning in stdout.
    out = capsys.readouterr().out
    assert "Skipping" in out


# ----- R6-8: update skips up-to-date emulators -----

def test_filter_outdated_drops_matching_versions(tmp_path, monkeypatch):
    """When manifest version == installed version, the emulator is skipped."""
    from core import updater
    # Stub the manifest to match what's installed.
    (tmp_path / "Dolphin").mkdir()
    (tmp_path / "Dolphin" / "version.txt").write_text("2603a")
    (tmp_path / "PCSX2").mkdir()
    (tmp_path / "PCSX2" / "version.txt").write_text("2.6.3")

    fake_manifest = updater.Manifest(
        schemulator_version="1.0",
        emulators={
            "dolphin": {"version": "2603a"},   # match → skip
            "pcsx2":   {"version": "2.6.4"},   # newer → update
        },
    )
    monkeypatch.setattr(updater, "fetch_manifest", lambda *a, **kw: fake_manifest)

    out = lifecycle._filter_outdated(
        {"DOLPHIN": ["entries-don't-matter"], "PCSX2": ["entries-don't-matter"]},
        str(tmp_path),
    )
    assert "DOLPHIN" not in out
    assert "PCSX2" in out


def test_filter_outdated_falls_through_on_no_manifest(tmp_path, monkeypatch):
    """Offline / no manifest: don't block — let user rebuild everything."""
    from core import updater
    monkeypatch.setattr(updater, "fetch_manifest", lambda *a, **kw: None)
    emulators = {"DOLPHIN": ["x"], "PCSX2": ["y"]}
    out = lifecycle._filter_outdated(emulators, str(tmp_path))
    assert out == emulators


# ----- R6-7: first-run wizard exposes chosen SD card -----

def test_first_run_wizard_chosen_sd_card_returns_card(tmp_path, monkeypatch):
    """The wizard's chosen_sd_card method should expose page-2's selection
    so the install flow can wire ES-DE without a separate dialog visit."""
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    import sys
    from PySide6.QtWidgets import QApplication
    QApplication.instance() or QApplication(sys.argv)

    # Build a fake mount and force list_sdcards to return it.
    mount = tmp_path / "fake-sd"
    (mount / "Emulation" / "roms" / "snes").mkdir(parents=True)
    (mount / "Emulation" / "roms" / "snes" / "x.sfc").write_bytes(b"\0")
    fake_card = sdcard._inspect_mount(str(mount), "fake-sd")
    monkeypatch.setattr(sdcard, "list_sdcards", lambda: [fake_card])

    from gui.dialogs.first_run import FirstRunWizard
    w = FirstRunWizard(str(tmp_path))
    chosen = w.chosen_sd_card()
    assert chosen is not None
    assert chosen.label == "fake-sd"
