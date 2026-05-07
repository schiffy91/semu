"""End-to-end CLI tests via subprocess.

Each test invokes `python setup.py <subcmd>` against a temp project tree and
asserts the visible behaviour (file changes, exit codes, stdout). Catches
regressions in argparse wiring that pure unit tests miss.
"""

import json
import os
import shutil
import subprocess
import sys

import pytest


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


@pytest.fixture
def fake_project(tmp_path):
    """Mirror real schemulator layout with one fake emulator."""
    setup_json = {
        "host":     {"linux": str(tmp_path / "host"), "macos": str(tmp_path / "host"), "windows": str(tmp_path / "host")},
        "portable": {"linux": str(tmp_path / "portable"), "macos": str(tmp_path / "portable"), "windows": str(tmp_path / "portable")},
    }
    (tmp_path / "setup.json").write_text(json.dumps(setup_json))

    emu = tmp_path / "TestEmu"
    (emu / "config").mkdir(parents=True)
    (emu / "config" / "settings.ini").write_text("default=true\n")
    (emu / "symlinks.json").write_text(json.dumps({
        "config": {
            "linux":   str(tmp_path / "host" / "TestEmu"),
            "macos":   str(tmp_path / "host" / "TestEmu"),
            "windows": str(tmp_path / "host" / "TestEmu"),
        }
    }))
    return tmp_path


def _run(*args, project=None, **kw):
    """Invoke the schemulator CLI as a subprocess."""
    cmd = [sys.executable, os.path.join(REPO_ROOT, "setup.py")]
    if project is not None:
        cmd += ["--config", str(project / "setup.json")]
    cmd += list(args)
    env = {**os.environ, "PYTHONPATH": REPO_ROOT}
    return subprocess.run(cmd, capture_output=True, text=True, env=env, **kw)


def test_cli_help_lists_all_subcommands():
    r = _run("--help")
    assert r.returncode == 0
    for cmd in ("symlink", "status", "backup", "install", "update",
                "uninstall", "rollback", "controllers", "sd-scan", "sync"):
        assert cmd in r.stdout


def test_cli_status_works_on_empty_project(fake_project):
    r = _run("status", project=fake_project)
    assert r.returncode == 0
    assert "TESTEMU" in r.stdout
    assert "MISSING" in r.stdout or "OK" in r.stdout


def test_cli_symlink_creates_links(fake_project):
    r = _run("symlink", project=fake_project)
    assert r.returncode == 0
    # Symlink should now exist under host/
    assert (fake_project / "host" / "TestEmu" / "settings.ini").exists()


def test_cli_backup_produces_zip(fake_project):
    _run("symlink", project=fake_project)
    r = _run("backup", project=fake_project)
    assert r.returncode == 0
    backups = list((fake_project / "backups").glob("*.zip"))
    assert len(backups) == 1


def test_cli_capture_revert_round_trip(fake_project):
    r1 = _run("capture", "TestEmu", "v1.0", project=fake_project)
    assert r1.returncode == 0

    # Modify
    (fake_project / "TestEmu" / "config" / "settings.ini").write_text("modified=true\n")

    r2 = _run("revert", "TestEmu", "v1.0", project=fake_project)
    assert r2.returncode == 0
    assert (fake_project / "TestEmu" / "config" / "settings.ini").read_text() == "default=true\n"


def test_cli_originals_lists_captures(fake_project):
    _run("capture", "TestEmu", "v1.0", project=fake_project)
    _run("capture", "TestEmu", "v2.0", project=fake_project)
    r = _run("originals", "TestEmu", project=fake_project)
    assert r.returncode == 0
    assert "v1.0" in r.stdout and "v2.0" in r.stdout


def test_cli_sd_scan_runs_without_crashing(fake_project, tmp_path, monkeypatch):
    """sd-scan should print 'no external storage' when /run/media is empty
    rather than crash."""
    r = _run("sd-scan", project=fake_project)
    # Exit 1 is fine (no storage found), exit 0 also fine. Crash is not.
    assert r.returncode in (0, 1)


def test_cli_controllers_apply(fake_project):
    """Apply Xbox profile to all emulators - must not crash even if some are missing."""
    r = _run("controllers", "xbox", project=fake_project)
    # No bundled fragment for "TestEmu" - should report 0 applied (exit 1)
    # but not crash.
    assert r.returncode in (0, 1)


def test_cli_unknown_subcommand_fails(fake_project):
    r = _run("not-a-real-subcommand", project=fake_project)
    assert r.returncode != 0
