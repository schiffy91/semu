import os
import struct
import shutil

import decrypt3ds


def test_check_identifies_needs_fix(test_rom_encrypted):
    result = decrypt3ds.check_file(str(test_rom_encrypted))
    assert result["error"] is None
    assert result["needs_fix"] is True
    assert result["already_ok"] is False
    assert result["truly_encrypted"] is False


def test_check_identifies_already_ok(test_rom_decrypted):
    result = decrypt3ds.check_file(str(test_rom_decrypted))
    assert result["error"] is None
    assert result["already_ok"] is True
    assert result["needs_fix"] is False


def test_fix_sets_nocrypto_flag(test_rom_encrypted, tmp_path):
    output = tmp_path / "output" / test_rom_encrypted.name
    output.parent.mkdir()
    assert decrypt3ds.fix_file(str(test_rom_encrypted), str(output))

    # Verify output has NoCrypto set
    result = decrypt3ds.check_file(str(output))
    assert result["already_ok"] is True
    assert result["needs_fix"] is False


def test_fix_does_not_modify_original(test_rom_encrypted, tmp_path):
    original_bytes = test_rom_encrypted.read_bytes()
    output = tmp_path / "output" / test_rom_encrypted.name
    output.parent.mkdir()
    decrypt3ds.fix_file(str(test_rom_encrypted), str(output))

    # Original must be byte-identical to before
    assert test_rom_encrypted.read_bytes() == original_bytes


def test_fix_preserves_file_size(test_rom_encrypted, tmp_path):
    original_size = test_rom_encrypted.stat().st_size
    output = tmp_path / "output" / test_rom_encrypted.name
    output.parent.mkdir()
    decrypt3ds.fix_file(str(test_rom_encrypted), str(output))

    assert output.stat().st_size == original_size


def test_already_ok_rom_is_copied_not_modified(test_rom_decrypted, tmp_path):
    original_bytes = test_rom_decrypted.read_bytes()
    output = tmp_path / "output" / test_rom_decrypted.name
    output.parent.mkdir()
    shutil.copy2(str(test_rom_decrypted), str(output))

    # Output should be byte-identical
    assert output.read_bytes() == original_bytes


def test_check_invalid_file(tmp_path):
    bad_file = tmp_path / "garbage.3ds"
    bad_file.write_bytes(b"\x00" * 1024)
    result = decrypt3ds.check_file(str(bad_file))
    assert result["error"] == "Not a valid NCSD file"
