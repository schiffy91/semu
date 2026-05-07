# Controller profiles

Per-emulator binding fragments. `core.controllers.apply(profile, emulator)`
copies the matching `<emulator>.<ext>` into the emulator's project-dir slot.

Profile dirs:
- `xbox/` — Xbox Series X|S / One layout (and any XInput-compatible pad).
- `dualsense/` — DualSense / DualShock 4 layout.
- `steamdeck/` — Steam Input templates (`steam_input_template.vdf`). Wired
  through the GUI's "Steam Deck setup" dialog, NOT through `controllers
  apply` — Steam Input templates are not per-emulator binding fragments.

Adding a new fragment:

1. Drop a file named `<emulator>.<ext>` (lowercase emulator name) into the
   profile dir.
2. Add a target path mapping in `core/controllers.py::PROFILE_TARGETS` if the
   emulator isn't already there.

The fragments are intentionally minimal — we ship sensible button layouts, not
full configs. Per-emulator config still loads everything else from the
emulator's defaults.
