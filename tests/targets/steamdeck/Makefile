# Steam Deck target tests. The physical Deck is reached over SSH; when it is
# unreachable (powered off), the suite reports an explicit skip instead of
# failing — the Bazzite VM harness (tests/targets/vm) covers the linux target
# in the meantime.

SEMU_DECK_HOST ?= deck
SEMU_DECK_SSH_OPTS ?= -o ConnectTimeout=5 -o BatchMode=yes

.PHONY: test deck-reachable

deck-reachable:
	@ssh $(SEMU_DECK_SSH_OPTS) $(SEMU_DECK_HOST) 'true' >/dev/null 2>&1

test: ## Run the Deck verification chain on the physical device (skips when offline)
	@if ssh $(SEMU_DECK_SSH_OPTS) $(SEMU_DECK_HOST) 'true' >/dev/null 2>&1; then \
		echo "Deck reachable — syncing and verifying..."; \
		$(MAKE) cross-linux; \
		git ls-files -co --exclude-standard -z | rsync -az --files-from=- --from0 \
			-e "ssh $(SEMU_DECK_SSH_OPTS)" . $(SEMU_DECK_HOST):~/semu/; \
		ssh $(SEMU_DECK_SSH_OPTS) $(SEMU_DECK_HOST) 'mkdir -p ~/semu/src/generated/build'; \
		rsync -az -e "ssh $(SEMU_DECK_SSH_OPTS)" src/generated/build/semu-linux-x64 \
			$(SEMU_DECK_HOST):~/semu/src/generated/build/semu; \
		ssh $(SEMU_DECK_SSH_OPTS) $(SEMU_DECK_HOST) 'chmod +x ~/semu/src/generated/build/semu \
			&& cd ~/semu && src/generated/build/semu test core \
			&& src/generated/build/semu bootstrap \
			&& src/generated/build/semu deck provision \
			&& src/generated/build/semu deck verify-emulators \
			&& src/generated/build/semu deck verify-sync \
			&& src/generated/build/semu deck verify-input'; \
	else \
		echo "SKIP steamdeck target tests: $(SEMU_DECK_HOST) unreachable (deck offline)"; \
	fi

# The closed visual loop: every bezel-declaring system x variant(a/b/c/off)
# x shader(on/off), launched through the REAL shim context on the Deck,
# geometry read back from the tap's own frame log, portal screenshot per
# combination into src/generated/verification/loop/. SEMU_LOOP_ALIGN=1 adds
# the tap's purple alignment outline to every capture for pixel debugging.
visual-loop:
	@ssh $(SEMU_DECK_SSH_OPTS) $(SEMU_DECK_HOST) 'bash -s' < tests/targets/steamdeck/visual_loop.sh
