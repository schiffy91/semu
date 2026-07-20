SHELL := /bin/bash

.PHONY: compiler-tests tree-audit tree-audit-contracts configs-smoke \
	steamdeck-contracts test verify verify-production

BTRC_TRANSPILE ?= nix run .\#btrcpy --
TARGET ?= steam-deck

ifeq ($(strip $(SEMU_BIN)),)
SEMU_BIN := build/semu
endif

SEMU_COMPILER_TEST_SOURCE := tests/compiler/main.btrc
SEMU_COMPILER_TEST_SOURCES := $(shell find tests/compiler src/compiler src/generators src/lib -name '*.btrc' -print 2>/dev/null) tests/core/compiler_contract_test.btrc
SEMU_COMPILER_TEST_C := build/tests/semu-compiler-tests.c
SEMU_COMPILER_TEST_BIN := build/tests/semu-compiler-tests
SEMU_CONFIG := $(shell find config -type f -print 2>/dev/null)

TREE_AUDIT_SOURCE := tests/core/tree_audit/main.btrc
TREE_AUDIT_SOURCES := $(shell find tests/core/tree_audit -name '*.btrc' -print 2>/dev/null)
TREE_AUDIT_C := build/tests/tree-audit.c
TREE_AUDIT_BIN := build/tests/tree-audit
STEAMDECK_HARNESS := tests/targets/steamdeck/Makefile

tree-audit: $(TREE_AUDIT_BIN) tree-audit-contracts ## Check the approved refactor shape
	$(TREE_AUDIT_BIN) --project "$(CURDIR)"

tree-audit-contracts: $(TREE_AUDIT_BIN) ## Exercise tree-policy rejection cases
	$(TREE_AUDIT_BIN) --self-test

$(TREE_AUDIT_BIN): $(TREE_AUDIT_SOURCES) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(TREE_AUDIT_C))" "$(dir $(TREE_AUDIT_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(TREE_AUDIT_SOURCE)" \
		-o "$(CURDIR)/$(TREE_AUDIT_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(TREE_AUDIT_C)"
	$(CC) "$(TREE_AUDIT_C)" -std=c11 -o "$@" -lm

compiler-tests: $(SEMU_COMPILER_TEST_BIN) ## Run the focused strict-import compiler contracts
	$(SEMU_COMPILER_TEST_BIN) --project "$(CURDIR)"

$(SEMU_COMPILER_TEST_BIN): $(SEMU_COMPILER_TEST_SOURCES) $(SEMU_CONFIG) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(SEMU_COMPILER_TEST_C))" "$(dir $(SEMU_COMPILER_TEST_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(SEMU_COMPILER_TEST_SOURCE)" \
		-o "$(CURDIR)/$(SEMU_COMPILER_TEST_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(SEMU_COMPILER_TEST_C)"
	$(CC) "$(SEMU_COMPILER_TEST_C)" -std=c11 -o "$@" -lm

configs-smoke: $(SEMU_BIN) ## Smoke-test generated configs without a device
	$(SEMU_BIN) build configs --target steam-deck --project "$(CURDIR)" \
		--output build/test/configs/steam-deck
	@test -f build/test/configs/steam-deck/build-plan.json

steamdeck-contracts: ## Run local Steam Deck contracts without SSH or a device
	$(MAKE) -f "$(STEAMDECK_HARNESS)" test installer-contract

test: tree-audit compiler-tests configs-smoke steamdeck-contracts ## Run all device-free contracts

verify: $(SEMU_BIN) compiler-tests ## Verify TARGET plan (default: steam-deck)
	"$(SEMU_BIN)" verify target "$(TARGET)" --project "$(CURDIR)" \
		--output "build/test/verify/$(TARGET)"

verify-production: compiler-tests appimage-verify ## Compiler contracts + exact production AppImage verification
