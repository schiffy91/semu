#!/usr/bin/env bash
# Pixel-alignment evidence: reads the preview matrix's job manifest and cuts
# each game-area corner at 1:1 pixels with a buffer (default 12% of the card)
# spanning both sides of the game/bezel seam — full-resolution previews hide
# single-pixel misalignment; these crops exist for zoomed critique.
set -euo pipefail
PROJECT="${1:-$(pwd)}"
BUFFER_FRACTION="${2:-0.12}"
OUT="$PROJECT/src/generated/verification/preview-crops"
mkdir -p "$OUT"
python3 - "$OUT" "$BUFFER_FRACTION" <<'PY' > /tmp/crop-jobs.json
import json, os, sys
out, buffer_fraction = sys.argv[1], float(sys.argv[2])
manifest = json.load(open("/tmp/preview-jobs.json"))
crops = []
for job in manifest["jobs"]:
    base = os.path.splitext(os.path.basename(job["out"]))[0]
    for card_index, card in enumerate(job["cards"]):
        x, y, w, h = card
        buffer_px = max(24, round(min(w, h) * buffer_fraction))
        corners = {"tl": (x, y), "tr": (x + w, y), "bl": (x, y + h), "br": (x + w, y + h)}
        for corner, (cx, cy) in corners.items():
            suffix = f"-s{card_index}" if len(job["cards"]) > 1 else ""
            crops.append({
                "source": job["out"],
                "out": f"{out}/{base}{suffix}-{corner}.png",
                "x": max(0, cx - buffer_px), "y": max(0, cy - buffer_px),
                "w": 2 * buffer_px, "h": 2 * buffer_px})
print(json.dumps({"width": manifest["width"], "height": manifest["height"], "crops": crops}))
PY
cat > /tmp/corner-crop.swift <<'SWIFT'
import CoreGraphics
import ImageIO
import Foundation
import UniformTypeIdentifiers

let manifest = try! JSONSerialization.jsonObject(
    with: Data(contentsOf: URL(fileURLWithPath: "/tmp/crop-jobs.json"))) as! [String: Any]
let canvasWidth = manifest["width"] as! Int
let canvasHeight = manifest["height"] as! Int
for crop in manifest["crops"] as! [[String: Any]] {
    let sourcePath = crop["source"] as! String
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: sourcePath) as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else { continue }
    let scaleX = CGFloat(image.width) / CGFloat(canvasWidth)
    let scaleY = CGFloat(image.height) / CGFloat(canvasHeight)
    let rect = CGRect(x: CGFloat(crop["x"] as! Int) * scaleX,
                      y: CGFloat(crop["y"] as! Int) * scaleY,
                      width: CGFloat(crop["w"] as! Int) * scaleX,
                      height: CGFloat(crop["h"] as! Int) * scaleY)
    guard let cut = image.cropping(to: rect.intersection(
        CGRect(x: 0, y: 0, width: image.width, height: image.height))) else { continue }
    let destination = CGImageDestinationCreateWithURL(
        URL(fileURLWithPath: crop["out"] as! String) as CFURL,
        UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(destination, cut, nil)
    CGImageDestinationFinalize(destination)
}
print("crops done")
SWIFT
swift /tmp/corner-crop.swift
ls "$OUT" | wc -l
