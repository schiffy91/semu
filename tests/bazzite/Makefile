# =============================================================================
# Bazzite Deck VM (SteamOS-like, software-rendered on Apple Silicon)
# =============================================================================

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

.PHONY: bazzite-iso bazzite-vm-start bazzite-vm-start-live bazzite-vm-start-installed bazzite-vm-screenshot bazzite-vm-smoke bazzite-desktop-vm-smoke bazzite-vm-ssh bazzite-vm-sync bazzite-vm-verify-ssh bazzite-vm-stop bazzite-vm-clean

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
