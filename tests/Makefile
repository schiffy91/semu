E2E_GRAPH := tests/e2e/graph.json
E2E_GRAPH_NODES ?=
E2E_GRAPH_ARGS ?=

.PHONY: payload-audit generated-build generated-smoke test ftux-test e2e-smoke e2e-graph-list e2e-graph-status e2e-graph-coverage e2e-graph-run lifecycle-smoke sandbox-smoke launcher-smoke appimage-smoke deck-ssh-smoke nix-e2e verify

generated-build: generated/semu.c ## Build committed generated C without invoking BTRC
	@mkdir -p build
	$(CC) generated/semu.c -std=c11 -o "$(SEMU_BIN)" -lm

generated-smoke: generated-build ## Run BTRC-native smoke tests from generated C
	$(SEMU_BIN) e2e all
	$(SEMU_BIN) e2e shell-syntax --project "$(CURDIR)"

payload-audit: $(SEMU_BIN) ## Fail if tracked licensed payloads or VM artifacts would be upstreamed
	$(SEMU_BIN) e2e payload-audit --project "$(CURDIR)"

test: payload-audit generated-smoke appimage-smoke ## Run native BTRC/runtime tests locally

ftux-test: generated-smoke ## Validate Steam Deck/Linux first-run bootstrap path

e2e-smoke: $(SEMU_BIN) ## Run BTRC-native sandbox and lifecycle smoke tests
	$(SEMU_BIN) e2e all

e2e-graph-list: $(SEMU_BIN) ## List declarative E2E graph nodes
	$(SEMU_BIN) e2e graph "$(E2E_GRAPH)" list

e2e-graph-status: $(SEMU_BIN) ## Show cached/stale E2E graph node state
	$(SEMU_BIN) e2e graph "$(E2E_GRAPH)" status $(E2E_GRAPH_ARGS)

e2e-graph-coverage: $(SEMU_BIN) ## Check declared E2E operation coverage
	$(SEMU_BIN) e2e graph "$(E2E_GRAPH)" coverage

e2e-graph-run: $(SEMU_BIN) ## Run E2E graph defaults or E2E_GRAPH_NODES="node ..."
	$(SEMU_BIN) e2e graph "$(E2E_GRAPH)" run $(E2E_GRAPH_NODES) $(E2E_GRAPH_ARGS)

lifecycle-smoke: $(SEMU_BIN) ## Validate install/reconfigure/change/uninstall/reinstall/upgrade lifecycle
	$(SEMU_BIN) e2e lifecycle

sandbox-smoke: $(SEMU_BIN) ## Validate all BTRC sandbox prepare routes
	$(SEMU_BIN) e2e sandbox

launcher-smoke: $(SEMU_BIN) ## Validate BTRC Linux launcher routing with fake flatpak/bwrap
	$(SEMU_BIN) e2e launcher

appimage-smoke: generated-build ## Validate AppImage assembly and Nix-store mount wiring with BTRC fakes
	$(SEMU_BIN) e2e appimage --project "$(CURDIR)"

deck-ssh-smoke: ## Probe a physical Steam Deck over SSH and capture a baseline screenshot
	tests/deck/ssh-smoke.sh

nix-e2e: ## Validate flake routed-emulator shape and mock wrapper behavior
	@set -euo pipefail; \
	host_system="$$(nix eval --impure --raw --expr builtins.currentSystem)"; \
	nix eval .#packages.x86_64-linux.semu-routed-emulators.name >/dev/null; \
	nix eval .#packages.x86_64-linux.default.name >/dev/null; \
	nix eval --raw .#apps.x86_64-linux.semu-retroarch.program | grep -F '/bin/semu-retroarch' >/dev/null; \
	nix eval --raw .#apps.x86_64-linux.semu-es-de.program | grep -F '/bin/semu-es-de' >/dev/null; \
	nix build ".#checks.$$host_system.routed-emulator-mock" --print-build-logs; \
	echo "OK Nix routed-emulator smoke ($$host_system)"

verify: payload-audit $(SEMU_BIN) ## Run deterministic BTRC/Steam Deck verification
	@set -euo pipefail; \
	verify_dir="$(CURDIR)/build/verification"; \
	rm -rf "$$verify_dir"; \
	mkdir -p "$$verify_dir"; \
	echo "== Manifest determinism =="; \
	"$(SEMU_BIN)" manifest --output "$$verify_dir/semu.json"; \
	cmp -s "$$verify_dir/semu.json" "semu.json"; \
	grep -F '"source_language": "semu-keymap-v1"' "$$verify_dir/semu.json" >/dev/null; \
		grep -F '"source_path": "$${paths.keymaps}/steam_deck.skm"' "$$verify_dir/semu.json" >/dev/null; \
		grep -F '"screenshot_verification"' "$$verify_dir/semu.json" >/dev/null; \
		grep -F '"before_launch"' "$$verify_dir/semu.json" >/dev/null; \
		grep -F '"after_spawn"' "$$verify_dir/semu.json" >/dev/null; \
		grep -F '"engine": "syncthing"' "$$verify_dir/semu.json" >/dev/null; \
		grep -F '"tray_app": "syncthingtray"' "$$verify_dir/semu.json" >/dev/null; \
	cmp -s "$(SEMU_C)" "generated/semu.c"; \
	echo "OK manifest generated from BTRC matches semu.json"; \
	echo "== Keymap compiler and renderers =="; \
	"$(SEMU_BIN)" keymap validate --project "$(CURDIR)" | tee "$$verify_dir/keymap-validate.txt"; \
	"$(SEMU_BIN)" keymap render --project "$(CURDIR)" --target manifest --output "$$verify_dir/keymap.json"; \
	"$(SEMU_BIN)" keymap render --project "$(CURDIR)" --target retroarch --output "$$verify_dir/retroarch.cfg"; \
	"$(SEMU_BIN)" keymap render --project "$(CURDIR)" --target dolphin --output "$$verify_dir/dolphin.ini"; \
	"$(SEMU_BIN)" keymap render --project "$(CURDIR)" --target pcsx2 --output "$$verify_dir/pcsx2.ini"; \
	"$(SEMU_BIN)" keymap render --project "$(CURDIR)" --target steam-input --output "$$verify_dir/steam-input.vdf"; \
	grep -F 'input_exit_emulator = "escape"' "$$verify_dir/retroarch.cfg" >/dev/null; \
	grep -F 'input_quit_gamepad_combo = "4"' "$$verify_dir/retroarch.cfg" >/dev/null; \
	grep -F 'input_load_state = "a"' "$$verify_dir/retroarch.cfg" >/dev/null; \
	grep -F 'input_save_state = "s"' "$$verify_dir/retroarch.cfg" >/dev/null; \
	grep -F 'General/Stop = @(Ctrl+Q)' "$$verify_dir/dolphin.ini" >/dev/null; \
	grep -F 'Save State/Save State Slot 1 = @(Ctrl+S)' "$$verify_dir/dolphin.ini" >/dev/null; \
	grep -F 'SaveStateToSlot = Keyboard/Control & Keyboard/S' "$$verify_dir/pcsx2.ini" >/dev/null; \
	grep -F 'LoadStateFromSlot = Keyboard/Control & Keyboard/A' "$$verify_dir/pcsx2.ini" >/dev/null; \
	grep -F 'key_press S, Save State' "$$verify_dir/steam-input.vdf" >/dev/null; \
	grep -F 'xinput_button SELECT, Quit' "$$verify_dir/steam-input.vdf" >/dev/null; \
	grep -F 'xinput_button START, Quit' "$$verify_dir/steam-input.vdf" >/dev/null; \
	grep -F 'key_press ESCAPE, Quit' "$$verify_dir/steam-input.vdf" >/dev/null; \
	grep -F 'key_press LEFT_ALT, Quit' "$$verify_dir/steam-input.vdf" >/dev/null; \
	grep -F 'key_press F4, Quit' "$$verify_dir/steam-input.vdf" >/dev/null; \
	echo "OK keymap render targets"; \
	echo "== Doctor invariants =="; \
	"$(SEMU_BIN)" doctor --project "$(CURDIR)" | tee "$$verify_dir/doctor.txt"; \
	grep -F 'OK gyro: disabled' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK right_trackpad: mouse' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK left_trackpad: radial_hotkeys' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK hotkeys: HKB+L1 load, HKB+R1 save, HKB+Start quit' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK layers: controller_model -> emulation_backend -> emitted_input -> emulator_keymap' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK backends: uinput, evemu, uhid, inputplumber, steam_input' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK controller_models: steam_deck, steam_controller, xbox_xinput, dualshock4, dualsense, switch_pro' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'Input capabilities' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK app.quit: desktop_verified=retroarch,dolphin,ppsspp,flycast,melonds,pcsx2,cemu,azahar,ryujinx; physical_game_mode=pending' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'pending state.save: generated=retroarch,dolphin,pcsx2; disabled=azahar,ppsspp,flycast,melonds,cemu,ryujinx; physical_state_proof=pending' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'pending state.load: generated=retroarch,dolphin,pcsx2; disabled=azahar,ppsspp,flycast,melonds,cemu,ryujinx; physical_state_proof=pending' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'Keymap compiler' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK steam_deck:' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK neptune_simple: controller_neptune, trackpads, save/load/quit' "$$verify_dir/doctor.txt" >/dev/null; \
		grep -F 'OK neptune_full: controller_neptune, trackpads, save/load/quit' "$$verify_dir/doctor.txt" >/dev/null; \
		grep -F 'ROM preflight' "$$verify_dir/doctor.txt" >/dev/null; \
		grep -F 'Screenshot verification hooks' "$$verify_dir/doctor.txt" >/dev/null; \
		grep -F 'OK hooks: before_launch, after_spawn, after_exit, manual_visual_checkpoint' "$$verify_dir/doctor.txt" >/dev/null; \
		grep -E 'OK screenshot_tool|MISSING screenshot_tool' "$$verify_dir/doctor.txt" >/dev/null; \
		grep -F 'Sync' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK start_at_boot: enabled' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'OK saves: watch, 900s' "$$verify_dir/doctor.txt" >/dev/null; \
	grep -F 'optional roms: watch, 3600s' "$$verify_dir/doctor.txt" >/dev/null; \
	echo "OK doctor invariants"; \
	echo "== Shell artifact syntax =="; \
	"$(SEMU_BIN)" e2e shell-syntax --project "$(CURDIR)"; \
	echo "== BTRC lifecycle/sandbox smoke =="; \
	"$(SEMU_BIN)" e2e all; \
	echo "OK BTRC lifecycle/sandbox smoke"; \
	echo "== AppImage/Nix routing smoke =="; \
	"$(SEMU_BIN)" e2e appimage --project "$(CURDIR)"; \
	$(MAKE) --no-print-directory nix-e2e; \
	echo "OK AppImage/Nix routing smoke"; \
	echo "== BTRC runtime source boundary =="; \
	test -f src/semu.btrc; \
	test -d src/semu; \
	echo "OK BTRC runtime source boundary"; \
	sandbox_dir="$$verify_dir/sandbox-retroarch"; \
	rm -rf "$$sandbox_dir"; \
	"$(SEMU_BIN)" sandbox prepare --project "$(CURDIR)" --emulator retroarch --scratch "$$sandbox_dir"; \
	test -L "$$sandbox_dir/.config/retroarch/retroarch.cfg"; \
	echo "OK runtime is BTRC-backed"; \
	echo "== Generated-C smoke =="; \
	$(MAKE) --no-print-directory generated-smoke; \
	echo "== Whitespace check =="; \
	git diff --check; \
	echo "Verification artifacts: $$verify_dir"

include tests/vm/Makefile
include tests/bazzite/Makefile
