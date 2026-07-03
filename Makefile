SHELL := /bin/bash

BTRC_TRANSPILE := nix run .\#btrcpy --

SEMU_SOURCE := src/semu.btrc
SEMU_SOURCES := $(SEMU_SOURCE) $(shell find src/semu tests -name '*.btrc' -print 2>/dev/null)
SEMU_C := src/generated/build/semu.c
SEMU_BIN := src/generated/build/semu
SEMU_MANIFEST := src/generated/semu.json
NIX_RESULT := src/generated/nix/result

.PHONY: all install setup btrc-build cross-linux manifest help

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

# Static x86_64-linux CLI for immutable targets (Bazzite VM, the Deck) that
# ship no compiler: same transpiled C, zig cc as the cross toolchain.
cross-linux: $(SEMU_BIN) ## Cross-build src/generated/build/semu-linux-x64 (static musl)
	nix shell nixpkgs\#zig --command zig cc -target x86_64-linux-musl -std=c11 \
		"$(SEMU_C)" -o src/generated/build/semu-linux-x64 -lm
	@file src/generated/build/semu-linux-x64 | grep -q "ELF 64-bit" || \
		{ echo "cross-linux did not produce an x86_64 ELF"; exit 2; }
	@mkdir -p src/generated/build/steamdeck/tap
	nix shell nixpkgs\#zig --command zig cc -target x86_64-linux-gnu -O2 -std=c11 \
		src/semu/platforms/linux/quit_watch.c \
		-o src/generated/build/steamdeck/tap/semu-quit-watch

manifest: $(SEMU_BIN) ## Generate src/generated/semu.json from semu.btrc
	@mkdir -p "$(dir $(SEMU_MANIFEST))"
	$(SEMU_BIN) manifest --output "$(SEMU_MANIFEST)"

# The shippable x86_64 AppImage, assembled ON THIS MACHINE (no linux builder):
# emitted AppRun/desktop/shims + the cross CLI + the cross tap library + the
# dereferenced visualAssets tree + the contract-pinned ES-DE AppImage's usr/
# (hash verified against es_de.nix — the single pin store) + the contract
# cores from the libretro buildbot, squashed and concatenated onto the
# AppImage type2 runtime. Output: src/generated/build/Semu-x86_64.AppImage.
APPIMAGE_WORK := src/generated/build/appimage
appimage: cross-linux ## Assemble src/generated/build/Semu-x86_64.AppImage (cross, on macOS)
	$(SEMU_BIN) bootstrap --project "$$(pwd)"
	nix shell nixpkgs\#zig --command zig cc -target x86_64-linux-gnu -shared -fPIC -O2 \
		src/semu/emulators/rendering/tap/libsemutap.c \
		-o src/generated/build/steamdeck/tap/libsemutap.so -lm -ldl
	@set -euo pipefail; \
	workdir="$(APPIMAGE_WORK)"; rm -rf "$$workdir"; mkdir -p "$$workdir"; \
	esdeUrl=$$(grep -A3 'x86_64-linux = {' src/semu/emulators/es_de/es_de.nix | grep -o 'https[^"]*' | head -1); \
	esdeHash=$$(grep -A3 'x86_64-linux = {' src/semu/emulators/es_de/es_de.nix | grep -o 'sha256-[^"]*' | head -1); \
	echo "fetching es-de: $$esdeUrl"; \
	curl -sL -o "$$workdir/es-de.AppImage" "$$esdeUrl"; \
	gotHash=$$(nix hash file --sri --type sha256 "$$workdir/es-de.AppImage"); \
	[ "$$gotHash" = "$$esdeHash" ] || { echo "es-de hash mismatch: $$gotHash != $$esdeHash"; exit 2; }; \
	offset=$$(python3 -c 'import struct,sys;h=open(sys.argv[1],"rb").read(64);print(struct.unpack_from("<Q",h,0x28)[0]+struct.unpack_from("<H",h,0x3A)[0]*struct.unpack_from("<H",h,0x3C)[0])' "$$workdir/es-de.AppImage"); \
	nix shell nixpkgs\#squashfsTools --command unsquashfs -q -offset "$$offset" -d "$$workdir/esde-root" "$$workdir/es-de.AppImage"; \
	appdir="$$workdir/AppDir"; mkdir -p "$$appdir/usr/bin" "$$appdir/usr/lib/retroarch/cores" "$$appdir/lib/semu"; \
	cp src/generated/packaging/linux/AppRun "$$appdir/AppRun"; chmod 755 "$$appdir/AppRun"; \
	cp src/generated/packaging/linux/semu.desktop "$$appdir/semu.desktop"; \
	cp src/generated/packaging/linux/bin/* "$$appdir/usr/bin/" 2>/dev/null || true; \
	cp -R "$$workdir/esde-root/usr/." "$$appdir/usr/"; \
	cp src/generated/build/semu-linux-x64 "$$appdir/usr/bin/semu"; chmod 755 "$$appdir/usr/bin/semu"; \
	cp src/generated/build/steamdeck/tap/libsemutap.so "$$appdir/lib/semu/"; \
	cp src/generated/build/steamdeck/tap/semu-quit-watch "$$appdir/lib/semu/"; \
	visualAssets=$$(nix build .\#visualAssets --no-link --print-out-paths); \
	rsync -rltL "$$visualAssets/share/" "$$appdir/share/"; \
	cp -L "$$visualAssets/lib/semu/"* "$$appdir/lib/semu/" 2>/dev/null || true; \
	for core in gambatte mgba mesen snes9x genesis_plus_gx mupen64plus_next mednafen_psx desmume; do \
		curl -sL -o "$$workdir/$$core.zip" "https://buildbot.libretro.com/nightly/linux/x86_64/latest/$${core}_libretro.so.zip"; \
		unzip -oq "$$workdir/$$core.zip" -d "$$appdir/usr/lib/retroarch/cores/"; \
	done; \
	cp "$$appdir/usr/share/pixmaps/org.es_de.frontend.svg" "$$appdir/semu.svg"; \
	cp "$$appdir/usr/share/pixmaps/org.es_de.frontend.svg" "$$appdir/.DirIcon"; \
	nix shell nixpkgs\#squashfsTools --command mksquashfs "$$appdir" "$$workdir/semu.squashfs" \
		-comp zstd -Xcompression-level 15 -root-owned -noappend -quiet; \
	curl -sL -o "$$workdir/runtime-x86_64" \
		"https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64"; \
	cat "$$workdir/runtime-x86_64" "$$workdir/semu.squashfs" > src/generated/build/Semu-x86_64.AppImage; \
	chmod 755 src/generated/build/Semu-x86_64.AppImage; \
	ls -la src/generated/build/Semu-x86_64.AppImage

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
