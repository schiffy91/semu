#!/usr/bin/env bash
# Contract-driven shader matrix: enumerates every enabled system's
# shaders.json (screen + composite + widescreen kinds, nothing hand-listed)
# and statically verifies each preset's FULL slang chain against the staged
# tree — every #reference hop, every shaderN= stage, every textureN path —
# the failure classes that actually break shaders on-device (dangling
# references, missing LUTs, bad relative paths). Emits a machine-readable
# report for the critic pass. Visual captures ride the Deck's tap loop; this
# harness proves the declarative layer end-to-end without a GPU.
set -euo pipefail
PROJECT="${1:-$(pwd)}"
SHADER_ROOT="${2:-$PROJECT/src/generated/nix/result/lib/semu/share/libretro/shaders}"
OUT="$PROJECT/src/generated/verification/shader-matrix"
mkdir -p "$OUT"
python3 - "$PROJECT" "$SHADER_ROOT" "$OUT" <<'PY'
import json, glob, os, re, sys

project, shader_root, out = sys.argv[1], sys.argv[2], sys.argv[3]

REFERENCE = re.compile(r'^#reference\s+"([^"]+)"')
STAGE = re.compile(r'^shader\d+\s*=\s*"?([^"\s]+)"?')
TEXTURE_LIST = re.compile(r'^textures\s*=\s*"?([^"\n]+?)"?\s*$')
ASSIGN = re.compile(r'^(\w+)\s*=\s*"?([^"\n]+?)"?\s*$')

def canonical_to_staged(canonical):
    # assets/shaders/<rest> -> <shader_root>/semu/<rest> (the wrapper namespace)
    return os.path.join(shader_root, "semu", canonical[len("assets/shaders/"):])

def walk_chain(path, seen, problems, depth=0):
    if depth > 16:
        problems.append(f"reference chain too deep at {path}")
        return
    real = os.path.normpath(path)
    if real in seen:
        return
    seen.add(real)
    if not os.path.exists(real):
        problems.append(f"missing file: {real[len(shader_root)+1:]}")
        return
    if not real.endswith((".slangp", ".slang", ".inc", ".h")):
        return
    base = os.path.dirname(real)
    texture_names = []
    try:
        lines = open(real, errors="replace").read().splitlines()
    except OSError as error:
        problems.append(f"unreadable: {real}: {error}")
        return
    for line in lines:
        match = REFERENCE.match(line)
        if match:
            walk_chain(os.path.join(base, match.group(1)), seen, problems, depth + 1)
            continue
        match = STAGE.match(line)
        if match:
            walk_chain(os.path.join(base, match.group(1)), seen, problems, depth + 1)
            continue
        match = TEXTURE_LIST.match(line)
        if match:
            texture_names = [name.strip() for name in match.group(1).split(";") if name.strip()]
            continue
        match = ASSIGN.match(line)
        if match and match.group(1) in texture_names:
            texture_path = os.path.join(base, match.group(2))
            if not os.path.exists(os.path.normpath(texture_path)):
                problems.append(f"missing texture {match.group(1)}: {match.group(2)} (from {real[len(shader_root)+1:]})")
    if real.endswith(".slang"):
        for line in lines:
            include = re.match(r'^#include\s+"([^"]+)"', line)
            if include:
                walk_chain(os.path.join(base, include.group(1)), seen, problems, depth + 1)

report = []
for shaders_file in sorted(glob.glob(f"{project}/src/semu/systems/*/shaders.json")):
    system = os.path.basename(os.path.dirname(shaders_file))
    contract = json.load(open(shaders_file))
    if not contract.get("enabled", True):
        report.append({"system": system, "kind": "disabled",
                       "status": "SKIP", "note": contract.get("doc", {}).get("note", "")})
        continue
    kinds = [("screen", contract.get("screen")), ("composite", contract.get("composite"))]
    widescreen = contract.get("widescreen") or {}
    kinds += [("widescreen.screen", widescreen.get("screen")),
              ("widescreen.composite", widescreen.get("composite"))]
    for kind, canonical in kinds:
        if not canonical:
            continue
        staged = canonical_to_staged(canonical)
        seen, problems = set(), []
        walk_chain(staged, seen, problems)
        chain_files = len([entry for entry in seen if os.path.exists(entry)])
        report.append({"system": system, "kind": kind, "preset": canonical,
                       "chain_files": chain_files,
                       "status": "PASS" if not problems else "FAIL",
                       "problems": problems,
                       "intent": contract.get("doc", {}).get("intent", "")})
        print(f"{'PASS' if not problems else 'FAIL'} {system} {kind}: {chain_files} files"
              + (f" :: {problems[:2]}" if problems else ""))

with open(os.path.join(out, "report.json"), "w") as handle:
    json.dump(report, handle, indent=2)
failures = [entry for entry in report if entry["status"] == "FAIL"]
checked = [entry for entry in report if entry["status"] == "PASS"]
print(f"SHADER MATRIX: {len(checked)} chains PASS, {len(failures)} FAIL, "
      f"{len([e for e in report if e['status'] == 'SKIP'])} systems delegate to the tap/vkBasalt path")
sys.exit(1 if failures else 0)
PY
