SHELL := /bin/bash

ROOT ?= $(CURDIR)
BTRC_TRANSPILE ?= nix run .\#btrcpy --
CC ?= cc
ZIG_CC := nix shell nixpkgs\#zig --command zig cc

DECK_DIR := tests/targets/steamdeck
HARNESS_SOURCE := $(DECK_DIR)/main.btrc
HARNESS_MODULES := $(DECK_DIR)/steamdeck_harness.btrc \
	$(DECK_DIR)/acceptance_plan.btrc $(DECK_DIR)/acceptance_evidence.btrc \
	$(DECK_DIR)/deck_transport.btrc $(DECK_DIR)/deck_preflight.btrc \
	$(DECK_DIR)/deck_deploy.btrc $(DECK_DIR)/game_mode.btrc \
	$(DECK_DIR)/evidence.btrc $(DECK_DIR)/input_automation.btrc \
	$(DECK_DIR)/steam_shortcuts.btrc
REMOTE_HELPER_SOURCE := $(DECK_DIR)/remote_helper.btrc
REMOTE_HELPER_MODULES := $(DECK_DIR)/remote_system.btrc \
	$(DECK_DIR)/game_mode.btrc $(DECK_DIR)/evidence.btrc \
	$(DECK_DIR)/steam_shortcuts.btrc
UINPUT_SOURCE := $(DECK_DIR)/uinput_driver.btrc
INSTALLER_CONTRACT_SOURCE := $(DECK_DIR)/installer_contract_main.btrc
CONTRACT_SOURCE := $(DECK_DIR)/contract_test.btrc
FIXTURE := $(DECK_DIR)/fixture.json

HARNESS_C := build/tests/steamdeck-harness.c
HARNESS_BIN := build/tests/steamdeck-harness
REMOTE_HELPER_C := build/tests/steamdeck-acceptance-helper.c
REMOTE_HELPER_BIN := build/tests/steamdeck-acceptance-helper-x86_64-linux
UINPUT_C := build/tests/semu-deck-uinput.c
UINPUT_BIN := build/tests/semu-deck-uinput-x86_64-linux
INSTALLER_CONTRACT_C := build/tests/appimage-installer-contract.c
CONTRACT_C := build/tests/steamdeck-harness-contract.c
CONTRACT_BIN := build/tests/steamdeck-harness-contract

SEMU_DECK_HOST ?= deck@steamdeck.local
SEMU_DECK_SSH_OPTS ?= -o ConnectTimeout=5 -o BatchMode=yes
SEMU_DECK_APPIMAGE ?= $(CURDIR)/build/Semu-x86_64.AppImage
SEMU_APPIMAGE_RUNTIME_ROOT ?= $(HOME)/.cache/semu/appimage-runtime/runtime-root
SEMU_DECK_INSTALLER ?= $(CURDIR)/packaging/install/installer.sh
SEMU_DECK_PROJECT ?= /home/deck/.local/share/semu
SEMU_DECK_ROMS ?= /run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
SEMU_DECK_BIOS ?= /run/media/deck/SD/Emulation/RetroArch/config/system
SEMU_DECK_EXTERNAL_STATE ?= /run/media/deck/SD/Emulation
SEMU_DECK_ESDE_HOME ?= /home/deck/ES-DE
SEMU_DECK_STEAM_ROOT ?= /home/deck/.local/share/Steam
SEMU_DECK_EVIDENCE ?= $(CURDIR)/build/verification/steamdeck
SYSTEM ?=
HUMAN_OBSERVED ?= 0

COMMON_ARGS = --project "$(CURDIR)" --host "$(SEMU_DECK_HOST)" \
	--ssh-options="$(SEMU_DECK_SSH_OPTS)" --artifact "$(SEMU_DECK_APPIMAGE)" \
	--runtime-root "$(SEMU_APPIMAGE_RUNTIME_ROOT)" \
	--installer "$(SEMU_DECK_INSTALLER)" --remote-project "$(SEMU_DECK_PROJECT)" \
	--roms-root "$(SEMU_DECK_ROMS)" --bios-root "$(SEMU_DECK_BIOS)" \
	--external-state-root "$(SEMU_DECK_EXTERNAL_STATE)" \
	--es-de-home "$(SEMU_DECK_ESDE_HOME)" --steam-root "$(SEMU_DECK_STEAM_ROOT)" \
	--evidence-root "$(SEMU_DECK_EVIDENCE)" \
	--remote-helper "$(REMOTE_HELPER_BIN)" --input-helper "$(UINPUT_BIN)"
HUMAN_ARG = $(if $(filter 1,$(HUMAN_OBSERVED)),--human-observed,)

.PHONY: build helpers contract-test installer-contract test plan preflight steam-launch deploy prepare \
	automated operator-start-select operator-radial accept-system accept help

build: $(HARNESS_BIN) $(CONTRACT_BIN) helpers

$(HARNESS_BIN): $(HARNESS_SOURCE) $(HARNESS_MODULES) $(shell find src/compiler src/lib -name '*.btrc' -print) $(shell find config -type f -print)
	@mkdir -p "$(dir $@)"
	$(BTRC_TRANSPILE) "$<" -o "$(HARNESS_C)" --strict-imports --no-cache --no-stdlib
	$(CC) "$(HARNESS_C)" -std=c11 -o "$@" -lm

$(REMOTE_HELPER_C): $(REMOTE_HELPER_SOURCE) $(REMOTE_HELPER_MODULES)
	@mkdir -p "$(dir $@)"
	$(BTRC_TRANSPILE) "$<" -o "$@" --strict-imports --no-cache --no-stdlib

$(REMOTE_HELPER_BIN): $(REMOTE_HELPER_C)
	$(ZIG_CC) -target x86_64-linux-gnu -O2 -std=c11 "$<" -o "$@" -lm
	@file "$@" | grep -q 'ELF 64-bit'

$(UINPUT_C): $(UINPUT_SOURCE)
	@mkdir -p "$(dir $@)"
	$(BTRC_TRANSPILE) "$<" -o "$@" --strict-imports --no-cache --no-stdlib

$(UINPUT_BIN): $(UINPUT_C)
	$(ZIG_CC) -target x86_64-linux-gnu -O2 -std=c11 "$<" -o "$@" -lm
	@file "$@" | grep -q 'ELF 64-bit'

helpers: $(REMOTE_HELPER_BIN) $(UINPUT_BIN)

$(CONTRACT_BIN): $(CONTRACT_SOURCE) $(HARNESS_MODULES) $(REMOTE_HELPER_SOURCE) \
	$(REMOTE_HELPER_MODULES) $(UINPUT_SOURCE) $(FIXTURE)
	@mkdir -p "$(dir $@)"
	$(BTRC_TRANSPILE) "$<" -o "$(CONTRACT_C)" --strict-imports --no-cache --no-stdlib
	$(CC) "$(CONTRACT_C)" -std=c11 -o "$@" -lm

contract-test: $(CONTRACT_BIN)
	"$(CONTRACT_BIN)" --project "$(CURDIR)" --fixture "$(FIXTURE)"

$(INSTALLER_CONTRACT_C): $(INSTALLER_CONTRACT_SOURCE) \
	$(shell find src/generators/appimage -name '*.btrc' -print) \
	$(shell find packaging/install packaging/appimage -type f -print) \
	$(shell find config -type f -print)
	@mkdir -p "$(dir $@)"
	$(BTRC_TRANSPILE) "$<" -o "$@" --strict-imports --no-cache --no-stdlib

installer-contract: $(INSTALLER_CONTRACT_C)
	sh "$(ROOT)/$(DECK_DIR)/installer_contract_test.sh" \
		"$(ROOT)" "$(ROOT)/$(INSTALLER_CONTRACT_C)"

plan: $(HARNESS_BIN)
	"$(HARNESS_BIN)" plan $(COMMON_ARGS) --output "$(SEMU_DECK_EVIDENCE)/acceptance-plan.json"

test: contract-test plan

preflight steam-launch deploy prepare: $(HARNESS_BIN) helpers
	"$(HARNESS_BIN)" $@ $(COMMON_ARGS)

automated operator-start-select operator-radial accept-system: $(HARNESS_BIN) helpers
	@test -n "$(SYSTEM)" || { echo 'SYSTEM is required' >&2; exit 64; }
	"$(HARNESS_BIN)" $@ $(COMMON_ARGS) --system "$(SYSTEM)" $(HUMAN_ARG)

accept: $(HARNESS_BIN) helpers
	"$(HARNESS_BIN)" accept $(COMMON_ARGS) $(HUMAN_ARG)

help:
	@grep -E '^[a-zA-Z0-9_.-]+:.*## ' "$(firstword $(MAKEFILE_LIST))" || true
