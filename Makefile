SHELL := /bin/sh
BTRC ?= $(shell command -v btrcpy 2>/dev/null || echo "nix run --no-warn-dirty .\#btrcpy --")
CC ?= cc
CFLAGS ?= -std=c11 -O1 -Wno-unknown-warning-option -Wno-incompatible-pointer-types-discards-qualifiers -Wno-discarded-qualifiers  # BTRC passes volatile error slots
INCLUDES := -I"$(CURDIR)/src/launch"
LIBS ?= -lm
TRANSPILE := $(BTRC) --strict-imports --no-cache --no-stdlib

BUILD := build
SOURCES := $(shell find src tests/contracts \( -name '*.btrc' -o -name '*.h' \) | sort)
CONFIG := $(shell find config -type f | sort)
SEMU_BIN := $(BUILD)/semu
CONTRACTS_BIN := $(BUILD)/contracts
TARGET ?= linux-desktop
ASSET_ROOT ?= $(BUILD)/nix/result

.PHONY: all build test bezel-tree render-host configs nix nix-check nix-eval prepare doctor clean help

all: build ## Build the semu CLI

build: $(SEMU_BIN)

$(BUILD)/semu.c: $(SOURCES)
	@mkdir -p "$(BUILD)"
	$(TRANSPILE) "$(CURDIR)/src/semu.btrc" -o "$(CURDIR)/$@"

$(SEMU_BIN): $(BUILD)/semu.c
	$(CC) $(CFLAGS) $(INCLUDES) "$(CURDIR)/$<" -o "$(CURDIR)/$@" $(LIBS)

$(BUILD)/contracts.c: $(SOURCES)
	@mkdir -p "$(BUILD)"
	$(TRANSPILE) "$(CURDIR)/tests/contracts/main.btrc" -o "$(CURDIR)/$@"

$(CONTRACTS_BIN): $(BUILD)/contracts.c
	$(CC) $(CFLAGS) $(INCLUDES) "$(CURDIR)/$<" -o "$(CURDIR)/$@" $(LIBS)

BEZEL_TREE := $(BUILD)/bezel-tree

bezel-tree: ## Link the pinned Mega Bezel tree (slang shaders and packs) at build/bezel-tree
	@mkdir -p "$(BUILD)"
	nix build --no-warn-dirty --out-link "$(BEZEL_TREE)" ".#bezel-tree"

test: $(CONTRACTS_BIN) bezel-tree ## Run the contract tests against the real config tree
	SEMU_PROJECT="$(CURDIR)" SEMU_BEZEL_TREE="$(CURDIR)/$(BEZEL_TREE)/share/semu/bezel/shaders" "$(CURDIR)/$(CONTRACTS_BIN)"

configs: $(SEMU_BIN) ## Emit ES-DE documents and emulator profiles for TARGET
	"$(CURDIR)/$(SEMU_BIN)" build configs --target "$(TARGET)" --project "$(CURDIR)" \
		--asset-root "$(abspath $(ASSET_ROOT))" --output "$(CURDIR)/$(BUILD)/targets/$(TARGET)"

RENDERER_SOURCES := $(shell find src/renderer \( -name '*.btrc' -o -name '*.h' \) | sort)

render-host: $(BUILD)/render-host ## The real renderer offscreen on macOS for tests/visual/render.sh

$(BUILD)/render-host.c: tests/visual/render_host.btrc $(RENDERER_SOURCES)
	@mkdir -p "$(BUILD)"
	$(TRANSPILE) --no-dce "$(CURDIR)/tests/visual/render_host.btrc" -o "$(CURDIR)/$@"

$(BUILD)/render-host: $(BUILD)/render-host.c
	librashader="$$(nix build --no-warn-dirty --no-link --print-out-paths --inputs-from . nixpkgs#librashader)" && \
	$(CC) -std=c11 -O2 -w -DGL_SILENCE_DEPRECATION -DLIBRA_RUNTIME_OPENGL=1 -DSTB_IMAGE_IMPLEMENTATION -DSTBI_ONLY_PNG -DSTBI_ONLY_JPEG \
		-I"$(CURDIR)/src/renderer" -I"$$librashader/include" "$(CURDIR)/$<" -o "$(CURDIR)/$@" \
		-L"$$librashader/lib" -Wl,-rpath,"$$librashader/lib" -lrashader -framework OpenGL -lm

nix: ## Build the composed bundle (CLI, ES-DE, emulators) at build/nix/result
	@mkdir -p "$(BUILD)/nix"
	nix build --no-warn-dirty --out-link "$(BUILD)/nix/result" ".#semu"

nix-check: ## Build and run every flake check for this system
	nix flake check --no-warn-dirty

nix-eval: ## Evaluate every flake output without building
	nix flake check --no-warn-dirty --no-build

prepare: nix ## Install ES-DE documents and settings for TARGET using the built bundle
	"$(CURDIR)/$(BUILD)/nix/result/bin/semu" prepare --target "$(TARGET)"

doctor: $(SEMU_BIN) ## Show resolved paths and what is missing for TARGET
	"$(CURDIR)/$(SEMU_BIN)" doctor --target "$(TARGET)" --project "$(CURDIR)" --asset-root "$(abspath $(ASSET_ROOT))"

clean:
	rm -rf "$(BUILD)"

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-12s %s\n", $$1, $$2}'
