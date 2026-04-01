# Phase 0: 3DS ROM Decryption

Batch-decrypt all 173 encrypted `.3ds` ROMs so they work in any 3DS emulator (Azahar, AzaharPlus, RetroArch, etc.) without runtime key dependencies.

## Background

The `.3ds` ROMs are encrypted NCSD/NCCH cartridge dumps. Azahar refuses to play encrypted ROMs (policy decision — the decryption code is `#ifdef`'d out). AzaharPlus re-enabled the decryption and plays them, but that locks us into one emulator. Decrypting once makes the ROMs universally compatible.

## How 3DS Encryption Works

A `.3ds` file (NCSD) contains 1-8 NCCH partitions. Each NCCH has three encrypted sections:

```
NCSD Container (.3ds file)
├── NCSD Header (0x0000-0x0FFF) — unencrypted
├── Partition 0 (game) — NCCH
│   ├── NCCH Header (0x100 bytes) — unencrypted
│   ├── ExHeader — AES-128-CTR encrypted
│   ├── ExeFS (code + assets) — AES-128-CTR encrypted
│   └── RomFS (game data) — AES-128-CTR encrypted
├── Partition 1 (manual) — NCCH (if present)
├── ...
└── Partition 7 — NCCH (if present)
```

### Key Derivation

Each NCCH section is encrypted with AES-128-CTR. The key is derived from hardware key slots:

1. **KeyX** — fixed per slot, from the 3DS hardware (published in AzaharPlus source code)
2. **KeyY** — first 16 bytes of the NCCH signature (from the ROM header itself, unique per game)
3. **Generator Constant** — `1ff9e9aac5fe0408024591dc5d52768a`
4. **NormalKey** = `LROT128(ADD128(XOR128(LROT128(KeyX, 2), KeyY), GeneratorConstant), 87)`

The CTR (counter/nonce) is derived from the partition ID and section type.

### Key Slot Mapping

| NCCH `secondary_key_slot` value | Slot ID | KeyX (from AzaharPlus source) |
|---|---|---|
| 0 (original, 0x2C "Secure1") | 0x2C | `b98e95ceca3e4d171f76a94de934c053` |
| 1 (7.x, 0x25 "Secure2") | 0x25 | `cee7d8ab30c00dae850ef5e382ac5af3` |
| 10 (9.6, 0x18 "Secure3") | 0x18 | `82e9c9bebfb8bdb875ecc0a07d474374` |
| 11 (9.6 alt, 0x1B "Secure4") | 0x1B | `45ad04953992c7c893724a9a7bce6182` |

The **primary key** always uses slot 0x2C. The **secondary key** varies by `secondary_key_slot` in the NCCH header — your ROMs show `flags[3]=0x01`, meaning they use 7.x crypto (slot 0x25) for the secondary key.

Primary key encrypts: ExHeader, ExeFS header, and the `.code` section within ExeFS.
Secondary key encrypts: all other ExeFS sections, and the entire RomFS.

### What "Decrypted" Means

A decrypted `.3ds` file is byte-identical to the encrypted one except:
- All AES-CTR encrypted sections are replaced with their plaintext
- The NCCH header's `no_crypto` flag (byte 7 of the NCCH flags at offset +0x188) is set to `0x04`
- Everything else (headers, partition table, sizes) stays the same

The file size does not change. The format does not change. It's still a valid `.3ds` file.

## Tool: `decrypt3ds.py`

A standalone Python script that batch-decrypts `.3ds` files. No external dependencies beyond the Python standard library (uses `cryptography` package for AES, or implements AES-CTR from PyCryptodome).

### Usage

```
# Decrypt a single ROM (test)
python decrypt3ds.py "ROMs/n3ds/BOXBOY! (USA) (En,Fr,Es) (eShop).3ds" --output test/

# Batch decrypt all ROMs
python decrypt3ds.py "ROMs/n3ds/" --output "ROMs/n3ds-decrypted/"

# Dry run — report encryption status without decrypting
python decrypt3ds.py "ROMs/n3ds/" --check
```

### What It Does

For each `.3ds` file:
1. Read the NCSD header, identify partitions
2. For each NCCH partition:
   a. Read NCCH header, check `no_crypto` flag
   b. If already decrypted, skip
   c. Extract KeyY from NCCH signature (first 16 bytes)
   d. Determine key slot from `secondary_key_slot` flag
   e. Derive primary and secondary NormalKeys using the scrambler
   f. Build CTR values from partition ID
   g. Decrypt ExHeader, ExeFS, and RomFS sections in-place using AES-128-CTR
   h. Set the `no_crypto` flag in the NCCH header
3. Write the decrypted file to the output directory

### Verification

After decrypting, verify by:
- Checking the `no_crypto` flag is set
- Confirming the ExHeader has a valid NCCH magic after decryption
- Opening one ROM in Azahar and confirming it loads

### Seed Crypto

Some games (mostly eShop titles) use "seed crypto" where the secondary KeyY is derived from:
`SHA256(primary_KeyY || game_seed)[0:16]`

Seeds are per-game and stored in a `seeddb.bin` file. If any ROMs use seed crypto, we'll need to source a seeddb. This is detectable from the NCCH flags before decryption.

## Order of Operations

### Step 1: Set up tooling
- Install `pycryptodome` (`pip install pycryptodome`)
- Write `decrypt3ds.py` with the key data, scrambler, and NCCH parser

### Step 2: Scan the ROM collection
- Run `decrypt3ds.py ROMs/n3ds/ --check`
- Report: how many encrypted, how many already decrypted, how many use seed crypto
- Identify if we need `seeddb.bin`

### Step 3: Test with one small ROM
- Pick a small ROM (eShop title, likely under 500MB)
- Copy it to a test directory
- Decrypt it
- Download Azahar for macOS (`azahar-macos-universal-2125.0.1.zip`)
- Open the decrypted ROM in Azahar — confirm it loads past the title screen

### Step 4: Handle seed crypto (if needed)
- If `--check` found seed-crypto ROMs, source `seeddb.bin`
- Azahar/AzaharPlus source may have a seed database, or one can be generated from a title list
- Add seed lookup to the decryptor

### Step 5: Batch decrypt
- Run on all 173 ROMs
- Output to a new directory (`ROMs/n3ds-decrypted/` or overwrite in-place with `--in-place`)
- Verify a sample of the output

### Step 6: Swap into ES-DE
- Point ES-DE's n3ds ROM path to the decrypted directory (or replace in-place)
- Update ES-DE systeminfo.txt launch command to use Azahar instead of citra_libretro
- Test end-to-end from ES-DE

## File Naming

The decrypted files keep their original filenames. The `.3ds` extension is correct for decrypted ROMs too — it's the same container format, just without encryption. No renaming needed.

## Risk: Seed Database

The main unknown is whether any of the 173 ROMs use seed crypto. Step 2 (`--check`) will tell us immediately. If they do, we need `seeddb.bin` — this is a known-format database file that the community maintains. The AzaharPlus source may contain one, or we can generate entries from the game's title IDs.

## Integration with Phase 2

Once Phase 0 is done:
- `decrypt3ds.py` can be integrated into `setup.py migrate Lime3DS Azahar` as a migration step
- The Azahar `emulator.json` manifest replaces the Lime3DS one
- ES-DE launch commands updated
- Lime3DS directory kept in `backups/old/` (already done)
