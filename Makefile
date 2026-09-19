SHELL := /bin/sh
BTRC ?= $(shell command -v btrcpy 2>/dev/null || echo "nix run --no-warn-dirty .\#btrcpy --")
CC ?= cc
CFLAGS ?= -std=c11 -O1
LIBS ?= -lm
TRANSPILE := $(BTRC) --strict-imports --no-cache --no-stdlib

BUILD := build
SOURCES := $(shell find src tests -name '*.btrc' | sort)
CONFIG := $(shell find config -type f | sort)
SEMU_BIN := $(BUILD)/semu
CONTRACTS_BIN := $(BUILD)/contracts
TARGET ?= linux-desktop
ASSET_ROOT ?= $(BUILD)/nix/result

.PHONY: all build test configs nix nix-check prepare doctor clean help

all: build ## Build the semu CLI

build: $(SEMU_BIN)

$(BUILD)/semu.c: $(SOURCES)
	@mkdir -p "$(BUILD)"
	$(TRANSPILE) "$(CURDIR)/src/semu.btrc" -o "$(CURDIR)/$@"

$(SEMU_BIN): $(BUILD)/semu.c
	$(CC) $(CFLAGS) "$(CURDIR)/$<" -o "$(CURDIR)/$@" $(LIBS)

$(BUILD)/contracts.c: $(SOURCES)
	@mkdir -p "$(BUILD)"
	$(TRANSPILE) "$(CURDIR)/tests/contracts/main.btrc" -o "$(CURDIR)/$@"

$(CONTRACTS_BIN): $(BUILD)/contracts.c
	$(CC) $(CFLAGS) "$(CURDIR)/$<" -o "$(CURDIR)/$@" $(LIBS)

test: $(CONTRACTS_BIN) $(SEMU_BIN) ## Run the contract tests against the real config tree
	SEMU_PROJECT="$(CURDIR)" "$(CURDIR)/$(CONTRACTS_BIN)"

configs: $(SEMU_BIN) ## Emit ES-DE documents and emulator profiles for TARGET
	"$(CURDIR)/$(SEMU_BIN)" build configs --target "$(TARGET)" --project "$(CURDIR)" \
		--asset-root "$(abspath $(ASSET_ROOT))" --output "$(CURDIR)/$(BUILD)/targets/$(TARGET)"

nix: ## Build the composed bundle (CLI, ES-DE, emulators) at build/nix/result
	@mkdir -p "$(BUILD)/nix"
	nix build --no-warn-dirty --out-link "$(BUILD)/nix/result" ".#semu"

nix-check: ## Evaluate every flake output
	nix flake check --no-warn-dirty --no-build

prepare: nix ## Install ES-DE documents and settings for TARGET using the built bundle
	"$(CURDIR)/$(BUILD)/nix/result/bin/semu" prepare --target "$(TARGET)"

doctor: $(SEMU_BIN) ## Show resolved paths and what is missing for TARGET
	"$(CURDIR)/$(SEMU_BIN)" doctor --target "$(TARGET)" --project "$(CURDIR)" --asset-root "$(abspath $(ASSET_ROOT))"

clean:
	rm -rf "$(BUILD)"

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-12s %s\n", $$1, $$2}'
