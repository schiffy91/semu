SHELL := /bin/bash

BTRC_FLAKE ?= github:schiffy91/btrc\#btrcpy
BTRC_TRANSPILE ?= $(shell command -v btrcpy 2>/dev/null || printf '%s' 'nix run $(BTRC_FLAKE) --')
SEMU_CC ?= $(shell if command -v cc >/dev/null 2>&1; then command -v cc; elif command -v gcc >/dev/null 2>&1; then command -v gcc; elif command -v nix >/dev/null 2>&1; then printf '%s' 'nix develop --command cc'; else printf '%s' cc; fi)
UNAME_S := $(shell uname -s 2>/dev/null || printf unknown)

SEMU_SOURCE := src/main.btrc
SEMU_QUIT_WATCH_SOURCE := src/lib/quit_watch.btrc
SEMU_SOURCES := $(shell find src -name '*.btrc' ! -path 'src/lib/quit_watch.btrc' 2>/dev/null)
SEMU_C := build/out/semu.c
SEMU_BIN := build/out/semu
SEMU_QUIT_WATCH_C := build/generated/semu-quit-watch.c
SEMU_QUIT_WATCH_BIN := build/out/semu-quit-watch
SEMU_QUIT_WATCH_TARGET := $(if $(filter Linux,$(UNAME_S)),$(SEMU_QUIT_WATCH_BIN),$(SEMU_QUIT_WATCH_C))
EMULATOR ?= all
SEMU_PROJECT ?= $(HOME)/Drive/media/Games/Emulation
SEMU_ASSET_ROOT ?= $(CURDIR)
SEMU_ENV := SEMU_ASSET_ROOT="$(SEMU_ASSET_ROOT)" SEMU_PROJECT_DIR="$(SEMU_PROJECT)"

.PHONY: all steam-deck emulator configs verify install setup btrc-build manifest help

all: steam-deck ## Build the default Steam Deck target

btrc-build: $(SEMU_BIN) $(SEMU_QUIT_WATCH_TARGET) ## Build the BTRC semu CLI and helpers

$(SEMU_BIN): $(SEMU_SOURCES) flake.nix flake.lock Makefile
	@mkdir -p build/out
	$(BTRC_TRANSPILE) "$(CURDIR)/$(SEMU_SOURCE)" -o "$(CURDIR)/$(SEMU_C)" --no-cache --no-stdlib --strict-imports
	perl -0pi -e 's/\n+\z/\n/' "$(SEMU_C)"
	$(SEMU_CC) "$(SEMU_C)" -std=c11 -o "$@" -lm
	@mkdir -p build/generated
	cp "$(SEMU_C)" build/generated/semu.c

$(SEMU_QUIT_WATCH_C): $(SEMU_QUIT_WATCH_SOURCE) flake.nix flake.lock Makefile
	@mkdir -p build/generated
	$(BTRC_TRANSPILE) "$(CURDIR)/$(SEMU_QUIT_WATCH_SOURCE)" -o "$(CURDIR)/$(SEMU_QUIT_WATCH_C)" --no-cache --no-stdlib --strict-imports
	perl -0pi -e 's/\n+\z/\n/' "$(SEMU_QUIT_WATCH_C)"

$(SEMU_QUIT_WATCH_BIN): $(SEMU_QUIT_WATCH_C)
	@mkdir -p build/out
	$(SEMU_CC) "$(SEMU_QUIT_WATCH_C)" -std=c11 -O2 -o "$@"

steam-deck: $(SEMU_BIN) ## Build the Steam Deck compiler target
	$(SEMU_ENV) $(SEMU_BIN) build target steam-deck --project "$(SEMU_PROJECT)"

emulator: $(SEMU_BIN) ## Build emulator outputs, optionally EMULATOR=name
	$(SEMU_ENV) $(SEMU_BIN) build emulator $(EMULATOR) --project "$(SEMU_PROJECT)"

configs: $(SEMU_BIN) ## Generate target configs and launch glue
	$(SEMU_ENV) $(SEMU_BIN) build configs --project "$(SEMU_PROJECT)"

verify: $(SEMU_BIN) ## Verify the Steam Deck compiler target
	$(SEMU_ENV) $(SEMU_BIN) verify target steam-deck --project "$(SEMU_PROJECT)"

install setup: steam-deck

manifest: configs

include tests/Makefile

help: ## Show available targets
	@grep -hE '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-26s\033[0m %s\n", $$1, $$2}'

.DEFAULT_GOAL := help
