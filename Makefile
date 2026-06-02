SHELL := /bin/bash

BTRCPY ?= $(if $(SEMU_BTRCPY),$(SEMU_BTRCPY),btrcpy)
BTRC_FLAKE ?=
BTRC_INPUT_ARGS ?= $(if $(BTRC_FLAKE),--override-input btrc "$(BTRC_FLAKE)",)
BTRC_TRANSPILE ?= $(if $(BTRC_FLAKE),nix run $(BTRC_INPUT_ARGS) .\#btrcpy --,$(BTRCPY))

SEMU_SOURCE := src/semu.btrc
SEMU_SOURCES := $(SEMU_SOURCE) $(shell find src/semu -name '*.btrc' 2>/dev/null)
SEMU_C := build/semu.c
SEMU_BIN := build/semu

VM_DIR := tests/vms
E2E_GRAPH := tests/e2e/graph.json
E2E_GRAPH_NODES ?=
E2E_GRAPH_ARGS ?=

.PHONY: all install setup btrc-build manifest

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

include tests/Makefile

.PHONY: dev help

dev: ## Enter the flake dev shell with btrcpy and test tooling
	nix develop

help: ## Show available targets
	@grep -hE '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-24s\033[0m %s\n", $$1, $$2}'

.DEFAULT_GOAL := help
