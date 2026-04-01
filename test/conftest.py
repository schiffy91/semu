import json
import os
import struct
import sys
import tempfile

import pytest

# Add project root to path so we can import setup and decrypt3ds
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

PLATFORM = {"win32": "windows", "darwin": "macos", "linux": "linux"}.get(sys.platform, "linux")


@pytest.fixture
def temp_dir(tmp_path):
    """Provides a temporary directory that's cleaned up after the test."""
    return tmp_path


@pytest.fixture
def mock_project(tmp_path):
    """Creates a mock project structure with config and emulator manifests."""
    # setup.json
    config = {
        "host": {
            "windows": str(tmp_path / "host_win"),
            "linux": str(tmp_path / "host_linux"),
            "macos": str(tmp_path / "host_macos"),
        },
        "portable": {
            "windows": str(tmp_path / "portable_win"),
            "linux": str(tmp_path / "portable_linux"),
            "macos": str(tmp_path / "portable_macos"),
        },
    }
    (tmp_path / "setup.json").write_text(json.dumps(config))

    # A mock emulator with symlinks.json
    emu_dir = tmp_path / "TestEmu"
    emu_dir.mkdir()
    config_dir = emu_dir / "config"
    config_dir.mkdir()
    (config_dir / "settings.ini").write_text("[General]\nfullscreen=true\n")

    symlinks = {
        "config": {
            "windows": str(tmp_path / "host_win" / "TestEmu"),
            "linux": str(tmp_path / "host_linux" / "TestEmu"),
            "macos": str(tmp_path / "host_macos" / "TestEmu"),
        }
    }
    (emu_dir / "symlinks.json").write_text(json.dumps(symlinks))

    return tmp_path


def build_test_3ds(path, no_crypto=False):
    """Build a minimal synthetic .3ds file with valid NCSD/NCCH headers.

    This is NOT a real ROM — just enough structure to test the flag-fix logic.
    """
    # We need: NCSD header (0x4000 bytes) + NCCH partition
    # Total: NCSD header region (0x4000) + NCCH (0x1000 min)
    data = bytearray(0x5000)

    # NCSD magic at 0x100
    struct.pack_into("4s", data, 0x100, b"NCSD")
    # Image size in media units
    struct.pack_into("<I", data, 0x104, len(data) // 0x200)

    # Partition table at 0x120: partition 0 at offset 0x4000, size 0x1000
    struct.pack_into("<II", data, 0x120, 0x4000 // 0x200, 0x1000 // 0x200)

    # NCCH at 0x4000
    ncch_base = 0x4000

    # NCCH magic at ncch_base + 0x100
    struct.pack_into("4s", data, ncch_base + 0x100, b"NCCH")
    # Content size
    struct.pack_into("<I", data, ncch_base + 0x104, 0x1000 // 0x200)
    # Version = 2
    struct.pack_into("<H", data, ncch_base + 0x112, 2)

    # ExeFS offset (in media units from NCCH start) at ncch_base + 0x100 + 0xA0
    exefs_offset_mu = 0x800 // 0x200  # 4 media units in
    struct.pack_into("<I", data, ncch_base + 0x1A0, exefs_offset_mu)
    struct.pack_into("<I", data, ncch_base + 0x1A4, 1)  # size = 1 media unit

    # Write a fake ExeFS header with ".code" to look decrypted
    exefs_abs = ncch_base + 0x800
    data[exefs_abs:exefs_abs + 5] = b".code"

    # Crypto flags at ncch_base + 0x100 + 0x8F
    flags_offset = ncch_base + 0x18F
    if no_crypto:
        data[flags_offset] = 0x04  # NoCrypto bit set
    else:
        data[flags_offset] = 0x00  # NoCrypto bit not set

    with open(path, "wb") as f:
        f.write(data)


@pytest.fixture
def test_rom_encrypted(tmp_path):
    """Synthetic .3ds with NoCrypto flag NOT set (needs fix)."""
    path = tmp_path / "test_encrypted.3ds"
    build_test_3ds(str(path), no_crypto=False)
    return path


@pytest.fixture
def test_rom_decrypted(tmp_path):
    """Synthetic .3ds with NoCrypto flag already set."""
    path = tmp_path / "test_decrypted.3ds"
    build_test_3ds(str(path), no_crypto=True)
    return path
