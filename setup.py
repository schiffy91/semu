#!/usr/bin/env python
"""Schemulator CLI entry point.

The implementation lives in `core/` so the GUI can reuse it. This file is the
argparse shim plus a few re-exports so the existing test suite keeps working.
"""

import os
import sys

# Make sure `core` resolves when this file is run directly from the repo root
# (and from `nix run`, which does the same thing).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.backup import (  # noqa: F401  (re-exported for tests)
    cmd_backup,
    cmd_capture,
    cmd_migrate,
    cmd_originals,
    cmd_revert,
)
from core.cli import main
from core.state import PLATFORM  # noqa: F401  (re-exported for tests)
from core.symlinks import (  # noqa: F401  (re-exported for tests)
    create_symlink,
    create_symlinks,
    parse_config,
    resolve_config as _resolve_config,
)


if __name__ == "__main__":
    main()
