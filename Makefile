SHELL := /bin/bash

BTRC_TRANSPILE := nix run .\#btrcpy --

SEMU_SOURCE := src/semu.btrc
SEMU_SOURCES := $(SEMU_SOURCE) $(shell find src/semu tests -path src/semu/garbage -prune -o -name '*.btrc' -print 2>/dev/null)
SEMU_C := src/generated/build/semu.c
SEMU_BIN := src/generated/build/semu
SEMU_MANIFEST := src/generated/semu.json
NIX_RESULT := src/generated/nix/result

.PHONY: all install setup btrc-build manifest help

all: install ## Build all emulators + bootstrap content (idempotent, cached by nix)
install: setup

btrc-build: $(SEMU_BIN) ## Build the BTRC semu CLI

$(SEMU_BIN): $(SEMU_SOURCES) flake.nix flake.lock Makefile
	@mkdir -p "$(dir $(SEMU_C))" "$(dir $(SEMU_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(SEMU_SOURCE)" -o "$(CURDIR)/$(SEMU_C)" --strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(SEMU_C)"
	$(CC) "$(SEMU_C)" -std=c11 -o "$@" -lm
	@mkdir -p src/generated
	cp "$(SEMU_C)" src/generated/semu.c

manifest: $(SEMU_BIN) ## Generate src/generated/semu.json from semu.btrc
	@mkdir -p "$(dir $(SEMU_MANIFEST))"
	$(SEMU_BIN) manifest --output "$(SEMU_MANIFEST)"

setup: $(SEMU_BIN) ## Build all emulators and bootstrap Steam Deck/Linux content
	@echo "Building semu bundle (nix handles caching)..."
	@mkdir -p "$(dir $(NIX_RESULT))"
	nix build --out-link "$(NIX_RESULT)" .#default
	@echo ""
	@# Extract Ryujinx on first run (.NET needs writable dir)
	@if [ -f "$(NIX_RESULT)/bin/ryujinx" ] && [ ! -d "$$HOME/.local/share/ryujinx-app/Ryujinx.app" ]; then \
		echo "Extracting Ryujinx (first run)..."; \
		"$(NIX_RESULT)/bin/ryujinx" --help >/dev/null 2>&1 || true; \
	fi
	@echo ""
	@echo "Bootstrapping Steam Deck/Linux-style content folders..."
	$(SEMU_BIN) bootstrap --project "$$(pwd)"
	@echo ""
	@echo "Done. Launch with $(NIX_RESULT)/bin/semu or the bundled Semu AppImage."

include tests/Makefile

help: ## Show available targets
	@grep -hE '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-26s\033[0m %s\n", $$1, $$2}'

.DEFAULT_GOAL := help
