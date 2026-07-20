SHELL := /bin/bash

BTRC_TRANSPILE := nix run .\#btrcpy --

TARGET ?= steam-deck
EMULATOR ?=
ACTION ?= help

SEMU_SOURCE := src/main.btrc
SEMU_SOURCES := $(SEMU_SOURCE) src/cli.btrc $(shell find src/compiler src/generators src/lib -name '*.btrc' -print 2>/dev/null)
SEMU_CONFIG := $(shell find config -type f -print 2>/dev/null)
SEMU_C := build/semu.c
SEMU_BIN := build/semu
NIX_RESULT := build/nix/result

.PHONY: btrc-build cross-linux nix-build target configs emulator \
	appimage-runtime appimage-build appimage-verify steamdeck bazzite help

btrc-build: $(SEMU_BIN) ## Build the BTRC semu CLI

$(SEMU_BIN): $(SEMU_SOURCES) $(SEMU_CONFIG) flake.nix flake.lock Makefile
	@mkdir -p "$(dir $(SEMU_C))" "$(dir $(SEMU_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(SEMU_SOURCE)" -o "$(CURDIR)/$(SEMU_C)" --strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(SEMU_C)"
	$(CC) "$(SEMU_C)" -std=c11 -o "$@" -lm
# Static x86_64-linux CLI and input supervisor for immutable Linux targets.
cross-linux: $(SEMU_BIN) ## Cross-build build/semu-linux-x64 (static musl)
	nix shell nixpkgs\#zig --command zig cc -target x86_64-linux-musl -std=c11 \
		"$(SEMU_C)" -o build/semu-linux-x64 -lm
	@file build/semu-linux-x64 | grep -q "ELF 64-bit" || \
		{ echo "cross-linux did not produce an x86_64 ELF"; exit 2; }
	@mkdir -p build/steamdeck/input
	$(BTRC_TRANSPILE) src/generators/input/linux_supervisor.btrc \
		-o build/steamdeck/input/linux_supervisor.c \
		--strict-imports --no-cache --no-stdlib
	nix shell nixpkgs\#zig --command zig cc -target x86_64-linux-gnu -O2 -std=c11 \
		build/steamdeck/input/linux_supervisor.c \
		-o build/steamdeck/input/semu-input-supervisor

nix-build: ## Build the composed Nix package
	@mkdir -p "$(dir $(NIX_RESULT))"
	nix build --out-link "$(NIX_RESULT)" .\#default

target: $(SEMU_BIN) ## Compile TARGET (default: steam-deck)
	$(SEMU_BIN) build target "$(TARGET)" --project "$(CURDIR)"

configs: $(SEMU_BIN) ## Compile generated configs for TARGET (default: steam-deck)
	$(SEMU_BIN) build configs --target "$(TARGET)" --project "$(CURDIR)"

emulator: $(SEMU_BIN) ## Compile EMULATOR for TARGET
	@test -n "$(EMULATOR)" || { echo "EMULATOR is required" >&2; exit 64; }
	$(SEMU_BIN) build emulator "$(EMULATOR)" --target "$(TARGET)" \
		--project "$(CURDIR)"

# The shippable x86_64 AppImage is assembled by the compiler from an immutable
# runtime root; packaging policy remains in BTRC and Nix.
APPIMAGE_OUTPUT := $(CURDIR)/build/Semu-x86_64.AppImage
APPIMAGE_WORK_OPTION = $(if $(strip $(SEMU_APPIMAGE_WORK_ROOT)),--work-root "$(SEMU_APPIMAGE_WORK_ROOT)",)
ifeq ($(origin SEMU_APPIMAGE_RUNTIME_ROOT),undefined)
SEMU_APPIMAGE_RUNTIME_ROOT := $(HOME)/.cache/semu/appimage-runtime/runtime-root
APPIMAGE_RUNTIME_COMMAND = packaging/appimage/build_runtime.sh "$(SEMU_APPIMAGE_RUNTIME_ROOT)"
else
APPIMAGE_RUNTIME_COMMAND = test -d "$(SEMU_APPIMAGE_RUNTIME_ROOT)" || { \
	echo "runtime root does not exist: $(SEMU_APPIMAGE_RUNTIME_ROOT)" >&2; exit 2; }
endif
appimage-runtime: ## Build or validate the pinned x86_64 Deck runtime root
	$(APPIMAGE_RUNTIME_COMMAND)

appimage-build: $(SEMU_BIN) appimage-runtime ## Assemble and verify the AppImage without deploying it
	$(SEMU_BIN) package appimage --target steam-deck --project "$(CURDIR)" \
		--runtime-root "$(SEMU_APPIMAGE_RUNTIME_ROOT)" $(APPIMAGE_WORK_OPTION) \
		--output "$(APPIMAGE_OUTPUT)"

appimage-verify: $(SEMU_BIN) ## Verify exact existing AppImage bytes without rebuilding
	@test -x "$(APPIMAGE_OUTPUT)" || \
		{ echo "missing AppImage; run 'make appimage-build' first" >&2; exit 2; }
	$(SEMU_BIN) package appimage verify --target steam-deck --project "$(CURDIR)" \
		--runtime-root "$(SEMU_APPIMAGE_RUNTIME_ROOT)" $(APPIMAGE_WORK_OPTION) \
		--artifact "$(APPIMAGE_OUTPUT)"

steamdeck: ## Delegate ACTION=<target> to the Steam Deck harness
	$(MAKE) -f tests/targets/steamdeck/Makefile "$(ACTION)"

bazzite: ## Delegate ACTION=<target> after physical Deck acceptance
	$(MAKE) -f tests/targets/bazzite/Makefile "$(ACTION)"

include tests/Makefile

help: ## Show available targets
	@grep -hE '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-26s\033[0m %s\n", $$1, $$2}'

.DEFAULT_GOAL := help
