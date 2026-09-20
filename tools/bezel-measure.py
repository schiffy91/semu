#!/usr/bin/env nix-shell
#!nix-shell -i python3 -p python3Packages.numpy python3Packages.pillow python3Packages.scipy
# Pixel-exact screen-opening measurement for bezel packages (tools/bezel_measure.py is the module).
# usage: bezel-measure.py IMAGE [--mode dark|alpha|both|opaque|seed|raycast] [--threshold N] [--count N] [--overlay OUT.png] [--json]
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bezel_measure import Measurer

sys.exit(Measurer.main())
