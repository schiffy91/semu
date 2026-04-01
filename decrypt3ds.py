#!/usr/bin/env python3
"""
Fix 3DS ROM files (.3ds / NCSD) that are decrypted but missing the NoCrypto flag.

Many ROM dumping tools produce decrypted .3ds files without setting the NoCrypto
flag in the NCCH header. Emulators like Azahar refuse to load these because they
see the flag and assume encryption. This tool copies the ROM and sets the flag.

Usage:
  python decrypt3ds.py ROMs/n3ds/ --check              # Report flag status
  python decrypt3ds.py "path/to/game.3ds" -o output/   # Fix one ROM
  python decrypt3ds.py ROMs/n3ds/ -o ROMs/n3ds-fixed/  # Batch fix all
"""

import argparse
import os
import shutil
import struct
import sys
from pathlib import Path

MEDIA_UNIT = 0x200


def parse_ncsd_partitions(f):
    """Parse NCSD header and return partition (index, offset, size) tuples."""
    f.seek(0x100)
    if f.read(4) != b"NCSD":
        return None
    f.seek(0x120)
    partitions = []
    for i in range(8):
        offset_mu, size_mu = struct.unpack("<II", f.read(8))
        if size_mu > 0:
            partitions.append((i, offset_mu * MEDIA_UNIT, size_mu * MEDIA_UNIT))
    return partitions


def check_ncch_partition(f, part_offset):
    """Check a single NCCH partition. Returns dict with status info."""
    f.seek(part_offset + 0x100)
    hdr = f.read(0x100)
    if len(hdr) < 0xC0 or hdr[0:4] != b"NCCH":
        return None

    crypto_flags = hdr[0x8F]
    no_crypto = bool(crypto_flags & 0x04)
    crypto_method = hdr[0x8B]

    # Check if content looks decrypted by reading ExeFS header
    exefs_off_mu = struct.unpack_from("<I", hdr, 0xA0)[0]
    content_decrypted = False
    if exefs_off_mu > 0:
        f.seek(part_offset + exefs_off_mu * MEDIA_UNIT)
        exefs_hdr = f.read(8)
        # If ExeFS header starts with a readable section name like ".code", it's decrypted
        if exefs_hdr.startswith(b".code") or exefs_hdr.startswith(b"icon\x00") or exefs_hdr.startswith(b"banner"):
            content_decrypted = True

    return {
        "no_crypto_flag": no_crypto,
        "crypto_method": crypto_method,
        "content_decrypted": content_decrypted,
    }


def check_file(filepath):
    """Check a .3ds file. Returns summary dict."""
    result = {
        "path": filepath,
        "needs_fix": False,
        "already_ok": False,
        "truly_encrypted": False,
        "error": None,
        "partitions": 0,
    }
    try:
        with open(filepath, "rb") as f:
            parts = parse_ncsd_partitions(f)
            if parts is None:
                result["error"] = "Not a valid NCSD file"
                return result

            for idx, offset, size in parts:
                info = check_ncch_partition(f, offset)
                if info is None:
                    continue
                result["partitions"] += 1

                if info["no_crypto_flag"]:
                    result["already_ok"] = True
                elif info["content_decrypted"]:
                    result["needs_fix"] = True
                else:
                    result["truly_encrypted"] = True
    except Exception as e:
        result["error"] = str(e)
    return result


def fix_file(input_path, output_path):
    """Copy a .3ds file and set NoCrypto flag on all NCCH partitions."""
    shutil.copy2(input_path, output_path)

    with open(output_path, "r+b") as f:
        parts = parse_ncsd_partitions(f)
        if parts is None:
            return False

        for idx, offset, size in parts:
            f.seek(offset + 0x100)
            hdr = f.read(0x100)
            if len(hdr) < 0xC0 or hdr[0:4] != b"NCCH":
                continue

            crypto_flags = hdr[0x8F]
            if crypto_flags & 0x04:
                continue  # Already has NoCrypto set

            # Set NoCrypto bit (bit 2) in crypto flags byte
            f.seek(offset + 0x100 + 0x8F)
            f.write(struct.pack("B", crypto_flags | 0x04))

            # Clear secondary_key_slot to 0
            f.seek(offset + 0x100 + 0x8B)
            f.write(struct.pack("B", 0))

    return True


def main():
    parser = argparse.ArgumentParser(
        description="Fix 3DS ROMs: set NoCrypto flag on already-decrypted dumps"
    )
    parser.add_argument("input", help="Input .3ds file or directory")
    parser.add_argument("-o", "--output", help="Output directory for fixed files")
    parser.add_argument("--check", action="store_true", help="Only check status, don't fix")
    args = parser.parse_args()

    input_path = Path(args.input)
    if input_path.is_dir():
        files = sorted(input_path.glob("*.3ds"))
        if not files:
            files = sorted(input_path.glob("*.3DS"))
    elif input_path.is_file():
        files = [input_path]
    else:
        print(f"Error: {input_path} not found")
        sys.exit(1)

    if not files:
        print("No .3ds files found")
        sys.exit(1)

    if args.check:
        needs_fix = 0
        already_ok = 0
        truly_encrypted = 0
        errors = 0

        for f in files:
            r = check_file(str(f))
            if r["error"]:
                print(f"  ERROR {f.name}: {r['error']}")
                errors += 1
            elif r["already_ok"]:
                print(f"  OK:       {f.name}")
                already_ok += 1
            elif r["needs_fix"]:
                print(f"  NEEDS FIX: {f.name}")
                needs_fix += 1
            elif r["truly_encrypted"]:
                print(f"  ENCRYPTED: {f.name} (truly encrypted, cannot fix with flag flip)")
                truly_encrypted += 1
            else:
                print(f"  UNKNOWN:  {f.name}")

        print(f"\nSummary: {len(files)} files")
        print(f"  Already OK (NoCrypto set):    {already_ok}")
        print(f"  Needs fix (flag missing):     {needs_fix}")
        print(f"  Truly encrypted (need keys):  {truly_encrypted}")
        print(f"  Errors:                       {errors}")
        return

    # Fix mode
    if not args.output:
        print("Error: --output directory required")
        sys.exit(1)

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    fixed = 0
    copied = 0
    failed = 0

    for i, f in enumerate(files, 1):
        print(f"[{i}/{len(files)}] {f.name}")
        r = check_file(str(f))

        if r["error"]:
            print(f"  ERROR: {r['error']}")
            failed += 1
            continue

        output_path = output_dir / f.name

        if r["already_ok"]:
            print("  Already OK, copying as-is")
            shutil.copy2(str(f), str(output_path))
            copied += 1
        elif r["needs_fix"]:
            print("  Fixing NoCrypto flag...")
            if fix_file(str(f), str(output_path)):
                print("  Done")
                fixed += 1
            else:
                print("  FAILED")
                failed += 1
        elif r["truly_encrypted"]:
            print("  Truly encrypted — skipping (needs full decryption)")
            failed += 1
        else:
            print("  Unknown state — copying as-is")
            shutil.copy2(str(f), str(output_path))
            copied += 1

    print(f"\nDone: {fixed} fixed, {copied} copied as-is, {failed} failed")


if __name__ == "__main__":
    main()
