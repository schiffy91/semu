SHELL := /bin/bash
CONTAINER_ENGINE := $(shell command -v podman 2>/dev/null || command -v docker 2>/dev/null)
CONTAINER_IMAGE := semu-test
BTRC_ROOT ?= ../../../dev/btrc
BTRC_FLAKE ?=
BTRC_INPUT_ARGS ?= $(if $(BTRC_FLAKE),--override-input btrc "$(BTRC_FLAKE)",)
BTRC_USE_FLAKE ?= 1
ifeq ($(BTRC_USE_FLAKE),1)
BTRC_TRANSPILE := nix run $(BTRC_INPUT_ARGS) .\#btrcpy --
else
BTRC_TRANSPILE := cd "$(BTRC_ROOT)" && nix develop --command ./bin/btrcpy
endif
SEMU_SOURCE := src/semu.btrc
SEMU_SOURCES := $(SEMU_SOURCE) $(shell find src/semu -name '*.btrc' 2>/dev/null)
SEMU_C := build/semu.c
SEMU_BIN := build/semu
VM_DIR := tests/vms
E2E_GRAPH := tests/e2e/graph.json
E2E_GRAPH_NODES ?=
E2E_GRAPH_ARGS ?=
CLOUD_INIT := tests/cloud-init
SSH_PORT_LINUX := 2222
VM_KEY := $(VM_DIR)/id_ed25519
SSH_OPTS := -o "StrictHostKeyChecking=no" -o "UserKnownHostsFile=/dev/null" -o "LogLevel=ERROR" -i "$(VM_KEY)"

ARCH_IMAGE_URL := https://geo.mirror.pkgbuild.com/images/latest/Arch-Linux-x86_64-cloudimg.qcow2
ARCH_BASE := $(VM_DIR)/arch-base.qcow2
LINUX_DISK := $(VM_DIR)/linux.qcow2
LINUX_SEED := $(VM_DIR)/seed.img
LINUX_PID := $(VM_DIR)/linux.pid
BAZZITE_ISO_URL ?= https://download.bazzite.gg/bazzite-deck-stable-live-amd64.iso
BAZZITE_ISO := $(VM_DIR)/bazzite-deck-stable-live-amd64.iso
BAZZITE_ISO_CHECKSUM_URL ?= $(BAZZITE_ISO_URL)-CHECKSUM
BAZZITE_ISO_SHA256 ?=
BAZZITE_VERIFY_ISO ?= 1
BAZZITE_DESKTOP_ISO_URL ?= https://download.bazzite.gg/bazzite-stable-live-amd64.iso
BAZZITE_DESKTOP_ISO := $(VM_DIR)/bazzite-stable-live-amd64.iso
BAZZITE_DESKTOP_DISK := $(VM_DIR)/bazzite-desktop.qcow2
BAZZITE_DESKTOP_PID := $(VM_DIR)/bazzite-desktop.pid
BAZZITE_DESKTOP_STARTED := $(VM_DIR)/bazzite-desktop.started
BAZZITE_DESKTOP_MONITOR := $(VM_DIR)/bazzite-desktop-monitor.sock
BAZZITE_DESKTOP_SERIAL := $(VM_DIR)/bazzite-desktop-serial.log
BAZZITE_DESKTOP_SCREEN := $(VM_DIR)/bazzite-desktop-screen.ppm
BAZZITE_DISK := $(VM_DIR)/bazzite.qcow2
BAZZITE_DISK_SIZE ?= 80G
BAZZITE_PID := $(VM_DIR)/bazzite.pid
BAZZITE_STARTED := $(VM_DIR)/bazzite.started
BAZZITE_MONITOR := $(VM_DIR)/bazzite-monitor.sock
BAZZITE_SERIAL := $(VM_DIR)/bazzite-serial.log
BAZZITE_SCREEN := $(VM_DIR)/bazzite-screen.ppm
BAZZITE_SCREEN_TMP ?= /tmp/semu-bazzite-screen.ppm
BAZZITE_SCREEN_WIDTH ?= 1280
BAZZITE_SCREEN_HEIGHT ?= 800
BAZZITE_MIN_NONBLACK_PERCENT ?= 0.1
BAZZITE_MEM ?= 8192
BAZZITE_SMP ?= 4
BAZZITE_VNC_DISPLAY ?= 5
BAZZITE_VNC_PORT ?= $(shell expr 5900 + $(BAZZITE_VNC_DISPLAY))
BAZZITE_SSH_PORT ?= 2223
BAZZITE_OVMF ?= /opt/homebrew/share/qemu/edk2-x86_64-code.fd
BAZZITE_OVMF_VARS ?= $(VM_DIR)/bazzite-ovmf-vars.fd
BAZZITE_OVMF_VARS_TEMPLATE ?=
BAZZITE_SSH_USER ?= bazzite
BAZZITE_BOOT_MODE ?= live
BAZZITE_BASIC_GRAPHICS ?= 0
BAZZITE_BASIC_GRAPHICS_KEYS ?= 30
BAZZITE_BASIC_GRAPHICS_KEY_DELAY ?= 0.2
BAZZITE_MIN_BOOT_WAIT ?= 30
BAZZITE_BOOT_WAIT ?= 180
BAZZITE_SMOKE_POLL ?= 10
BAZZITE_SSH_OPTS := -o "StrictHostKeyChecking=no" -o "UserKnownHostsFile=/dev/null" -o "LogLevel=ERROR" -o "BatchMode=yes" -o "ConnectTimeout=5" -o "ConnectionAttempts=1" -i "$(VM_KEY)"

# =============================================================================
# Setup (build bundle + write declarative runtime config)
# =============================================================================

.PHONY: all install setup btrc-build generated-build generated-smoke manifest verify payload-audit ftux-test container-build container-test test e2e-smoke e2e-graph-list e2e-graph-status e2e-graph-coverage e2e-graph-run lifecycle-smoke sandbox-smoke launcher-smoke appimage-smoke nix-e2e help

all: install ## Build all emulators + bootstrap content (idempotent, cached by nix)
install: setup
btrc-build: $(SEMU_BIN) ## Build the BTRC semu CLI

$(SEMU_BIN): $(SEMU_SOURCES) flake.nix flake.lock Makefile
	@mkdir -p build
	$(BTRC_TRANSPILE) "$(CURDIR)/$(SEMU_SOURCE)" -o "$(CURDIR)/$(SEMU_C)" --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(SEMU_C)"
	$(CC) "$(SEMU_C)" -std=c11 -o "$@" -lm
	@mkdir -p generated
	cp "$(SEMU_C)" generated/semu.c

manifest: $(SEMU_BIN) ## Generate semu.json from semu.btrc
	$(SEMU_BIN) manifest --output semu.json

setup: $(SEMU_BIN) ## Build all emulators and bootstrap Steam Deck/Linux content
	@echo "Building semu bundle (nix handles caching)..."
	nix build .#default
	@echo ""
	@# Extract Ryujinx on first run (.NET needs writable dir)
	@if [ -f result/bin/ryujinx ] && [ ! -d "$$HOME/.local/share/ryujinx-app/Ryujinx.app" ]; then \
		echo "Extracting Ryujinx (first run)..."; \
		result/bin/ryujinx --help >/dev/null 2>&1 || true; \
	fi
	@echo ""
	@echo "Bootstrapping Steam Deck/Linux-style content folders..."
	$(SEMU_BIN) bootstrap --project "$$(pwd)"
	@echo ""
	@echo "Done. Launch emulators:"
	@echo "  open result/Applications/ES-DE.app"
	@echo "  open result/Applications/azahar.app"
	@echo "  open result/Applications/Dolphin.app"
	@echo "  open result/Applications/RetroArch.app"
	@echo "  open result/Applications/PCSX2.app"
	@echo "  open result/Applications/Cemu.app"
	@echo "  result/bin/ryujinx"

# =============================================================================
# Container tests (fast, deterministic, same locally and in CI)
# =============================================================================

container-build: ## Build test container image
	$(CONTAINER_ENGINE) build -t $(CONTAINER_IMAGE) .

container-test: ## Run tests in container (fast, deterministic)
	$(CONTAINER_ENGINE) build --no-cache -t $(CONTAINER_IMAGE) . && \
	$(CONTAINER_ENGINE) run --rm --platform linux/amd64 -v "$$(pwd):/semu" $(CONTAINER_IMAGE) \
		make -C /semu generated-build test

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
	grep -F 'input_enable_hotkey = "ctrl"' "$$verify_dir/retroarch.cfg" >/dev/null; \
	grep -F 'input_load_state = "a"' "$$verify_dir/retroarch.cfg" >/dev/null; \
	grep -F 'input_save_state = "s"' "$$verify_dir/retroarch.cfg" >/dev/null; \
	grep -F 'input_exit_emulator = "q"' "$$verify_dir/retroarch.cfg" >/dev/null; \
	grep -F 'General/Stop = @(Ctrl+Q)' "$$verify_dir/dolphin.ini" >/dev/null; \
	grep -F 'Save State/Save State Slot 1 = @(Ctrl+S)' "$$verify_dir/dolphin.ini" >/dev/null; \
	grep -F 'SaveStateToSlot = Keyboard/Control & Keyboard/S' "$$verify_dir/pcsx2.ini" >/dev/null; \
	grep -F 'LoadStateFromSlot = Keyboard/Control & Keyboard/A' "$$verify_dir/pcsx2.ini" >/dev/null; \
	grep -F 'key_press S, Save State' "$$verify_dir/steam-input.vdf" >/dev/null; \
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

# =============================================================================
# QEMU VM (full system, for flatpak/GUI/integration testing)
# =============================================================================

$(VM_DIR):
	mkdir -p $(VM_DIR)

$(VM_KEY): | $(VM_DIR)
	ssh-keygen -t ed25519 -f $@ -N "" -q

$(ARCH_BASE): | $(VM_DIR)
	@echo "Downloading Arch Linux cloud image (~540MB, one-time)..."
	curl -L -o $@ $(ARCH_IMAGE_URL)

$(LINUX_DISK): $(ARCH_BASE)
	cp $(ARCH_BASE) $@
	qemu-img resize $@ 10G

$(LINUX_SEED): $(VM_KEY) $(CLOUD_INIT)/user-data $(CLOUD_INIT)/meta-data | $(VM_DIR)
	@echo "Building cloud-init seed ISO..."
	@mkdir -p $(VM_DIR)/cloud-init-tmp
	@cp $(CLOUD_INIT)/meta-data $(VM_DIR)/cloud-init-tmp/
	@cat $(CLOUD_INIT)/user-data > $(VM_DIR)/cloud-init-tmp/user-data
	@echo "ssh_authorized_keys:" >> $(VM_DIR)/cloud-init-tmp/user-data
	@echo "  - $$(cat $(VM_KEY).pub)" >> $(VM_DIR)/cloud-init-tmp/user-data
	@mkisofs -output $@ -volid cidata -joliet -rock \
		$(VM_DIR)/cloud-init-tmp/user-data $(VM_DIR)/cloud-init-tmp/meta-data 2>/dev/null || \
	genisoimage -output $@ -volid cidata -joliet -rock \
		$(VM_DIR)/cloud-init-tmp/user-data $(VM_DIR)/cloud-init-tmp/meta-data 2>/dev/null || \
	(hdiutil makehybrid -o $(VM_DIR)/seed -iso -joliet -default-volume-name cidata \
		$(VM_DIR)/cloud-init-tmp/ && mv $(VM_DIR)/seed.iso $@)
	@rm -rf $(VM_DIR)/cloud-init-tmp

.PHONY: linux linux-ssh linux-sync linux-test linux-stop linux-clean linux-purge deck-vm-start deck-vm-sync deck-vm-provision deck-vm-verify deck-vm-verify-strict deck-vm-stop bazzite-iso bazzite-vm-start bazzite-vm-start-live bazzite-vm-start-installed bazzite-vm-screenshot bazzite-vm-smoke bazzite-desktop-vm-smoke bazzite-vm-ssh bazzite-vm-sync bazzite-vm-verify-ssh bazzite-vm-stop bazzite-vm-clean

linux: $(LINUX_DISK) $(LINUX_SEED) $(VM_KEY) ## Start Linux VM (full system)
	@if [ -f $(LINUX_PID) ] && kill -0 $$(cat $(LINUX_PID)) 2>/dev/null; then \
		echo "VM already running (pid $$(cat $(LINUX_PID)))"; \
	else \
		echo "Starting Arch Linux x86_64 VM..."; \
		qemu-system-x86_64 \
			-machine q35 \
			-cpu qemu64 \
			-m 2048 \
			-smp 2 \
			-drive file=$(LINUX_DISK),if=virtio \
			-drive file=$(LINUX_SEED),if=virtio,format=raw \
			-nic user,hostfwd=tcp::$(SSH_PORT_LINUX)-:22 \
			-display none \
			-daemonize \
			-pidfile $(LINUX_PID); \
	fi; \
	echo "Waiting for SSH..."; \
	for i in $$(seq 1 180); do \
		ssh -q $(SSH_OPTS) -o "ConnectTimeout=2" -p $(SSH_PORT_LINUX) arch@localhost true 2>/dev/null && break; \
		[ $$i -eq 180 ] && echo "Timed out" && exit 1; \
		sleep 3; \
	done; \
	echo "Waiting for cloud-init and guest rsync..."; \
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'if command -v cloud-init >/dev/null 2>&1; then cloud-init status --wait >/dev/null; fi'; \
	for i in $$(seq 1 60); do \
		ssh -q $(SSH_OPTS) -o "ConnectTimeout=2" -p $(SSH_PORT_LINUX) arch@localhost 'command -v rsync >/dev/null' 2>/dev/null && break; \
		[ $$i -eq 60 ] && echo "Timed out waiting for guest rsync" && exit 1; \
		sleep 2; \
	done; \
	echo "VM ready."

linux-ssh: ## SSH into Linux VM
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost

linux-sync: ## Sync project files into Linux VM
	@git ls-files -co --exclude-standard -z | while IFS= read -r -d '' path; do \
		[ -e "$$path" ] && printf '%s\0' "$$path"; \
	done | rsync -az --files-from=- --from0 \
		-e "ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX)" \
		. arch@localhost:~/semu/

linux-test: linux-sync ## Sync + run tests in Linux VM
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && make generated-build test'

deck-vm-start: linux ## Start Deck-like Linux VM

deck-vm-sync: deck-vm-start ## Sync project and build the guest BTRC binary in VM
	$(MAKE) linux-sync
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && make generated-build'

deck-vm-provision: deck-vm-sync ## Provision VM with Deck-style services/config
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && SEMU_BIN=$$PWD/build/semu build/semu deck provision --project "$$PWD"'

deck-vm-verify: deck-vm-provision ## Run Deck-style emulator/input/sync checks in VM
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && SEMU_BIN=$$PWD/build/semu build/semu deck verify-emulators --project "$$PWD"'
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && SEMU_BIN=$$PWD/build/semu build/semu deck verify-sync --project "$$PWD"'
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && SEMU_BIN=$$PWD/build/semu build/semu deck verify-input --project "$$PWD"'

deck-vm-verify-strict: deck-vm-provision ## Run Deck VM checks and fail if input devices/services are missing
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && SEMU_BIN=$$PWD/build/semu build/semu deck verify-emulators --project "$$PWD"'
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && SEMU_BIN=$$PWD/build/semu build/semu deck verify-sync --project "$$PWD"'
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/semu && SEMU_BIN=$$PWD/build/semu build/semu deck verify-input --strict --project "$$PWD"'

deck-vm-stop: linux-stop ## Stop Deck-like Linux VM

linux-stop: ## Stop Linux VM
	@if [ -f $(LINUX_PID) ] && kill -0 $$(cat $(LINUX_PID)) 2>/dev/null; then \
		kill $$(cat $(LINUX_PID)); rm -f $(LINUX_PID); echo "VM stopped."; \
	else \
		echo "No VM running."; rm -f $(LINUX_PID); \
	fi

linux-clean: linux-stop ## Delete Linux VM disk (keeps base image)
	rm -f $(LINUX_DISK) $(LINUX_SEED) $(LINUX_PID)

linux-purge: linux-clean ## Delete everything including base image
	rm -f $(ARCH_BASE)
	rm -f $(VM_KEY) $(VM_KEY).pub

# =============================================================================
# Bazzite Deck VM (SteamOS-like, software-rendered on Apple Silicon)
# =============================================================================

$(BAZZITE_ISO): | $(VM_DIR)
	@echo "Downloading Bazzite ISO (~10GB, resumable): $(BAZZITE_ISO_URL)"
	curl -L --fail -C - -o "$@" "$(BAZZITE_ISO_URL)"

$(BAZZITE_DISK): | $(VM_DIR)
	qemu-img create -f qcow2 "$@" "$(BAZZITE_DISK_SIZE)"

bazzite-iso: $(BAZZITE_ISO) ## Download current Bazzite Deck live ISO
	@if [ "$(BAZZITE_VERIFY_ISO)" = "1" ]; then \
		actual="$$(shasum -a 256 "$(BAZZITE_ISO)" | awk '{print $$1}')"; \
		if [ -n "$(BAZZITE_ISO_SHA256)" ]; then \
			expected="$(BAZZITE_ISO_SHA256)"; \
		else \
			expected="$$(curl -fsSL "$(BAZZITE_ISO_CHECKSUM_URL)" | awk 'match($$0,/[0-9a-fA-F]{64}/){print substr($$0,RSTART,RLENGTH); exit}')"; \
		fi; \
		if [ -z "$$expected" ]; then \
			echo "Could not resolve SHA256 for $(BAZZITE_ISO_URL)"; \
			exit 2; \
		fi; \
		if [ "$$actual" != "$$expected" ]; then \
			echo "SHA256 mismatch for $(BAZZITE_ISO)"; \
			echo "expected $$expected"; \
			echo "actual   $$actual"; \
			exit 2; \
		fi; \
		echo "OK Bazzite ISO SHA256 $$actual"; \
	fi

bazzite-vm-start-live: ## Boot Bazzite live ISO over VNC
	$(MAKE) bazzite-vm-start BAZZITE_BOOT_MODE=live

bazzite-vm-start-installed: ## Boot installed Bazzite disk over VNC without attaching the ISO
	$(MAKE) bazzite-vm-start BAZZITE_BOOT_MODE=installed BAZZITE_BASIC_GRAPHICS=0

bazzite-vm-start: $(BAZZITE_DISK) $(VM_KEY) ## Boot Bazzite VM over VNC
	@if [ "$(BAZZITE_BOOT_MODE)" != "live" ] && [ "$(BAZZITE_BOOT_MODE)" != "installed" ]; then \
		echo "BAZZITE_BOOT_MODE must be live or installed"; \
		exit 2; \
	fi
	@if [ "$(BAZZITE_BOOT_MODE)" = "live" ]; then \
		$(MAKE) bazzite-iso; \
	fi
	@if [ ! -f "$(BAZZITE_OVMF)" ]; then \
		echo "Missing OVMF firmware: $(BAZZITE_OVMF)"; \
		echo "Set BAZZITE_OVMF=/path/to/edk2-x86_64-code.fd"; \
		exit 2; \
	fi
	@rm -f "$(BAZZITE_STARTED)"
	@if [ -f "$(BAZZITE_PID)" ] && kill -0 "$$(cat "$(BAZZITE_PID)")" 2>/dev/null; then \
		echo "Bazzite VM already running (pid $$(cat "$(BAZZITE_PID)"))"; \
	else \
		set -e; \
		rm -f "$(BAZZITE_MONITOR)" "$(BAZZITE_SERIAL)"; \
		echo "Starting Bazzite VM ($(BAZZITE_BOOT_MODE), TCG/software rendering, VNC :$(BAZZITE_VNC_DISPLAY))..."; \
		ovmf_args=(-drive if=pflash,format=raw,readonly=on,file="$(BAZZITE_OVMF)"); \
		if [ -n "$(BAZZITE_OVMF_VARS_TEMPLATE)" ]; then \
			if [ ! -f "$(BAZZITE_OVMF_VARS)" ]; then \
				cp "$(BAZZITE_OVMF_VARS_TEMPLATE)" "$(BAZZITE_OVMF_VARS)"; \
			fi; \
			ovmf_args+=(-drive if=pflash,format=raw,file="$(BAZZITE_OVMF_VARS)"); \
		elif [ -f "$(BAZZITE_OVMF_VARS)" ]; then \
			ovmf_args+=(-drive if=pflash,format=raw,file="$(BAZZITE_OVMF_VARS)"); \
		fi; \
		if [ "$(BAZZITE_BOOT_MODE)" = "live" ]; then \
			boot_args=(-cdrom "$(BAZZITE_ISO)" -boot order=d,menu=on); \
		else \
			boot_args=(-boot order=c,menu=on); \
		fi; \
		qemu-system-x86_64 \
			-machine q35,accel=tcg \
			-cpu max \
			-m $(BAZZITE_MEM) \
			-smp $(BAZZITE_SMP) \
			"$${ovmf_args[@]}" \
			-drive file="$(BAZZITE_DISK)",if=virtio,format=qcow2,cache=writeback,discard=unmap \
			"$${boot_args[@]}" \
			-device virtio-vga \
			-display vnc=127.0.0.1:$(BAZZITE_VNC_DISPLAY) \
			-nic user,hostfwd=tcp::$(BAZZITE_SSH_PORT)-:22 \
			-serial file:"$(BAZZITE_SERIAL)" \
			-monitor unix:"$(BAZZITE_MONITOR)",server,nowait \
			-daemonize \
			-pidfile "$(BAZZITE_PID)"; \
		touch "$(BAZZITE_STARTED)"; \
	fi
	@echo "Bazzite VM VNC: 127.0.0.1:$(BAZZITE_VNC_PORT)"
	@echo "SSH after install/key setup: ssh -p $(BAZZITE_SSH_PORT) $(BAZZITE_SSH_USER)@localhost"
	@if [ "$(BAZZITE_BASIC_GRAPHICS)" = "1" ] && [ -f "$(BAZZITE_STARTED)" ]; then \
		echo "Selecting Bazzite Basic Graphics Mode..."; \
		for i in $$(seq 1 100); do \
			[ -S "$(BAZZITE_MONITOR)" ] && break; \
			sleep 0.1; \
		done; \
		if [ ! -S "$(BAZZITE_MONITOR)" ]; then \
			echo "Timed out waiting for Bazzite monitor socket"; \
			exit 2; \
		fi; \
		for i in $$(seq 1 $(BAZZITE_BASIC_GRAPHICS_KEYS)); do \
			printf 'sendkey down\n' | nc -U "$(BAZZITE_MONITOR)" >/dev/null; \
			sleep $(BAZZITE_BASIC_GRAPHICS_KEY_DELAY); \
		done; \
		printf 'sendkey ret\n' | nc -U "$(BAZZITE_MONITOR)" >/dev/null; \
	fi

bazzite-vm-screenshot: ## Capture current Bazzite VM framebuffer to PPM
	@if [ ! -S "$(BAZZITE_MONITOR)" ]; then \
		echo "Bazzite monitor socket not found; run make bazzite-vm-start first"; \
		exit 2; \
	fi
	@rm -f "$(BAZZITE_SCREEN_TMP)"
	@printf 'screendump $(BAZZITE_SCREEN_TMP)\n' | nc -U "$(BAZZITE_MONITOR)" >/dev/null
	@cp "$(BAZZITE_SCREEN_TMP)" "$(BAZZITE_SCREEN)"
	@perl -0777 -e 'my $$file = shift; open my $$fh, "<:raw", $$file or die "open $$file: $$!\n"; local $$/; my $$ppm = <$$fh>; $$ppm =~ /\AP6\s+(?:#[^\n]*\n\s*)?(\d+)\s+(\d+)\s+(\d+)\s/s or die "invalid PPM header\n"; my ($$w, $$h, $$max) = ($$1, $$2, $$3); die "unexpected framebuffer $${w}x$${h}, want $(BAZZITE_SCREEN_WIDTH)x$(BAZZITE_SCREEN_HEIGHT)\n" unless $$w == $(BAZZITE_SCREEN_WIDTH) && $$h == $(BAZZITE_SCREEN_HEIGHT) && $$max == 255; my $$pixels = substr($$ppm, length($$&)); die "blank framebuffer\n" unless $$pixels =~ /[^\0]/; my $$nonblack = ($$pixels =~ tr/\0//c); my $$percent = 100 * $$nonblack / length($$pixels); die sprintf("framebuffer too sparse %.2f%%, want %.2f%%\n", $$percent, $(BAZZITE_MIN_NONBLACK_PERCENT)) unless $$percent >= $(BAZZITE_MIN_NONBLACK_PERCENT);' "$(BAZZITE_SCREEN)"
	@ls -lh "$(BAZZITE_SCREEN)"

bazzite-vm-smoke: ## Boot Bazzite VM and capture a visual smoke screenshot
	$(MAKE) bazzite-vm-start-live BAZZITE_BASIC_GRAPHICS=1
	@echo "Waiting $(BAZZITE_MIN_BOOT_WAIT)s for firmware/GRUB to hand off..."
	@sleep $(BAZZITE_MIN_BOOT_WAIT)
	@echo "Waiting up to $(BAZZITE_BOOT_WAIT)s for a nonblank Bazzite framebuffer..."
	@ok=0; deadline=$$((SECONDS + $(BAZZITE_BOOT_WAIT))); \
	while [ $$SECONDS -lt $$deadline ]; do \
		if $(MAKE) --no-print-directory bazzite-vm-screenshot; then \
			ok=1; \
			break; \
		fi; \
		sleep $(BAZZITE_SMOKE_POLL); \
	done; \
	if [ $$ok -ne 1 ]; then \
		echo "Timed out waiting for a valid nonblank Bazzite framebuffer"; \
		exit 2; \
	fi
	@echo "Open the VNC display at 127.0.0.1:$(BAZZITE_VNC_PORT) for manual install/Game Mode checks."

bazzite-desktop-vm-smoke: ## Boot Bazzite Desktop ISO over VNC for no-GPU VM validation
	$(MAKE) bazzite-vm-smoke \
		BAZZITE_ISO_URL=$(BAZZITE_DESKTOP_ISO_URL) \
		BAZZITE_ISO=$(BAZZITE_DESKTOP_ISO) \
		BAZZITE_DISK=$(BAZZITE_DESKTOP_DISK) \
		BAZZITE_PID=$(BAZZITE_DESKTOP_PID) \
		BAZZITE_STARTED=$(BAZZITE_DESKTOP_STARTED) \
		BAZZITE_MONITOR=$(BAZZITE_DESKTOP_MONITOR) \
		BAZZITE_SERIAL=$(BAZZITE_DESKTOP_SERIAL) \
		BAZZITE_SCREEN=$(BAZZITE_DESKTOP_SCREEN) \
		BAZZITE_VNC_DISPLAY=6 \
		BAZZITE_VNC_PORT=5906 \
		BAZZITE_SSH_PORT=2234 \
		BAZZITE_MIN_NONBLACK_PERCENT=1

bazzite-vm-ssh: ## SSH into installed Bazzite VM once user/key exists
	ssh $(BAZZITE_SSH_OPTS) -p $(BAZZITE_SSH_PORT) $(BAZZITE_SSH_USER)@localhost

bazzite-vm-sync: ## Sync repo to installed Bazzite VM over SSH
	git ls-files -co --exclude-standard -z | rsync -az --files-from=- --from0 \
		-e "ssh $(BAZZITE_SSH_OPTS) -p $(BAZZITE_SSH_PORT)" \
		. $(BAZZITE_SSH_USER)@localhost:~/semu/
	ssh $(BAZZITE_SSH_OPTS) -p $(BAZZITE_SSH_PORT) $(BAZZITE_SSH_USER)@localhost \
		'cd ~/semu && make generated-build'

bazzite-vm-verify-ssh: bazzite-vm-sync ## Run Deck checks inside installed Bazzite VM over SSH
	ssh $(BAZZITE_SSH_OPTS) -p $(BAZZITE_SSH_PORT) $(BAZZITE_SSH_USER)@localhost \
		'cd ~/semu && build/semu deck provision --project "$$PWD"'
	ssh $(BAZZITE_SSH_OPTS) -p $(BAZZITE_SSH_PORT) $(BAZZITE_SSH_USER)@localhost \
		'cd ~/semu && build/semu deck verify-emulators --project "$$PWD"'
	ssh $(BAZZITE_SSH_OPTS) -p $(BAZZITE_SSH_PORT) $(BAZZITE_SSH_USER)@localhost \
		'cd ~/semu && build/semu deck verify-sync --project "$$PWD"'
	ssh $(BAZZITE_SSH_OPTS) -p $(BAZZITE_SSH_PORT) $(BAZZITE_SSH_USER)@localhost \
		'cd ~/semu && build/semu deck verify-input --project "$$PWD"'

bazzite-vm-stop: ## Stop Bazzite VM
	@if [ -f "$(BAZZITE_PID)" ] && kill -0 "$$(cat "$(BAZZITE_PID)")" 2>/dev/null; then \
		kill "$$(cat "$(BAZZITE_PID)")"; rm -f "$(BAZZITE_PID)" "$(BAZZITE_STARTED)" "$(BAZZITE_MONITOR)"; echo "Bazzite VM stopped."; \
	else \
		echo "No Bazzite VM running."; rm -f "$(BAZZITE_PID)" "$(BAZZITE_STARTED)" "$(BAZZITE_MONITOR)"; \
	fi

bazzite-vm-clean: bazzite-vm-stop ## Delete Bazzite VM disk and runtime sockets/logs
	rm -f "$(BAZZITE_DISK)" "$(BAZZITE_PID)" "$(BAZZITE_STARTED)" "$(BAZZITE_MONITOR)" "$(BAZZITE_SERIAL)" "$(BAZZITE_SCREEN)" "$(BAZZITE_OVMF_VARS)" "$(BAZZITE_ISO).CHECKSUM"

# =============================================================================
# Help
# =============================================================================

help: ## Show available targets
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2}'

.DEFAULT_GOAL := help
