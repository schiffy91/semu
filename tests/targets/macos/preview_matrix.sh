#!/usr/bin/env bash
# Contract-driven bezel preview matrix: enumerates every enabled system x
# variant from src/semu/systems/*/bezels.json (nothing hand-listed), pulls the
# staged art from the composed bundle, and composites a gradient game card
# into each hole (per-screen holes aspect-fit like the tap; single holes fill
# exactly), art alpha over BLACK exactly as the tap presents it. Python does
# the contract math; a CoreGraphics helper (materialized at runtime, one
# compile for all jobs) does the pixels. Output:
# src/generated/verification/preview-matrix/.
set -euo pipefail
PROJECT="${1:-$(pwd)}"
BUNDLE="$PROJECT/src/generated/nix/result/lib/semu/share/semu"
OUT="$PROJECT/src/generated/verification/preview-matrix"
mkdir -p "$OUT"

python3 - "$PROJECT" "$BUNDLE" "$OUT" <<'PY' > /tmp/preview-jobs.json
import json, glob, os, sys

project, bundle, out = sys.argv[1], sys.argv[2], sys.argv[3]
WIDTH, HEIGHT = 1280, 800

def aspect_fit(hole, native_w, native_h):
    hw_px, hh_px = hole["w"] * WIDTH, hole["h"] * HEIGHT
    aspect = native_w / native_h
    fit_w = hw_px; fit_h = fit_w / aspect
    if fit_h > hh_px:
        fit_h = hh_px; fit_w = fit_h * aspect
    return [round(hole["x"] * WIDTH + (hw_px - fit_w) / 2),
            round(hole["y"] * HEIGHT + (hh_px - fit_h) / 2), round(fit_w), round(fit_h)]

def fill_rect(hole):
    return [round(hole["x"] * WIDTH), round(hole["y"] * HEIGHT),
            round(hole["w"] * WIDTH), round(hole["h"] * HEIGHT)]

jobs, missing = [], []
for bezels_file in sorted(glob.glob(f"{project}/src/semu/systems/*/bezels.json")):
    system = os.path.basename(os.path.dirname(bezels_file))
    bezels = json.load(open(bezels_file))
    if not bezels.get("enabled", True):
        continue
    system_json = json.load(open(os.path.join(os.path.dirname(bezels_file), "system.json")))
    screens = system_json.get("display", {}).get("screens", [])
    for variant in bezels.get("variants", []):
        art_path = variant.get("art", "")
        art_file = os.path.join(bundle, art_path) if art_path else ""
        if not art_file or not os.path.exists(art_file):
            missing.append(f"{system}/{variant['id']}: {art_path or 'NO ART'}")
            continue
        cards = []
        screen_holes = variant.get("screen_holes") or bezels.get("screen_holes")
        if screen_holes and len(screens) > 1:
            for screen in screens:
                hole = screen_holes.get(screen["id"])
                if hole:
                    cards.append(aspect_fit(hole, screen["native"]["w"], screen["native"]["h"]))
        else:
            hole = variant.get("hole") or bezels.get("hole")
            if hole:
                cards.append(fill_rect(hole))
        jobs.append({"art": art_file, "out": f"{out}/{system}-{variant['id']}.png",
                     "cards": cards})
print(json.dumps({"width": WIDTH, "height": HEIGHT, "jobs": jobs, "missing": missing}))
PY

cat > /tmp/preview-compose.swift <<'SWIFT'
import CoreGraphics
import ImageIO
import Foundation
import UniformTypeIdentifiers

let manifest = try! JSONSerialization.jsonObject(
    with: Data(contentsOf: URL(fileURLWithPath: "/tmp/preview-jobs.json"))) as! [String: Any]
let width = manifest["width"] as! Int
let height = manifest["height"] as! Int
for job in manifest["jobs"] as! [[String: Any]] {
    let artPath = job["art"] as! String
    let outPath = job["out"] as! String
    let cards = job["cards"] as! [[Int]]
    let space = CGColorSpace(name: CGColorSpace.sRGB)!
    let context = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8,
        bytesPerRow: 0, space: space,
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    context.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: 1))
    context.fill(CGRect(x: 0, y: 0, width: width, height: height))
    if let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: artPath) as CFURL, nil),
       let art = CGImageSourceCreateImageAtIndex(source, 0, nil) {
        context.draw(art, in: CGRect(x: 0, y: 0, width: width, height: height))
    }
    for card in cards {
        let rect = CGRect(x: card[0], y: height - card[1] - card[3], width: card[2], height: card[3])
        let gradient = CGGradient(colorsSpace: space, colors: [
            CGColor(red: 0.11, green: 0.23, blue: 0.66, alpha: 1),
            CGColor(red: 0.48, green: 0.12, blue: 0.63, alpha: 1)] as CFArray,
            locations: [0, 1])!
        context.saveGState()
        context.clip(to: rect)
        context.drawLinearGradient(gradient,
            start: CGPoint(x: rect.midX, y: rect.maxY),
            end: CGPoint(x: rect.midX, y: rect.minY), options: [])
        context.restoreGState()
    }
    let image = context.makeImage()!
    let destination = CGImageDestinationCreateWithURL(
        URL(fileURLWithPath: outPath) as CFURL, UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(destination, image, nil)
    CGImageDestinationFinalize(destination)
    print("PREVIEW \(outPath)")
}
for entry in manifest["missing"] as! [String] {
    print("MISSING \(entry)")
}
SWIFT
swift /tmp/preview-compose.swift
python3 - <<'PY'
import json
manifest = json.load(open("/tmp/preview-jobs.json"))
print(f"MATRIX RESULT: {len(manifest['jobs'])} previews, {len(manifest['missing'])} missing")
raise SystemExit(1 if manifest["missing"] else 0)
PY
