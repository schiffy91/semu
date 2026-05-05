"""Tests for controller-profile application."""

import os

from core import controllers


def test_list_profiles_includes_known():
    profiles = controllers.list_profiles()
    # The bundled profile dirs should at least exist after this PR.
    for expected in ("xbox", "dualsense"):
        assert expected in profiles


def test_apply_unknown_profile_returns_false(tmp_path):
    assert controllers.apply(str(tmp_path), "Dolphin", "totally-fake") is False


def test_apply_with_no_bundled_file_returns_false(tmp_path, monkeypatch):
    """An emulator that has no fragment in the requested profile is a no-op."""
    # Force the profile root to a temp location with only an empty profile dir
    fake = tmp_path / "fake_profiles" / "xbox"
    fake.mkdir(parents=True)
    monkeypatch.setattr(controllers, "_profile_root", lambda: str(tmp_path / "fake_profiles"))
    assert controllers.apply(str(tmp_path), "Dolphin", "xbox") is False


def test_apply_copies_fragment_into_emulator_dir(tmp_path, monkeypatch):
    fake_profiles = tmp_path / "profiles"
    (fake_profiles / "xbox").mkdir(parents=True)
    (fake_profiles / "xbox" / "dolphin.ini").write_text("[Profile]\n")

    monkeypatch.setattr(controllers, "_profile_root", lambda: str(fake_profiles))

    project = tmp_path / "project"
    (project / "Dolphin").mkdir(parents=True)
    assert controllers.apply(str(project), "Dolphin", "xbox") is True

    target = project / "Dolphin" / "config" / "GCPadNew.ini"
    assert target.exists()
    assert target.read_text() == "[Profile]\n"
