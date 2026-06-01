# =============================================================================
# Setup (build bundle + write declarative runtime config)
# =============================================================================

.PHONY: all install setup btrc-build manifest

all: install ## Build all emulators + bootstrap content (idempotent, cached by nix)
install: setup
btrc-build: $(SEMU_BIN) ## Build the BTRC semu CLI

$(SEMU_BIN): $(SEMU_SOURCES) flake.nix flake.lock Makefile mk/build.mk
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
