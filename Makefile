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

include mk/build.mk
include tests/Makefile

.PHONY: dev help

dev: ## Enter the flake dev shell with btrcpy and test tooling
	nix develop

help: ## Show available targets
	@grep -hE '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-24s\033[0m %s\n", $$1, $$2}'

.DEFAULT_GOAL := help
