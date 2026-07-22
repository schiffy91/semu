SHELL := /bin/bash

.PHONY: compiler-tests appimage-contract appimage-trust-contract \
	appimage-nix-release-proof-contract \
	appimage-frozen-builder-contract \
	appimage-runtime-builder-lifecycle-contract \
	appimage-transaction-contract tree-audit \
	tree-audit-contracts configs-smoke gb-definition-contract \
	retroarch-runtime-contract steamdeck-contracts test verify \
	verify-production

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

GB_DEFINITION_CONTRACT_SOURCE := \
	tests/compiler/gb_definition_contract_main.btrc
GB_DEFINITION_CONTRACT_C := build/tests/gb-definition-contract.c
GB_DEFINITION_CONTRACT_BIN := build/tests/gb-definition-contract

APPIMAGE_CONTRACT_SOURCE := tests/compiler/appimage_contract_main.btrc
APPIMAGE_CONTRACT_SOURCES := $(shell find src/generators/appimage src/compiler src/lib -name '*.btrc' -print 2>/dev/null)
APPIMAGE_CONTRACT_C := build/tests/appimage-contract.c
APPIMAGE_CONTRACT_BIN := build/tests/appimage-contract

APPIMAGE_TRUST_CONTRACT_SOURCE := \
	tests/compiler/appimage_trust_contract_main.btrc
APPIMAGE_TRUST_CONTRACT_SOURCES := $(shell find \
	src/generators/appimage src/lib -name '*.btrc' -print 2>/dev/null)
APPIMAGE_TRUST_CONTRACT_C := build/tests/appimage-trust-contract.c
APPIMAGE_TRUST_CONTRACT_BIN := build/tests/appimage-trust-contract

APPIMAGE_NIX_PROOF_CONTRACT_SOURCE := \
	tests/compiler/appimage_nix_release_proof_contract_main.btrc
APPIMAGE_NIX_PROOF_CONTRACT_C := \
	build/tests/appimage-nix-release-proof-contract.c
APPIMAGE_NIX_PROOF_CONTRACT_BIN := \
	build/tests/appimage-nix-release-proof-contract

APPIMAGE_FROZEN_BUILDER_CONTRACT_SOURCE := \
	tests/compiler/appimage_frozen_builder_contract_main.btrc
APPIMAGE_FROZEN_BUILDER_CONTRACT_C := \
	build/tests/appimage-frozen-builder-contract.c
APPIMAGE_FROZEN_BUILDER_CONTRACT_BIN := \
	build/tests/appimage-frozen-builder-contract

APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_SOURCE := \
	tests/compiler/appimage_runtime_builder_lifecycle_contract_main.btrc
APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_C := \
	build/tests/appimage-runtime-builder-lifecycle-contract.c
APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_BIN := \
	build/tests/appimage-runtime-builder-lifecycle-contract

APPIMAGE_TRANSACTION_CONTRACT_SOURCE := \
	tests/compiler/appimage_transaction_contract_main.btrc
APPIMAGE_TRANSACTION_CONTRACT_SOURCES := $(shell find \
	src/generators/appimage src/lib -name '*.btrc' -print 2>/dev/null)
APPIMAGE_TRANSACTION_CONTRACT_C := build/tests/appimage-transaction-contract.c
APPIMAGE_TRANSACTION_CONTRACT_BIN := build/tests/appimage-transaction-contract

TREE_AUDIT_SOURCE := tests/core/tree_audit/main.btrc
TREE_AUDIT_SOURCES := $(shell find tests/core/tree_audit -name '*.btrc' -print 2>/dev/null)
TREE_AUDIT_C := build/tests/tree-audit.c
TREE_AUDIT_BIN := build/tests/tree-audit
STEAMDECK_HARNESS := tests/targets/steamdeck/Makefile
RETROARCH_RUNTIME_HARNESS := tests/integration/retroarch/run.sh

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

compiler-tests: gb-definition-contract $(SEMU_COMPILER_TEST_BIN) ## Run the focused strict-import compiler contracts
	$(SEMU_COMPILER_TEST_BIN) --project "$(CURDIR)"

gb-definition-contract: $(GB_DEFINITION_CONTRACT_BIN) ## Prove the complete Game Boy declaration
	$(GB_DEFINITION_CONTRACT_BIN) --project "$(CURDIR)"

$(GB_DEFINITION_CONTRACT_BIN): $(GB_DEFINITION_CONTRACT_SOURCE) \
	$(SEMU_CONFIG) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(GB_DEFINITION_CONTRACT_C))" \
		"$(dir $(GB_DEFINITION_CONTRACT_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(GB_DEFINITION_CONTRACT_SOURCE)" \
		-o "$(CURDIR)/$(GB_DEFINITION_CONTRACT_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(GB_DEFINITION_CONTRACT_C)"
	$(CC) "$(GB_DEFINITION_CONTRACT_C)" -std=c11 -o "$@" -lm

appimage-contract: appimage-trust-contract appimage-transaction-contract \
	$(SEMU_BIN) \
	$(APPIMAGE_CONTRACT_BIN) ## Run AppImage assembly/runtime-builder contracts
	$(APPIMAGE_CONTRACT_BIN) --project "$(CURDIR)" --program "$(SEMU_BIN)"

appimage-trust-contract: appimage-nix-release-proof-contract \
	appimage-frozen-builder-contract \
	appimage-runtime-builder-lifecycle-contract \
	$(APPIMAGE_TRUST_CONTRACT_BIN) ## Run bounded release-trust contracts
	$(APPIMAGE_TRUST_CONTRACT_BIN)

appimage-nix-release-proof-contract: \
	$(APPIMAGE_NIX_PROOF_CONTRACT_BIN) ## Prove store-independent release receipts
	$(APPIMAGE_NIX_PROOF_CONTRACT_BIN) \
		--program "$(CURDIR)/$(APPIMAGE_NIX_PROOF_CONTRACT_BIN)"

appimage-frozen-builder-contract: \
	$(APPIMAGE_FROZEN_BUILDER_CONTRACT_BIN) ## Prove frozen-source/builder separation
	$(APPIMAGE_FROZEN_BUILDER_CONTRACT_BIN)

appimage-runtime-builder-lifecycle-contract: \
	$(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_BIN) ## Prove private anonymous-store cleanup
	$(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_BIN)

appimage-transaction-contract: $(APPIMAGE_TRANSACTION_CONTRACT_BIN) ## Run adversarial publication/copy contracts
	$(APPIMAGE_TRANSACTION_CONTRACT_BIN)

$(APPIMAGE_TRUST_CONTRACT_BIN): $(APPIMAGE_TRUST_CONTRACT_SOURCE) \
	$(APPIMAGE_TRUST_CONTRACT_SOURCES) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(APPIMAGE_TRUST_CONTRACT_C))" "$(dir $(APPIMAGE_TRUST_CONTRACT_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(APPIMAGE_TRUST_CONTRACT_SOURCE)" \
		-o "$(CURDIR)/$(APPIMAGE_TRUST_CONTRACT_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(APPIMAGE_TRUST_CONTRACT_C)"
	$(CC) "$(APPIMAGE_TRUST_CONTRACT_C)" -std=c11 -o "$@" -lm

$(APPIMAGE_NIX_PROOF_CONTRACT_BIN): $(APPIMAGE_NIX_PROOF_CONTRACT_SOURCE) \
	$(APPIMAGE_TRUST_CONTRACT_SOURCES) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(APPIMAGE_NIX_PROOF_CONTRACT_C))" \
		"$(dir $(APPIMAGE_NIX_PROOF_CONTRACT_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(APPIMAGE_NIX_PROOF_CONTRACT_SOURCE)" \
		-o "$(CURDIR)/$(APPIMAGE_NIX_PROOF_CONTRACT_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(APPIMAGE_NIX_PROOF_CONTRACT_C)"
	$(CC) "$(APPIMAGE_NIX_PROOF_CONTRACT_C)" -std=c11 -o "$@" -lm

$(APPIMAGE_FROZEN_BUILDER_CONTRACT_BIN): \
	$(APPIMAGE_FROZEN_BUILDER_CONTRACT_SOURCE) \
	$(APPIMAGE_TRUST_CONTRACT_SOURCES) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(APPIMAGE_FROZEN_BUILDER_CONTRACT_C))" \
		"$(dir $(APPIMAGE_FROZEN_BUILDER_CONTRACT_BIN))"
	$(BTRC_TRANSPILE) \
		"$(CURDIR)/$(APPIMAGE_FROZEN_BUILDER_CONTRACT_SOURCE)" \
		-o "$(CURDIR)/$(APPIMAGE_FROZEN_BUILDER_CONTRACT_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' \
		"$(APPIMAGE_FROZEN_BUILDER_CONTRACT_C)"
	$(CC) "$(APPIMAGE_FROZEN_BUILDER_CONTRACT_C)" -std=c11 -o "$@" -lm

$(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_BIN): \
	$(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_SOURCE) \
	$(APPIMAGE_TRUST_CONTRACT_SOURCES) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_C))" \
		"$(dir $(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_BIN))"
	$(BTRC_TRANSPILE) \
		"$(CURDIR)/$(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_SOURCE)" \
		-o "$(CURDIR)/$(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' \
		"$(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_C)"
	$(CC) "$(APPIMAGE_RUNTIME_LIFECYCLE_CONTRACT_C)" -std=c11 -o "$@" -lm

$(APPIMAGE_TRANSACTION_CONTRACT_BIN): $(APPIMAGE_TRANSACTION_CONTRACT_SOURCE) \
	$(APPIMAGE_TRANSACTION_CONTRACT_SOURCES) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(APPIMAGE_TRANSACTION_CONTRACT_C))" "$(dir $(APPIMAGE_TRANSACTION_CONTRACT_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(APPIMAGE_TRANSACTION_CONTRACT_SOURCE)" \
		-o "$(CURDIR)/$(APPIMAGE_TRANSACTION_CONTRACT_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(APPIMAGE_TRANSACTION_CONTRACT_C)"
	$(CC) "$(APPIMAGE_TRANSACTION_CONTRACT_C)" -std=c11 -o "$@" -lm

$(APPIMAGE_CONTRACT_BIN): $(APPIMAGE_CONTRACT_SOURCE) $(APPIMAGE_CONTRACT_SOURCES) $(SEMU_CONFIG) flake.nix flake.lock tests/Makefile
	@mkdir -p "$(dir $(APPIMAGE_CONTRACT_C))" "$(dir $(APPIMAGE_CONTRACT_BIN))"
	$(BTRC_TRANSPILE) "$(CURDIR)/$(APPIMAGE_CONTRACT_SOURCE)" \
		-o "$(CURDIR)/$(APPIMAGE_CONTRACT_C)" \
		--strict-imports --no-cache --no-stdlib
	perl -0pi -e 's/\n+\z/\n/' "$(APPIMAGE_CONTRACT_C)"
	$(CC) "$(APPIMAGE_CONTRACT_C)" -std=c11 -o "$@" -lm

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

retroarch-runtime-contract: ## Execute the exact ABI-3 hook with threaded llvmpipe
	"$(RETROARCH_RUNTIME_HARNESS)"

steamdeck-contracts: ## Run local Steam Deck contracts without SSH or a device
	$(MAKE) -f "$(STEAMDECK_HARNESS)" test installer-contract

test: tree-audit compiler-tests appimage-contract configs-smoke steamdeck-contracts ## Run all device-free contracts

verify: $(SEMU_BIN) compiler-tests ## Verify TARGET plan (default: steam-deck)
	"$(SEMU_BIN)" verify target "$(TARGET)" --project "$(CURDIR)" \
		--output "build/test/verify/$(TARGET)"

verify-production: compiler-tests retroarch-runtime-contract appimage-verify ## Runtime contracts + exact AppImage verification
