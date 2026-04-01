# Phase 1: Test Environment

GitHub Actions CI running smoke tests on Linux, Windows, and macOS on every push.

## Step 1: Initialize git repo with .gitignore

```gitignore
# ROMs and media (copyrighted, large)
ES-DE/ES-DE/ROMs/
ES-DE/ES-DE/downloaded_media/
**/n3ds-fixed/

# Emulator binaries and data (large, per-machine)
versions.json
**/bios/
**/memcards/
**/sstates/
**/StateSaves/
**/ScreenShots/
**/screenshots/

# Virtual environment
.venv/

# OS junk
.DS_Store
*.sync-conflict-*
.stfolder/

# Backups (large zips)
backups/*.zip
backups/old/
```

## Step 2: Write smoke tests

```
test/
├── smoke_test.py      # Cross-platform filesystem assertions
├── conftest.py        # Shared fixtures (temp dirs, mock config)
└── test_decrypt.py    # Tests for decrypt3ds.py flag-fix logic
```

Tests verify filesystem operations without needing emulator binaries or ROMs:

| Test | What it asserts |
|---|---|
| `test_symlink_creation` | Symlinks created, point to correct targets per platform |
| `test_config_parsing` | `config.json` + `emulator.json` load correctly, platform paths resolve |
| `test_platform_detection` | Correct platform detected on each CI runner |
| `test_decrypt_check` | `--check` correctly identifies NoCrypto flag status on a tiny test ROM |
| `test_decrypt_fix` | Flag flip produces correct output, original unchanged |
| `test_backup` | (Phase 2) Zip created with expected contents |
| `test_install` | (Phase 2) Correct binary downloaded for current platform |
| `test_status` | (Phase 2) Version comparison works |
| `test_revert` | (Phase 2) Originals restored, current config backed up first |

The decrypt tests use a minimal synthetic `.3ds` file (a few KB) committed to the repo — not a real ROM.

## Step 3: GitHub Actions workflow

```yaml
# .github/workflows/test.yml
name: Smoke Tests
on: [push, pull_request]

jobs:
  test:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.12"
      - run: pip install pycryptodome pytest
      - run: python -m pytest test/ -v
```

## Step 4: Push to GitHub, verify CI green on all 3 platforms

## Step 5: Expand tests as Phase 2 features land

Each new Phase 2 command gets a corresponding smoke test before it's considered done. The CI workflow doesn't change — just add test files.

## Implementation Order

1. `git init` + write `.gitignore`
2. Write `test/conftest.py` with fixtures (temp dirs, mock `config.json`)
3. Write `test/smoke_test.py` testing current `setup.py symlink` logic
4. Write `test/test_decrypt.py` with synthetic test ROM
5. Write `.github/workflows/test.yml`
6. Create GitHub repo, push, verify CI passes on all 3 platforms
7. Expand smoke tests as Phase 2 features land
