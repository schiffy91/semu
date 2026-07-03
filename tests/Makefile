.PHONY: tree-audit test verify manifest-smoke core-tests target-tests

APPROVED_TOP_LEVEL := .git .github .claude .gitignore Makefile flake.lock flake.nix src tests

tree-audit: ## Check that source/test files match the approved refactor shape
	@set -euo pipefail; \
	for path in $$(find . -maxdepth 1 -mindepth 1 -printf '%f\n' 2>/dev/null || find . -maxdepth 1 -mindepth 1 -exec basename {} \;); do \
		case " $(APPROVED_TOP_LEVEL) " in *" $$path "*) ;; *) echo "unexpected top-level path: $$path" >&2; exit 1 ;; esac; \
	done; \
	if find src/semu -type f | grep -Ev '^src/semu/(cli\.btrc|settings\.json|core/[a-z0-9_]+\.btrc|assets/(sources\.json|bezels\.nix|shaders\.nix|reshade/[^/]+|overlay/[^/]+|fonts/[^/]+)|platforms/(semu_platform\.btrc|platform_registry\.btrc|linux/([a-z0-9_]+\.btrc|quit_watch\.c)|macos/[a-z0-9_]+\.btrc)|emulators/(semu_emulator\.btrc|emulators\.nix|es_de/es_de\.nix|emulator_registry\.btrc|emulator_parser\.btrc|emulator_emitter\.btrc|emulator_process\.btrc|emulator_execution\.btrc|rendering/(semu_bezel\.btrc|semu_shader\.btrc|semu_rendering\.btrc|render_resolution\.btrc|tap/[a-z0-9_.]+)|retroarch/(retroarch_emulator\.btrc|retroarch_emitter\.btrc|retroarch_rendering\.btrc|retroarch\.patch|retroarch_darwin\.patch|retroarch\.nix|emulator\.json)|dolphin/(dolphin_emulator\.btrc|dolphin_emitter\.btrc|dolphin_wiimote_profiles\.btrc|dolphin_rendering\.btrc|dolphin\.nix|emulator\.json)|pcsx2/(pcsx2_emulator\.btrc|pcsx2_emitter\.btrc|pcsx2_rendering\.btrc|pcsx2\.nix|emulator\.json)|cemu/(cemu_emulator\.btrc|cemu_emitter\.btrc|cemu_rendering\.btrc|cemu\.nix|emulator\.json)|ryujinx/(ryujinx_emulator\.btrc|ryujinx_emitter\.btrc|ryujinx_rendering\.btrc|ryujinx\.nix|deps\.json|emulator\.json)|ppsspp/(ppsspp_emulator\.btrc|ppsspp_emitter\.btrc|ppsspp_rendering\.btrc|emulator\.json)|flycast/(flycast_emulator\.btrc|flycast_emitter\.btrc|flycast_rendering\.btrc|emulator\.json)|azahar/(azahar_emulator\.btrc|azahar_emitter\.btrc|azahar_rendering\.btrc|azahar\.nix|emulator\.json)|melonds/(melonds_emulator\.btrc|melonds_emitter\.btrc|melonds_rendering\.btrc|emulator\.json)|ares/(emulator\.json|ares\.nix))|systems/(system_parser\.btrc|system_emitter\.btrc|[^/]+/(system\.json|bezels\.json|shaders\.json))|input/(input_parser\.btrc|input_emitter\.btrc|controllers/steamdeck/(steamdeck\.skm|steam_input\.json|steam_input\.btrc|steamdeck_input_emitter\.btrc|steamdeck_hotkey_assignments\.btrc))|packaging/(appimage_package\.btrc|es_de_runtime\.btrc|nix_package\.btrc|installer\.sh|appimage/(AppRun\.template|semu\.desktop\.template)|nix/(semu_app\.nix|semu_cli\.nix|module\.nix|flake/(apps\.nix|checks\.nix|dev-shells\.nix|packages\.nix))))$$' >/tmp/semu-tree-audit.txt; then \
		echo "unexpected src/semu files outside approved tree:" >&2; \
		cat /tmp/semu-tree-audit.txt >&2; \
		exit 1; \
	fi; \
	if find tests -type f | grep -Ev '^tests/(Makefile|core/(emulator_contract_test\.btrc|system_contract_test\.btrc|rendering_contract_test\.btrc|input_contract_test\.btrc|package_contract_test\.btrc|generated_tree_test\.btrc)|targets/steamdeck/(Makefile|input_test\.btrc|visual_evidence_test\.btrc|emulator_loop_test\.btrc)|targets/vm/(Makefile|image_test\.btrc|install_test\.btrc|visual_evidence_test\.btrc|grub\.cfg|kickstart\.ks\.in|semu_install))$$' >/tmp/semu-test-tree-audit.txt; then \
		echo "unexpected tests files outside approved tree:" >&2; \
		cat /tmp/semu-test-tree-audit.txt >&2; \
		exit 1; \
	fi; \
	echo "OK approved tree"

manifest-smoke: $(SEMU_BIN)
	@mkdir -p src/generated/test
	$(SEMU_BIN) manifest --output src/generated/test/semu.json
	@test -f src/generated/test/semu.json

core-tests: $(SEMU_BIN)
	$(SEMU_BIN) test core

# Target Makefiles assume repo-root cwd (their asset paths are root-relative).
target-tests: $(SEMU_BIN)
	$(MAKE) -f tests/targets/steamdeck/Makefile test
	$(MAKE) -f tests/targets/vm/Makefile test

test: tree-audit $(SEMU_BIN) manifest-smoke core-tests

verify: test
