# Controller profiles

Each subdirectory is a controller profile (`xbox`, `dualsense`, `generic-xinput`,
`steamdeck`). When `core.controllers.apply(profile, emulator)` runs, it copies
the matching `<emulator>.<ext>` file into the emulator's project-dir slot.

Adding a new fragment:

1. Drop a file named `<emulator>.<ext>` (lowercase emulator name) into the
   profile dir.
2. Add a target path mapping in `core/controllers.py::PROFILE_TARGETS` if the
   emulator isn't already there.

The fragments are intentionally minimal — we ship sensible button layouts, not
full configs. Per-emulator config still loads everything else from the
emulator's defaults.
