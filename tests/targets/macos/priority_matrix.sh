#!/usr/bin/env bash
# Display-priority placement matrix: for every system, compose the default
# bezel and the default (screen-chain) shader treatment in every priority
# mode, using a line-for-line python port of semu_tap_compute_geometry from
# tap_geometry.h — the projector must never drift from the runtime.
#
#   game priority  : largest integer scale that fits the screen (fill
#                    fallback when even 2x cannot fit); bezel follows the
#                    hole center and may be cut off.
#   top priority   : dual-screen only — top screen at the largest integer
#                    scale leaving ~20% of the axis; bottom aspect-fits the
#                    remainder (fractional allowed). No bezel.
#   bezel priority : game at max integer k with the UNIFORMLY scaled bezel
#                    uncut (<=1% overflow tolerated); hole contains the game.
#   fit            : non-integer — bezel contain-fits and the game aspect-
#                    fits the hole; without art the game aspect-fills.
#
# Integer invariants are asserted for the integer modes, never eyeballed.
set -euo pipefail
PROJECT="${1:-$(pwd)}"
BUNDLE="${2:-$PROJECT/src/generated/nix/result/lib/semu/share/semu}"
SHADERS="${3:-$PROJECT/src/generated/nix/result/lib/semu/share/libretro/shaders}"
LIBRASHADER="${LIBRASHADER_CLI:-/tmp/librashader-root/bin/librashader-cli}"
OUT="${SEMU_OUT:-$PROJECT/src/generated/verification/priority-matrix}"
WORK="$(mktemp -d /tmp/priority-matrix.XXXXXX)"
mkdir -p "$OUT"

python3 - "$PROJECT" "$BUNDLE" "$SHADERS" "$LIBRASHADER" "$WORK" "$OUT" <<'PYJOBS'
import glob, json, os, struct, subprocess, sys, zlib

project, bundle, shader_root, librashader, work, out_dir = sys.argv[1:7]
# Canvas defaults to the Steam Deck panel; override via SEMU_CANVAS=WxH to prove
# the runtime placement is computed from the screen size (Deck / 1080p / 4K).
CANVAS_W, CANVAS_H = 1280, 800
if os.environ.get("SEMU_CANVAS"):
    CANVAS_W, CANVAS_H = (int(v) for v in os.environ["SEMU_CANVAS"].lower().split("x"))
# Top+Corner touched-PiP width cap (fraction of width); the rendered size snaps
# to the largest integer native scale within it. Mirrors pip_expand_budget.
PIP_EXPAND_BUDGET = float(os.environ.get("SEMU_PIP_BUDGET", "0.30"))

def round_px(value):
    return int(value + 0.5)

def art_scale_for_game(art_w, art_h, hole_w, hole_h, game_w, game_h):
    # semu_tap_art_scale_for_game: uniform scale, hole contains the game
    return max(game_w / (hole_w * art_w), game_h / (hole_h * art_h))

def compute_geometry(win_w, win_h, native_w, native_h, display_aspect,
                     priority_bezel, fill_hole, has_art, art_w, art_h,
                     hole_x, hole_y, hole_w, hole_h):
    # Verbatim port of semu_tap_compute_geometry (tap_geometry.h),
    # GL-origin outputs converted to top-left.
    aspect = display_aspect if display_aspect > 0.01 else native_w / native_h
    if hole_w <= 0.001: hole_w = 1.0
    if hole_h <= 0.001: hole_h = 1.0
    has_art = bool(has_art and art_w > 0 and art_h > 0)
    use_bezel_priority = bool(priority_bezel and has_art)
    art_scale = 0.0

    if not use_bezel_priority:
        scale = win_h // native_h
        if scale < 1: scale = 1
        game_h = scale * native_h
        game_w = round_px(game_h * aspect)
        if game_w > win_w:
            scale2 = int(win_w / (native_h * aspect))
            if scale2 < 1: scale2 = 1
            scale = scale2
            game_h = scale * native_h
            game_w = round_px(game_h * aspect)
        if scale < 2 or fill_hole:
            game_h = win_h
            game_w = round_px(win_h * aspect)
            if game_w > win_w:
                game_w = win_w
                game_h = round_px(win_w / aspect)
            scale = 0
        if has_art:
            art_scale = art_scale_for_game(art_w, art_h, hole_w, hole_h, game_w, game_h)
        game_x = (win_w - game_w) // 2
        game_y = (win_h - game_h) // 2
        art_rect = [0, 0, 0, 0]
        if has_art:
            bezel_w = art_scale * art_w
            bezel_h = art_scale * art_h
            center_x = game_x + game_w * 0.5
            center_y = game_y + game_h * 0.5
            art_rect = [center_x - (hole_x + hole_w * 0.5) * bezel_w,
                        center_y - (hole_y + hole_h * 0.5) * bezel_h,
                        bezel_w, bezel_h]
        return {"game": [game_x, game_y, game_w, game_h], "scale": scale,
                "art": art_rect, "bezel_priority": False}

    scale = 0
    if not fill_hole:
        candidate = 1
        while True:
            game_h = candidate * native_h
            game_w = round_px(game_h * aspect)
            trial = art_scale_for_game(art_w, art_h, hole_w, hole_h, game_w, game_h)
            if trial * art_w <= win_w * 1.01 + 0.5 and trial * art_h <= win_h * 1.01 + 0.5:
                scale = candidate
                candidate += 1
            else:
                break
    if scale >= 1:
        game_h = scale * native_h
        game_w = round_px(game_h * aspect)
        art_scale = art_scale_for_game(art_w, art_h, hole_w, hole_h, game_w, game_h)
    else:
        # FIT (or bezel priority where not even 1x stays uncut)
        art_scale = min(win_w / art_w, win_h / art_h)
        hole_px_w = hole_w * art_w * art_scale
        hole_px_h = hole_h * art_h * art_scale
        if hole_px_w / hole_px_h > aspect:
            game_h = round_px(hole_px_h)
            game_w = round_px(game_h * aspect)
        else:
            game_w = round_px(hole_px_w)
            game_h = round_px(game_w / aspect)
    bezel_w = art_scale * art_w
    bezel_h = art_scale * art_h
    bezel_left = (win_w - bezel_w) * 0.5
    bezel_top = (win_h - bezel_h) * 0.5
    hole_left = bezel_left + hole_x * bezel_w
    hole_top = bezel_top + hole_y * bezel_h
    hole_px_w = hole_w * bezel_w
    hole_px_h = hole_h * bezel_h
    game_x = round_px(hole_left + (hole_px_w - game_w) * 0.5)
    game_y = round_px(hole_top + (hole_px_h - game_h) * 0.5)
    return {"game": [game_x, game_y, game_w, game_h], "scale": scale,
            "art": [bezel_left, bezel_top, bezel_w, bezel_h], "bezel_priority": True}

def dual_screen_rect(art, hole, native_w, native_half_h, fractional):
    # Mirror of the libsemutap.c dual hole path (top-left coordinates).
    art_x, art_y, art_w, art_h = art
    hole_left = art_x + hole["x"] * art_w
    hole_top = art_y + hole["y"] * art_h
    hole_px_w = hole["w"] * art_w
    hole_px_h = hole["h"] * art_h
    if fractional:
        # FIT: fill the hole completely. Holes are authored to each screen's
        # native aspect, so filling introduces no distortion and no letterbox.
        return [round_px(hole_left), round_px(hole_top),
                round_px(hole_px_w), round_px(hole_px_h)], 0
    k = min(int(hole_px_w / native_w), int(hole_px_h / native_half_h))
    if k < 1: k = 1
    frame_w = k * native_w
    frame_h = k * native_half_h
    return [round_px(hole_left + (hole_px_w - frame_w) * 0.5),
            round_px(hole_top + (hole_px_h - frame_h) * 0.5),
            round_px(frame_w), round_px(frame_h)], k

def write_card(path, width, height, region=None):
    source_x, source_y = (region or (0, 0))[:2]
    rows = []
    for row_y in range(height):
        row = bytearray([0])
        for col_x in range(width):
            absolute_x, absolute_y = col_x + source_x, row_y + source_y
            red = int(255 * ((absolute_x % 1024) / 1024))
            green = int(255 * ((absolute_y % 1024) / 1024))
            blue = 200
            if absolute_x % 32 == 0 or absolute_y % 32 == 0:
                red, green, blue = 255, 255, 255
            row += bytes((red, green, blue))
        rows.append(bytes(row))
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    blob = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(blob)

def render_chain(preset, card, out_path, target_w, target_h):
    subprocess.run([librashader, "render", "-p", preset, "--image", card,
                    "--out", out_path, "--runtime", "metal", "--frame", "60",
                    "-d", f"{target_w}x{target_h}"], check=True, capture_output=True)

jobs, failures = [], []
for system_dir in sorted(glob.glob(f"{project}/src/semu/systems/*/")):
    system = os.path.basename(system_dir.rstrip("/"))
    bezel_path = os.path.join(system_dir, "bezels.json")
    system_path = os.path.join(system_dir, "system.json")
    if not (os.path.exists(bezel_path) and os.path.exists(system_path)):
        continue
    bezels = json.load(open(bezel_path))
    contract = json.load(open(system_path))
    screens = contract.get("display", {}).get("screens", [])
    if not screens:
        continue
    dual = len(screens) > 1
    native_w = screens[0]["native"]["w"]
    native_half_h = screens[0]["native"]["h"]
    native_h = native_half_h * 2 if dual else native_half_h
    screen_natives = [(s["native"]["w"], s["native"]["h"]) for s in screens]
    aspect_value = contract.get("display", {}).get("aspect")
    if isinstance(aspect_value, dict):
        display_aspect = aspect_value["w"] / aspect_value["h"]
    elif isinstance(aspect_value, (int, float)) and aspect_value > 0:
        display_aspect = float(aspect_value)
    else:
        display_aspect = native_w / native_h

    variant = None
    if bezels.get("enabled", True) and bezels.get("variants"):
        wanted = bezels.get("default_variant")
        variant = next((entry for entry in bezels["variants"] if entry["id"] == wanted), bezels["variants"][0])
    art_path, art_w, art_h, hole, screen_holes = None, 0, 0, None, None
    if variant:
        art_path = os.path.join(bundle, variant["art"])
        if os.path.exists(art_path):
            info = subprocess.run(["sips", "-g", "pixelWidth", "-g", "pixelHeight", art_path],
                                  capture_output=True, text=True).stdout
            values = [int(line.split(":")[1]) for line in info.splitlines() if "pixel" in line]
            art_w, art_h = values[0], values[1]
            hole = variant.get("hole") or bezels.get("hole") or {"x": 0, "y": 0, "w": 1, "h": 1}
            screen_holes = variant.get("screen_holes") or bezels.get("screen_holes")
        else:
            art_path = None

    shader_contract_path = os.path.join(system_dir, "shaders.json")
    screen_chain = None
    if os.path.exists(shader_contract_path):
        shader_contract = json.load(open(shader_contract_path))
        if shader_contract.get("enabled", True) and shader_contract.get("screen"):
            staged = os.path.join(shader_root, "semu", shader_contract["screen"][len("assets/shaders/"):])
            if os.path.exists(staged):
                screen_chain = staged

    modes = ["game", "top", "topcorner", "topcornerexp", "bezel", "fit"] if dual else ["game", "bezel", "fit"]
    for mode in modes:
        if dual and mode in ("game", "top", "topcorner", "topcornerexp"):
            gap = 8
            if mode == "game":
                k = min((CANVAS_H - gap) // (2 * native_half_h), CANVAS_W // native_w)
                if k < 1: k = 1
                total_h = 2 * (k * native_half_h) + gap
                start = (CANVAS_H - total_h) // 2
                rects = [[(CANVAS_W - k * native_w) // 2, start + index * (k * native_half_h + gap),
                          k * native_w, k * native_half_h] for index in range(2)]
            else:
                k = min(int(CANVAS_H * 0.8) // native_half_h, CANVAS_W // native_w)
                if k < 1: k = 1
                top_w, top_h = k * native_w, k * native_half_h
                rest = CANVAS_H - top_h - gap
                bottom_h = max(rest, native_half_h // 4)
                bottom_w = round_px(bottom_h * (native_w / native_half_h))
                if bottom_w > CANVAS_W:
                    bottom_w = CANVAS_W
                    bottom_h = round_px(bottom_w * (native_half_h / native_w))
                total_h = top_h + gap + bottom_h
                start = (CANVAS_H - total_h) // 2
                rects = [[(CANVAS_W - top_w) // 2, start, top_w, top_h],
                         [(CANVAS_W - bottom_w) // 2, start + top_h + gap, bottom_w, bottom_h]]
            if mode in ("topcorner", "topcornerexp"):
                # Mirror of the libsemutap.c mode-4 block. Top aspect-fills the
                # canvas (contain), leaving a black bezel margin on one axis.
                # REST size = that margin (derived from geometry, no baked %);
                # EXPAND size = largest integer native scale within the budget.
                ta = native_w / native_half_h
                if CANVAS_W / CANVAS_H > ta:
                    top_h = CANVAS_H; top_w = round_px(CANVAS_H * ta)
                else:
                    top_w = CANVAS_W; top_h = round_px(CANVAS_W / ta)
                mx = (CANVAS_W - top_w) / 2.0   # pillarbox side-bar width
                my = (CANVAS_H - top_h) / 2.0   # letterbox top/bottom-bar height
                rest_w = mx if mx >= my else my * ta
                ke = int(PIP_EXPAND_BUDGET * CANVAS_W / native_w)
                if ke < 1: ke = 1
                exp_w = ke * native_w
                if exp_w < rest_w: exp_w = rest_w
                if rest_w < 8: rest_w = exp_w
                bw = round_px(exp_w if mode == "topcornerexp" else rest_w)
                bh = round_px(bw / ta)
                rects = [[(CANVAS_W - top_w) // 2, (CANVAS_H - top_h) // 2, top_w, top_h],
                         [CANVAS_W - bw, CANVAS_H - bh, bw, bh]]   # flush bottom-right corner
            cards = []
            for index, hole_id in enumerate(("top", "bottom")):
                rect = rects[index]
                card = f"{work}/{system}-{hole_id}-card.png"
                if not os.path.exists(card):
                    write_card(card, native_w, native_half_h, region=(0, index * native_half_h))
                content = f"{work}/{system}-{mode}-{hole_id}.png"
                if screen_chain:
                    render_chain(screen_chain, card, content, rect[2], rect[3])
                else:
                    content = card
                cards.append({"rect": rect, "image": content})
            jobs.append({"out": f"{out_dir}/{system}-{mode}.png", "art": None,
                         "art_rect": [0, 0, 0, 0], "cards": cards, "system": system,
                         "mode": mode, "scale": k, "native": [native_w, native_h]})
            print(f"JOB {system}-{mode}: scale={k} (dual, no art)")
            continue

        priority_bezel = 1 if mode in ("bezel", "fit") else 0
        fill_hole = 1 if mode == "fit" else 0
        geometry = compute_geometry(CANVAS_W, CANVAS_H, native_w, native_h, display_aspect,
                                    priority_bezel, fill_hole, art_path is not None, art_w, art_h,
                                    hole["x"] if hole else 0, hole["y"] if hole else 0,
                                    hole["w"] if hole else 1, hole["h"] if hole else 1)
        if geometry["scale"] >= 2 and geometry["game"][3] % native_h != 0:
            failures.append(f"{system}-{mode}: game_h {geometry['game'][3]} not integer x {native_h}")

        cards = []
        if dual and screen_holes:
            hole_ids = list(screen_holes.keys())
            for index, hole_id in enumerate(hole_ids[:2]):
                s_w, s_h = screen_natives[index] if index < len(screen_natives) else (native_w, native_half_h)
                rect, screen_k = dual_screen_rect(geometry["art"], screen_holes[hole_id],
                                                  s_w, s_h, mode == "fit")
                if mode == "bezel" and rect[3] % s_h != 0:
                    failures.append(f"{system}-{mode}-{hole_id}: screen_h {rect[3]} not integer")
                card = f"{work}/{system}-{hole_id}-scard.png"
                if not os.path.exists(card):
                    write_card(card, s_w, s_h, region=(0, 0))
                content = f"{work}/{system}-{mode}-{hole_id}.png"
                if screen_chain:
                    render_chain(screen_chain, card, content, rect[2], rect[3])
                else:
                    content = card
                cards.append({"rect": rect, "image": content})
        else:
            card = f"{work}/{system}-card.png"
            if not os.path.exists(card):
                write_card(card, native_w, native_h)
            content = f"{work}/{system}-{mode}.png"
            if screen_chain:
                render_chain(screen_chain, card, content, geometry["game"][2], geometry["game"][3])
            else:
                content = card
            cards.append({"rect": geometry["game"], "image": content})

        jobs.append({"out": f"{out_dir}/{system}-{mode}.png",
                     "art": art_path, "art_rect": [round_px(v) for v in geometry["art"]],
                     "cards": cards, "system": system, "mode": mode,
                     "scale": geometry["scale"], "native": [native_w, native_h]})
        print(f"JOB {system}-{mode}: scale={geometry['scale']} game={geometry['game']}")

if failures:
    for failure in failures:
        print("INTEGER VIOLATION:", failure)
    sys.exit(1)
json.dump({"width": CANVAS_W, "height": CANVAS_H, "jobs": jobs},
          open("/tmp/priority-jobs.json", "w"), indent=1)
print(f"{len(jobs)} jobs, integer invariants hold")
PYJOBS

SWIFT="$WORK/compose.swift"
cat > "$SWIFT" <<'SWIFTEOF'
import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

let doc = try! JSONSerialization.jsonObject(
    with: Data(contentsOf: URL(fileURLWithPath: "/tmp/priority-jobs.json"))) as! [String: Any]
let width = doc["width"] as! Int
let height = doc["height"] as! Int

func loadImage(_ path: String) -> CGImage? {
    guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil) else { return nil }
    return CGImageSourceCreateImageAtIndex(source, 0, nil)
}

for job in doc["jobs"] as! [[String: Any]] {
    let context = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8,
                            bytesPerRow: 0, space: CGColorSpace(name: CGColorSpace.sRGB)!,
                            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    context.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: 1))
    context.fill(CGRect(x: 0, y: 0, width: width, height: height))
    if let artPath = job["art"] as? String, let art = loadImage(artPath) {
        let rect = job["art_rect"] as! [Int]
        if rect[2] > 0 {
            context.interpolationQuality = .high
            context.draw(art, in: CGRect(x: rect[0], y: height - rect[1] - rect[3],
                                         width: rect[2], height: rect[3]))
        }
    }
    for card in job["cards"] as! [[String: Any]] {
        let rect = card["rect"] as! [Int]
        guard let image = loadImage(card["image"] as! String) else { continue }
        context.interpolationQuality = .none
        context.draw(image, in: CGRect(x: rect[0], y: height - rect[1] - rect[3],
                                       width: rect[2], height: rect[3]))
    }
    let output = context.makeImage()!
    let url = URL(fileURLWithPath: job["out"] as! String)
    let destination = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(destination, output, nil)
    CGImageDestinationFinalize(destination)
    print("PRIORITY \(url.path)")
}
SWIFTEOF
swift "$SWIFT"
rm -rf "$WORK"
echo "PRIORITY MATRIX done -> $OUT"
