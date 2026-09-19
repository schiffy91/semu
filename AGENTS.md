# Semu Agent Instructions

Read `PLAN.md` first. It is the goal contract: vision, standing directives,
keep and delete lists, milestones with done criteria, verification rules, and
the live **Status** block. Reread it on every start, resume, or compaction.

Ground rules that never change:

- `~/Games/Emulation` (ROMs, BIOS, keys, firmware, saves) is read-only.
- Hand-edit only Semu-owned files under `config/`, `src/`, `tests/`,
  `packaging/`. Emulator-native files are compiled output.
- Code is BTRC against the current stdlib. `make build && make test` must pass
  before any commit.
- A milestone is done when its criterion is observed, not when code exists.
