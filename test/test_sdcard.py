"""Tests for SD-card detection and ROM scanning."""

import os

from core import sdcard


def test_emudeck_layout_detected(tmp_path):
    roms = tmp_path / "Emulation" / "roms"
    (roms / "snes").mkdir(parents=True)
    (roms / "snes" / "mario.sfc").write_bytes(b"")
    (roms / "n64").mkdir()
    (roms / "n64" / "zelda.z64").write_bytes(b"")

    found = sdcard.scan_roms(str(tmp_path))
    assert "snes" in found
    assert "n64" in found
    assert any(p.endswith("mario.sfc") for p in found["snes"])


def test_extension_fallback_when_no_emudeck_layout(tmp_path):
    (tmp_path / "Stuff").mkdir()
    (tmp_path / "Stuff" / "game.gba").write_bytes(b"")
    (tmp_path / "Stuff" / "switch.nsp").write_bytes(b"")
    (tmp_path / "Stuff" / "ignore.txt").write_bytes(b"")

    found = sdcard.scan_roms(str(tmp_path))
    assert "gba" in found
    assert "switch" in found
    assert "txt" not in found


def test_inspect_mount_marks_emudeck(tmp_path):
    (tmp_path / "Emulation" / "roms" / "ps2").mkdir(parents=True)
    (tmp_path / "Emulation" / "roms" / "ps2" / "game.iso").write_bytes(b"")
    card = sdcard._inspect_mount(str(tmp_path), "card")
    assert card.has_emudeck_layout is True
    assert "ps2" in card.rom_systems


def test_best_card_prefers_emudeck(tmp_path):
    a = sdcard.SdCard(mount_path=str(tmp_path / "a"), label="a", has_emudeck_layout=False, rom_systems={"snes": ["x"] * 100})
    b = sdcard.SdCard(mount_path=str(tmp_path / "b"), label="b", has_emudeck_layout=True, rom_systems={"snes": ["x"]})
    assert sdcard.best_card([a, b]) is b


def test_best_card_handles_empty():
    assert sdcard.best_card([]) is None


def test_check_firmware_reports_missing(tmp_path):
    pcsx2 = tmp_path / "PCSX2"
    pcsx2.mkdir()
    missing = sdcard.check_firmware(str(tmp_path))
    assert "PCSX2" in missing
    assert "PS2 BIOS" in missing["PCSX2"]


def test_check_firmware_clears_when_present(tmp_path):
    bios = tmp_path / "PCSX2" / "bios"
    bios.mkdir(parents=True)
    (bios / "ps2-0230a.bin").write_bytes(b"")
    missing = sdcard.check_firmware(str(tmp_path))
    assert "PCSX2" not in missing


def test_check_firmware_skips_uninstalled_emulators(tmp_path):
    # Emulator dir doesn't exist - no warning emitted.
    missing = sdcard.check_firmware(str(tmp_path))
    assert missing == {}


def test_check_firmware_returns_descriptions(tmp_path):
    for emu in ("PCSX2", "Ryujinx", "Cemu", "Azahar"):
        (tmp_path / emu).mkdir()
    missing = sdcard.check_firmware(str(tmp_path))
    assert set(missing.keys()) == {"PCSX2", "Ryujinx", "Cemu", "Azahar"}
    for desc in missing.values():
        assert isinstance(desc, str) and len(desc) > 0
