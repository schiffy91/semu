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
import json, glob, os, subprocess, sys

project, bundle, out = sys.argv[1], sys.argv[2], sys.argv[3]
WIDTH, HEIGHT = 1280, 800

def art_size(path):
    result = subprocess.run(["sips", "-g", "pixelWidth", "-g", "pixelHeight", path],
                            capture_output=True, text=True).stdout
    values = [int(line.split(":")[1]) for line in result.splitlines() if "pixel" in line]
    return values[0], values[1]

def aspect_fit_rect(container, aspect):
    cx, cy, cw, ch = container
    fit_w = cw; fit_h = fit_w / aspect
    if fit_h > ch:
        fit_h = ch; fit_w = fit_h * aspect
    return [cx + (cw - fit_w) / 2, cy + (ch - fit_h) / 2, fit_w, fit_h]

def rounded(rect):
    return [round(value) for value in rect]

jobs, missing = [], []
for bezels_file in sorted(glob.glob(f"{project}/src/semu/systems/*/bezels.json")):
    system = os.path.basename(os.path.dirname(bezels_file))
    bezels = json.load(open(bezels_file))
    if not bezels.get("enabled", True):
        continue
    system_json = json.load(open(os.path.join(os.path.dirname(bezels_file), "system.json")))
    display = system_json.get("display", {})
    render = system_json.get("render", {})
    screens = display.get("screens", [])
    aspect_block = display.get("aspect")
    display_aspect = (aspect_block["w"] / aspect_block["h"]) if aspect_block else (
        screens[0]["native"]["w"] / screens[0]["native"]["h"] if screens else 4 / 3)
    dual = render.get("kind") == "dual" or len(screens) > 1
    handheld = render.get("style") == "handheld" or render.get("fill", False)
    for variant in bezels.get("variants", []):
        art_path = variant.get("art", "")
        art_file = os.path.join(bundle, art_path) if art_path else ""
        if not art_file or not os.path.exists(art_file):
            missing.append(f"{system}/{variant['id']}: {art_path or 'NO ART'}")
            continue
        glass_path = variant.get("glass", "")
        glass_file = os.path.join(bundle, glass_path) if glass_path else ""
        art_w, art_h = art_size(art_file)
        art_aspect = art_w / art_h
        cards = []
        hole = variant.get("hole") or bezels.get("hole") or {"x": 0, "y": 0, "w": 1, "h": 1}
        if dual:
            # tap dual mode: the art rect IS the framebuffer; per-screen holes aspect-fit
            art_rect = [0, 0, WIDTH, HEIGHT]
            screen_holes = variant.get("screen_holes") or bezels.get("screen_holes") or {}
            for screen in screens:
                screen_hole = screen_holes.get(screen["id"])
                if screen_hole:
                    container = [screen_hole["x"] * WIDTH, screen_hole["y"] * HEIGHT,
                                 screen_hole["w"] * WIDTH, screen_hole["h"] * HEIGHT]
                    cards.append(rounded(aspect_fit_rect(
                        container, screen["native"]["w"] / screen["native"]["h"])))
        elif handheld:
            # tap fill mode: art contain-fits the framebuffer, the game fills the hole
            fitted = aspect_fit_rect([0, 0, WIDTH, HEIGHT], art_aspect)
            cards.append(rounded([fitted[0] + hole["x"] * fitted[2],
                                  fitted[1] + hole["y"] * fitted[3],
                                  hole["w"] * fitted[2], hole["h"] * fitted[3]]))
            art_rect = rounded(fitted)
        else:
            # tap console mode: integer-height scale of the native canvas
            # (the tap's rule: floor(fb_h / native_h) steps), width from the
            # display aspect; the art rect is SOLVED so the hole lands on it
            native_h = screens[0]["native"]["h"] if screens else 240
            steps = max(1, HEIGHT // native_h)
            game_h = steps * native_h
            game_w = game_h * display_aspect
            game = [(WIDTH - game_w) / 2, (HEIGHT - game_h) / 2, game_w, game_h]
            art_rect_w = game[2] / hole["w"]
            art_rect_h = art_rect_w / art_aspect
            art_rect = rounded([game[0] - hole["x"] * art_rect_w,
                                game[1] - hole["y"] * art_rect_h, art_rect_w, art_rect_h])
            cards.append(rounded(game))
        jobs.append({"art": art_file, "glass": glass_file, "art_rect": art_rect,
                     "out": f"{out}/{system}-{variant['id']}.png", "cards": cards})
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
    let artRect = job["art_rect"] as! [Int]
    let artBox = CGRect(x: artRect[0], y: height - artRect[1] - artRect[3],
                        width: artRect[2], height: artRect[3])
    if let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: artPath) as CFURL, nil),
       let art = CGImageSourceCreateImageAtIndex(source, 0, nil) {
        context.draw(art, in: artBox)
    }
    let glassPath = job["glass"] as! String
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
    if !glassPath.isEmpty,
       let glassSource = CGImageSourceCreateWithURL(URL(fileURLWithPath: glassPath) as CFURL, nil),
       let glass = CGImageSourceCreateImageAtIndex(glassSource, 0, nil) {
        context.draw(glass, in: artBox)
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
