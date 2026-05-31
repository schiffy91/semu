import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from conftest import build_test_3ds


REPO_ROOT = Path(__file__).resolve().parents[1]


def _load_project_manifest(project):
    return json.loads((project / "schemulator.json").read_text())


def _btrc_binary_or_skip():
    binary = REPO_ROOT / "build" / "schemulator"
    if not binary.is_file():
        pytest.skip("build/schemulator is required for BTRC CLI coverage")
    try:
        probe = subprocess.run(
            [str(binary), "keymap", "validate"],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as exc:
        pytest.skip(f"build/schemulator cannot run on this platform: {exc}")
    if probe.returncode != 0:
        pytest.fail(probe.stderr or probe.stdout)
    return binary, probe


def _run_btrc(args, **kwargs):
    binary, _ = _btrc_binary_or_skip()
    return subprocess.run(
        [str(binary), *args],
        capture_output=True,
        text=True,
        check=False,
        **kwargs,
    )


def test_manifest_declares_steam_deck_input_stack_and_defaults():
    manifest = _load_project_manifest(REPO_ROOT)
    input_stack = manifest["input"]
    assert input_stack["layers"] == [
        "controller_model",
        "emulation_backend",
        "emitted_input",
        "emulator_keymap",
    ]
    models = {item["id"]: item for item in input_stack["controller_models"]}
    assert {
        "steam_deck",
        "steam_controller",
        "xbox_xinput",
        "dualshock4",
        "dualsense",
        "switch_pro",
    } <= set(models)
    assert models["steam_deck"]["preferred_backend"] == "inputplumber"
    assert models["steam_deck"]["gyro_policy"] == "disabled_by_default"
    assert {"right_trackpad", "left_trackpad", "keyboard_hotkeys"} <= set(models["steam_deck"]["capabilities"])
    assert models["xbox_xinput"]["preferred_backend"] == "uinput"
    assert models["dualsense"]["layout"] == "playstation"
    assert "gyro" in models["switch_pro"]["capabilities"]

    backends = {item["id"]: item for item in input_stack["emulation_backends"]}
    assert {"uinput", "evemu", "uhid", "inputplumber", "steam_input"} <= set(backends)
    assert backends["uinput"]["automated"] is True
    assert backends["steam_input"]["requires_visual"] is True

    verification = {item["id"]: item for item in input_stack["verification_profiles"]}
    assert verification["linux_virtual_input"]["automated"] is True
    assert verification["deck_route"]["backend"] == "inputplumber"
    assert verification["steam_deck_game_mode"]["requires_visual"] is True

    steam_deck_profile = manifest["controller_profiles"]["steam_deck"]
    assert steam_deck_profile["controller_model"] == "steam_deck"
    assert steam_deck_profile["verification_profiles"] == [
        "linux_virtual_input",
        "deck_route",
        "steam_deck_game_mode",
    ]
    assert steam_deck_profile["gyro_enabled"] is False
    assert steam_deck_profile["trackpads"] == {
        "left": "radial_hotkeys",
        "right": "mouse",
    }
    assert not any("Gyro" in profile for profile in steam_deck_profile["profiles"])

    hotkeys = {item["id"]: item for item in steam_deck_profile["hotkeys"]}
    assert hotkeys["load_state"]["combo"] == "HKB + L1"
    assert hotkeys["load_state"]["command"] == "Ctrl+A"
    assert hotkeys["save_state"]["combo"] == "HKB + R1"
    assert hotkeys["save_state"]["command"] == "Ctrl+S"
    assert hotkeys["quit"]["combo"] == "HKB + Start"
    assert hotkeys["quit"]["command"] == "Ctrl+Q"
    assert "RetroArch/retroarch.cfg.linux-backup" in steam_deck_profile["profiles"]

    keymap = manifest["keymaps"]["steam_deck"]
    assert keymap["source_language"] == "schemulator-keymap-v1"
    assert keymap["source_path"] == "${paths.keymaps}/steam_deck.skm"
    assert set(keymap["render_targets"]) == {
        "manifest",
        "retroarch",
        "dolphin",
        "pcsx2",
        "steam-input",
    }
    actions = {item["id"]: item for item in keymap["actions"]}
    assert actions["ui.open"]["command"] == "Ctrl+O"
    assert actions["state.load"]["command"] == "Ctrl+A"
    assert actions["state.save"]["command"] == "Ctrl+S"
    assert actions["app.quit"]["command"] == "Ctrl+Q"
    bindings = {item["action"]: item for item in keymap["bindings"]}
    assert bindings["state.load"]["combo"] == "HKB + L1"
    assert bindings["state.save"]["combo"] == "HKB + R1"
    assert bindings["app.quit"]["combo"] == "HKB + Start"

    sync = manifest["sync"]
    assert sync["engine"] == "syncthing"
    assert sync["tray_app"] == "syncthingtray"
    assert sync["gui_address"] == "127.0.0.1:8384"
    sync_folders = {item["id"]: item for item in sync["folders"]}
    assert {"saves", "states", "emulator_state", "screenshots", "gamelists", "roms", "bios"} <= set(sync_folders)
    assert sync_folders["saves"]["enabled"] is True
    assert sync_folders["saves"]["watch"] is True
    assert sync_folders["saves"]["rescan_interval_s"] == 900
    assert sync_folders["emulator_state"]["enabled"] is True
    assert sync_folders["emulator_state"]["watch"] is True
    assert sync_folders["emulator_state"]["rescan_interval_s"] == 900
    assert sync_folders["roms"]["enabled"] is False
    assert sync_folders["bios"]["enabled"] is False

    screenshots = manifest["screenshot_verification"]
    hooks = {item["id"]: item for item in screenshots["hooks"]}
    assert {"before_launch", "after_spawn", "after_exit", "manual_visual_checkpoint"} <= set(hooks)
    assert screenshots["enable_env"] == "SCHEMULATOR_SCREENSHOT_HOOKS=1"


def test_steam_deck_controller_defaults_are_no_gyro_and_retrodeck_style():
    hotkeys = (
        REPO_ROOT
        / "Dolphin"
        / "config"
        / "Profiles"
        / "Hotkeys"
        / "Steam Deck.ini"
    ).read_text()
    assert "General/Stop = @(Ctrl+Q)" in hotkeys
    assert "Load State/Load State Slot 1 = @(Ctrl+A)" in hotkeys
    assert "Save State/Save State Slot 1 = @(Ctrl+S)" in hotkeys

    retroarch_profile = (REPO_ROOT / "RetroArch" / "retroarch.cfg.linux-backup").read_text()
    assert 'input_enable_hotkey = "ctrl"' in retroarch_profile
    assert 'input_load_state = "a"' in retroarch_profile
    assert 'input_save_state = "s"' in retroarch_profile
    assert 'input_exit_emulator = "q"' in retroarch_profile

    for wiimote in [
        "Wiimote (SD).ini",
        "Wiimote + Classic Controller (SD).ini",
    ]:
        profile = (
            REPO_ROOT
            / "Dolphin"
            / "config"
            / "Profiles"
            / "Wiimote"
            / wiimote
        ).read_text()
        assert "XInput2/0/Virtual core pointer:Cursor X" in profile
        assert "IMUIR/Enabled = False" in profile
        assert "Gyro" not in profile
        assert "Accel" not in profile

    cemu_profile = (
        REPO_ROOT
        / "Cemu"
        / "config"
        / "controllerProfiles"
        / "SteamInput-P1.xml"
    ).read_text()
    assert "<api>SDLController</api>" in cemu_profile
    assert "DSUController" not in cemu_profile

    pcsx2_profile = (
        REPO_ROOT
        / "PCSX2"
        / "config"
        / "inputprofiles"
        / "Steam Deck.ini"
    ).read_text()
    assert "UseProfileHotkeyBindings = true" in pcsx2_profile
    assert "SaveStateToSlot = Keyboard/Control & Keyboard/S" in pcsx2_profile
    assert "LoadStateFromSlot = Keyboard/Control & Keyboard/A" in pcsx2_profile

    ryujinx_profile = json.loads(
        (
            REPO_ROOT
            / "Ryujinx"
            / "config"
            / "profiles"
            / "controller"
            / "Steam Virtual Controller.json"
        ).read_text()
    )
    assert ryujinx_profile["motion"]["enable_motion"] is False
    assert ryujinx_profile["backend"] == "GamepadSDL2"


def test_linux_launcher_wrappers_are_executable_and_bash_syntax_valid():
    bash = shutil.which("bash")
    if not bash:
        pytest.skip("bash is required to syntax-check Linux launcher wrappers")

    launchers = list((REPO_ROOT / "linux" / "bin").glob("schem-*"))
    launchers.extend([
        REPO_ROOT / "linux" / "AppRun",
        REPO_ROOT / "linux" / "build-appimage.sh",
        REPO_ROOT / "linux" / "sandbox.sh",
    ])
    for launcher in sorted(launchers):
        assert launcher.is_file()
        assert os.access(launcher, os.X_OK)
        result = subprocess.run([bash, "-n", str(launcher)], capture_output=True, text=True, check=False)
        assert result.returncode == 0, result.stderr


def test_btrc_bootstrap_seeds_editable_steam_deck_keymap(tmp_path):
    binary, _ = _btrc_binary_or_skip()

    project = tmp_path / "project"
    project.mkdir()
    custom_source = (REPO_ROOT / "keymaps" / "steam_deck.skm").read_text().replace(
        "action state.save = Ctrl+S",
        "action state.save = Ctrl+V",
    )
    (project / "keymaps").mkdir()
    (project / "keymaps" / "steam_deck.skm").write_text(custom_source)

    result = subprocess.run(
        [str(binary), "bootstrap", "--project", str(project)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr

    keymap_source = project / "keymaps" / "steam_deck.skm"
    assert keymap_source.is_file()
    source = keymap_source.read_text()
    assert "action state.save = Ctrl+V" in source
    assert "bind HKB + R1 -> ${state.save}" in source
    assert (project / "schemulator.json").is_file()
    assert (project / "ES-DE" / "es_settings.xml").is_file()
    for path in [
        project / "ES-DE" / "ES-DE" / "ROMs" / "gb",
        project / "ES-DE" / "ES-DE" / "ROMs" / "switch",
        project / "ES-DE" / "ES-DE" / "bios" / "ps2",
        project / "ES-DE" / "ES-DE" / "bios" / "switch",
        project / "ES-DE" / "ES-DE" / "bios" / "dc",
        project / "ES-DE" / "ES-DE" / "saves",
        project / "ES-DE" / "ES-DE" / "states",
        project / "ES-DE" / "ES-DE" / "screenshots",
        project / "ES-DE" / "ES-DE" / "downloaded_media",
        project / "ES-DE" / "ES-DE" / "gamelists",
        project / "ES-DE" / "ES-DE" / "themes",
        project / "Cemu" / "data",
        project / "steam-input",
        project / "sync",
        project / "sync" / "bin",
        project / "sync" / "logs",
        project / "sync" / "syncthing" / "config",
        project / "sync" / "syncthing" / "data",
    ]:
        assert path.is_dir(), path

    sync_config = json.loads((project / "sync" / "sync.json").read_text())
    assert sync_config["enabled"] is True
    assert sync_config["start_at_boot"] is True
    assert sync_config["tray"] is True
    assert sync_config["roms_dir"] == "${paths.project_roms}"
    assert sync_config["sync_saves"] is True
    assert sync_config["sync_states"] is True
    assert sync_config["sync_emulator_state"] is True
    assert sync_config["sync_roms"] is False
    assert sync_config["rescan_saves_s"] == 900
    assert sync_config["rescan_emulator_state_s"] == 900
    assert sync_config["watch_saves"] is True
    assert sync_config["watch_emulator_state"] is True

    simple_template = project / "steam-input" / "neptune-simple.vdf"
    full_template = project / "steam-input" / "neptune-full.vdf"
    for template in [simple_template, full_template]:
        text = template.read_text()
        assert '"controller_mappings"' in text
        assert '"controller_type"\t\t"controller_neptune"' in text
        assert "left_trackpad active" in text
        assert "right_trackpad active" in text
        assert "key_press LEFT_CONTROL, Save State" in text
        assert "key_press A, Load State" in text
        assert "key_press Q, Quit" in text

    for relative in [
        "RetroArch/retroarch.cfg.linux-backup",
        "Dolphin/config/Profiles/GCPad/Steam Deck.ini",
        "Dolphin/config/Profiles/Hotkeys/Steam Deck.ini",
        "Dolphin/config/Profiles/Wiimote/Wiimote (SD).ini",
        "Dolphin/config/Profiles/Wiimote/Wiimote + Classic Controller (SD).ini",
        "Cemu/config/controllerProfiles/SteamInput-P1.xml",
        "PCSX2/config/inputprofiles/Steam Deck.ini",
        "Ryujinx/config/profiles/controller/Steam Virtual Controller.json",
        "linux/ES-DE/es_systems_linux.xml",
        "linux/ES-DE/es_find_rules_linux.xml",
        "linux/bin/schem-retroarch",
        "linux/bin/schem-dolphin",
        "linux/bin/schem-pcsx2",
        "linux/bin/schem-ryujinx",
    ]:
        assert (project / relative).is_file(), relative
    assert os.access(project / "linux" / "bin" / "schem-retroarch", os.X_OK)
    assert 'input_save_state = "v"' in (
        project / "RetroArch" / "retroarch.cfg.linux-backup"
    ).read_text()
    assert "Save State/Save State Slot 1 = @(Ctrl+V)" in (
        project / "Dolphin" / "config" / "Profiles" / "Hotkeys" / "Steam Deck.ini"
    ).read_text()
    assert "SaveStateToSlot = Keyboard/Control & Keyboard/V" in (
        project / "PCSX2" / "config" / "inputprofiles" / "Steam Deck.ini"
    ).read_text()

    validate = _run_btrc(["keymap", "validate", "--project", str(project)])
    assert validate.returncode == 0, validate.stderr
    retroarch = _run_btrc(["keymap", "render", "--project", str(project), "--target", "retroarch"])
    assert retroarch.returncode == 0, retroarch.stderr
    assert 'input_save_state = "v"' in retroarch.stdout
    steam_input = _run_btrc(["keymap", "render", "--project", str(project), "--target", "steam-input"])
    assert steam_input.returncode == 0, steam_input.stderr
    assert "key_press V, Save State" in steam_input.stdout


def test_btrc_doctor_preflights_steam_input_and_n3ds_roms(tmp_path):
    binary, _ = _btrc_binary_or_skip()

    project = tmp_path / "project"
    project.mkdir()
    bootstrap = subprocess.run(
        [str(binary), "bootstrap", "--project", str(project)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert bootstrap.returncode == 0, bootstrap.stderr

    rom_dir = project / "ES-DE" / "ES-DE" / "ROMs" / "n3ds"
    build_test_3ds(str(rom_dir / "ok.3ds"), no_crypto=True)
    build_test_3ds(str(rom_dir / "needs-fix.3ds"), no_crypto=False)
    encrypted = rom_dir / "encrypted.3ds"
    build_test_3ds(str(encrypted), no_crypto=False)
    with encrypted.open("r+b") as handle:
        handle.seek(0x4800)
        handle.write(b"\0" * 8)
    (rom_dir / "bad.3ds").write_bytes(b"not an ncsd image")

    doctor = subprocess.run(
        [str(binary), "doctor", "--project", str(project)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert doctor.returncode == 0, doctor.stderr
    assert "OK neptune_simple: controller_neptune, trackpads, save/load/quit" in doctor.stdout
    assert "OK neptune_full: controller_neptune, trackpads, save/load/quit" in doctor.stdout
    assert "OK n3ds/ok.3ds:" in doctor.stdout
    assert "NEEDS_FIX n3ds/needs-fix.3ds:" in doctor.stdout
    assert "ENCRYPTED n3ds/encrypted.3ds:" in doctor.stdout
    assert "INVALID n3ds/bad.3ds: missing NCSD header" in doctor.stdout


def test_btrc_sync_setup_writes_declarative_units_and_rom_override(tmp_path):
    binary, _ = _btrc_binary_or_skip()

    project = tmp_path / "project"
    home = tmp_path / "home"
    roms = tmp_path / "sdcard" / "ROMs"
    project.mkdir()
    home.mkdir()
    env = os.environ.copy()
    env["HOME"] = str(home)
    env["PATH"] = "/usr/bin:/bin"
    env["SCHEMULATOR_BIN"] = str(binary)

    install = subprocess.run(
        [
            str(binary),
            "deck",
            "install",
            "--project",
            str(project),
            "--roms",
            str(roms),
        ],
        capture_output=True,
        text=True,
        check=False,
        env=env,
    )
    assert install.returncode == 0, install.stderr

    sync_config = json.loads((project / "sync" / "sync.json").read_text())
    assert sync_config["roms_dir"] == str(roms)
    assert (roms / "gba").is_dir()
    assert (roms / "switch").is_dir()

    service_dir = home / ".config" / "systemd" / "user"
    service = (service_dir / "schemulator-syncthing.service").read_text()
    assert "ExecStart=syncthing serve -H" in service
    assert str(project / "sync" / "syncthing") in service
    force_service = (service_dir / "schemulator-sync-force.service").read_text()
    assert str(project / "sync" / "bin" / "sync-force.sh") in force_service
    timer = (service_dir / "schemulator-sync-force.timer").read_text()
    assert "OnUnitActiveSec=15min" in timer

    force_script = project / "sync" / "bin" / "sync-force.sh"
    assert os.access(force_script, os.X_OK)
    assert "sync force all" in force_script.read_text()

    desktop = (home / ".local" / "share" / "applications" / "schemulator.desktop").read_text()
    assert "Actions=ForceSync;SyncStatus;OpenSyncthing;OpenSyncTray;" in desktop
    assert "sync force all" in desktop

    env_result = subprocess.run(
        [str(binary), "config", "env", "--project", str(project)],
        capture_output=True,
        text=True,
        check=False,
        env=env,
    )
    assert env_result.returncode == 0
    assert "SCHEMULATOR_ROMS_DIR=" in env_result.stdout
    assert str(roms) in env_result.stdout


def test_btrc_keymap_compiler_validates_and_renders_emulator_targets(tmp_path):
    binary, validate = _btrc_binary_or_skip()
    assert validate.stdout.strip() == "OK keymap steam_deck"

    retroarch = subprocess.run(
        [str(binary), "keymap", "render", "--target", "retroarch"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert retroarch.returncode == 0, retroarch.stderr
    assert 'input_enable_hotkey = "ctrl"' in retroarch.stdout
    assert 'input_load_state = "a"' in retroarch.stdout
    assert 'input_save_state = "s"' in retroarch.stdout
    assert 'input_exit_emulator = "q"' in retroarch.stdout

    dolphin = subprocess.run(
        [str(binary), "keymap", "render", "--target", "dolphin"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert dolphin.returncode == 0, dolphin.stderr
    assert "General/Stop = @(Ctrl+Q)" in dolphin.stdout
    assert "Load State/Load State Slot 1 = @(Ctrl+A)" in dolphin.stdout
    assert "Save State/Save State Slot 1 = @(Ctrl+S)" in dolphin.stdout

    pcsx2 = subprocess.run(
        [str(binary), "keymap", "render", "--target", "pcsx2"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert pcsx2.returncode == 0, pcsx2.stderr
    assert "SaveStateToSlot = Keyboard/Control & Keyboard/S" in pcsx2.stdout
    assert "LoadStateFromSlot = Keyboard/Control & Keyboard/A" in pcsx2.stdout
    assert "OpenPauseMenu = Keyboard/Control & Keyboard/M" in pcsx2.stdout

    invalid = tmp_path / "invalid.skm"
    invalid.write_text("action state.save = Ctrl+S\nbind HKB + R1 -> ${missing.action}\n")
    bad = subprocess.run(
        [str(binary), "keymap", "validate", "--source", str(invalid)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert bad.returncode == 1
    assert "binding 'HKB + R1' references unknown action 'missing.action'" in bad.stdout
    assert "missing required action 'app.quit'" in bad.stdout

    rendered = tmp_path / "retroarch.cfg"
    write_render = subprocess.run(
        [
            str(binary),
            "keymap",
            "render",
            "--target",
            "retroarch",
            "--output",
            str(rendered),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert write_render.returncode == 0, write_render.stderr
    assert 'input_save_state = "s"' in rendered.read_text()

    bad_target = subprocess.run(
        [str(binary), "keymap", "render", "--target", "nope"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert bad_target.returncode == 1
    assert "unknown keymap target 'nope'" in bad_target.stdout

    missing_source = subprocess.run(
        [str(binary), "keymap", "validate", "--source", str(tmp_path / "missing.skm")],
        capture_output=True,
        text=True,
        check=False,
    )
    assert missing_source.returncode == 1
    assert "keymap source not found" in missing_source.stdout


def test_btrc_keymap_compiler_reports_common_authoring_errors(tmp_path):
    _btrc_binary_or_skip()
    source = (REPO_ROOT / "keymaps" / "steam_deck.skm").read_text()
    cases = [
        (
            "duplicate_action",
            source + "\naction state.save = Ctrl+S\n",
            "duplicate action 'state.save'",
        ),
        (
            "duplicate_combo",
            source + "\nbind HKB + R1 -> ${state.load}\n",
            "duplicate controller combo 'HKB + R1'",
        ),
        (
            "unsupported_modifier",
            source.replace("action state.save = Ctrl+S", "action state.save = Hyper+S"),
            "unsupported modifier 'Hyper'",
        ),
        (
            "trailing_modifier",
            source.replace("action state.save = Ctrl+S", "action state.save = Ctrl+"),
            "expected key after '+'",
        ),
        (
            "unterminated_ref",
            source.replace("bind HKB + R1 -> ${state.save}", "bind HKB + R1 -> ${state.save"),
            "unterminated action reference",
        ),
        (
            "missing_equals",
            source.replace("action state.save = Ctrl+S", "action state.save Ctrl+S"),
            "expected '=' after action id",
        ),
        (
            "missing_arrow",
            source.replace("bind HKB + R1 -> ${state.save}", "bind HKB + R1 ${state.save}"),
            "expected '->' in binding",
        ),
    ]

    for name, contents, expected in cases:
        keymap = tmp_path / f"{name}.skm"
        keymap.write_text(contents)
        result = _run_btrc(["keymap", "validate", "--source", str(keymap)])
        assert result.returncode == 1, name
        assert expected in result.stdout, result.stdout
