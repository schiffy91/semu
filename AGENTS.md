# Semu Agent Instructions

For production work, read these files before planning or editing:

1. `docs/production-goal.md` - normative outcome and operating contract.
2. `docs/todo.md` - implementation and verification ledger.
3. `docs/acceptance-matrix.md` - physical Steam Deck completion matrix.
4. `docs/semu-compiler-refactor.md` - compiler architecture.

Keep the active Codex goal concise and reference those documents. Do not mark
the goal complete while any required TODO or physical acceptance row remains
open. A status request is informational and does not stop ongoing goal work.

The repository is source. External ROM, BIOS, key, firmware, and existing save
media are protected inputs. Treat them as read-only unless an explicit goal
requirement authorizes a bounded, non-overwriting, hash-verified copy. Never
delete or rewrite external media.

Production behavior is compiler-driven from Semu-owned JSON. BTRC uses strict
per-file imports. Emulator-native configuration is generated output. Do not add
Gamescope, X11, Wayland, swap-interposition, host-binary, Flatpak, or FUSE launch
fallbacks to production paths.

The main agent owns integration, AppImage assembly, physical Deck control,
deployment, and acceptance. Delegate only bounded independent work. Writing
agents use isolated worktrees and disjoint files; reviewers and evidence agents
remain read-only. Only one agent may control or deploy to the Deck at a time.
Systems sharing an emulator are implemented as one emulator slice and validated
sequentially.

Before accepting a checkpoint, run the focused tests plus the relevant broad
gates, review the diff, bind artifacts to a clean revision, and state which
physical evidence is still missing. Bazzite runs only after every physical Steam
Deck row passes.
