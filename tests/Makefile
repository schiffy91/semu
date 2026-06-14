TEST_PROJECT ?= $(CURDIR)
TEST_ENV := SEMU_ASSET_ROOT="$(CURDIR)" SEMU_PROJECT_DIR="$(TEST_PROJECT)"

.PHONY: payload-audit compiler-build compiler-verify source-hook-metadata rendering-contract esde-settings-entry cemu-ryujinx-input retroarch-n64-config test ftux-test deck-ssh-smoke

payload-audit: ## Fail if tests accidentally track VM images, ROMs, BIOS, or AppImages
	@set -euo pipefail; \
	if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then \
		git check-ignore -q tests/vms/ || { echo "tests/vms must stay ignored"; exit 1; }; \
		payloads="$$(git ls-files tests utils | grep -Ei '(^|/)(ROMs|BIOS|bios|saves|states|screenshots)(/|$$)|\.(3ds|7z|appimage|bin|cia|chd|dmg|gb|gba|gbc|iso|n64|nds|nsp|qcow2|rar|rom|sfc|smc|vdi|vmdk|xci|z64|zip|keys)$$' || true)"; \
	else \
		payloads="$$(find tests utils -type f 2>/dev/null | grep -Ei '(^|/)(ROMs|BIOS|bios|saves|states|screenshots)(/|$$)|\.(3ds|7z|appimage|bin|cia|chd|dmg|gb|gba|gbc|iso|n64|nds|nsp|qcow2|rar|rom|sfc|smc|vdi|vmdk|xci|z64|zip|keys)$$' || true)"; \
	fi; \
	if [ -n "$$payloads" ]; then \
		echo "payload artifacts are not allowed in tests/utils:"; \
		printf '%s\n' "$$payloads"; \
		exit 1; \
	fi; \
	echo "OK payload audit"

compiler-build: $(SEMU_BIN) ## Run the compiler build planner against repo definitions
	$(TEST_ENV) $(SEMU_BIN) build target steam-deck --project "$(TEST_PROJECT)"

compiler-verify: $(SEMU_BIN) ## Run compiler definition validation against repo definitions
	$(TEST_ENV) $(SEMU_BIN) verify target steam-deck --project "$(TEST_PROJECT)"

source-hook-metadata: ## Validate source-hook rendering and package metadata
	tests/source-hook-metadata.sh

rendering-contract: ## Validate classic system source-hook shader/bezel contracts
	tests/rendering-contract.sh

esde-settings-entry: $(SEMU_BIN) ## Validate the generated ES-DE Semu Settings entry
	$(TEST_ENV) SEMU_BIN="$(SEMU_BIN)" tests/esde-settings-entry.sh

cemu-ryujinx-input: $(SEMU_BIN) ## Validate generated Cemu/Ryujinx Steam Deck input configs
	$(TEST_ENV) SEMU_BIN="$(SEMU_BIN)" tests/cemu-ryujinx-input.sh

retroarch-n64-config: $(SEMU_BIN) ## Validate generated RetroArch N64 core options route through Semu rendering
	$(TEST_ENV) SEMU_BIN="$(SEMU_BIN)" tests/retroarch-n64-config.sh

test: payload-audit source-hook-metadata rendering-contract compiler-verify esde-settings-entry cemu-ryujinx-input retroarch-n64-config ## Run fast host checks for the compiler layout

ftux-test: compiler-build ## Validate the first-run compiler build path

deck-ssh-smoke: ## Probe a physical Steam Deck over SSH and capture a baseline screenshot
	tests/deck/ssh-smoke.sh

include tests/bazzite/Makefile
