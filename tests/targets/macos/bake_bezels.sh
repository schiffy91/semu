#!/usr/bin/env bash
# Bake photoreal bezel art from Duimon Mega Bezel presets.
#
# For every entry in bake_manifest.json:
#   1. render the preset with a COLOR-KEYED probe card (screen 1 magenta,
#      screen 2 cyan) and threshold-detect each screen's bounding box;
#   2. render the same preset with a BLACK card and ship that render as the
#      static bezel PNG (src/semu/assets/baked/<system>/<variant>.png);
#   3. patch the system's bezels.json variant (hole + screen_holes) and the
#      asset manifest recipe (type local) so the contract matches the art
#      by construction.
# Requires a GPU (librashader on Metal), so it runs as a mac harness and the
# baked PNGs are committed as sources; the nix build just copies them.
set -euo pipefail
PROJECT="${1:-$(pwd)}"
LIBRASHADER="${LIBRASHADER_CLI:-/tmp/librashader-root/bin/librashader-cli}"
SHADER_ROOT="${2:-$PROJECT/src/generated/nix/result/lib/semu/share/libretro/shaders}"
DUIMON="$SHADER_ROOT/Mega_Bezel_Packs/Duimon-Mega-Bezel"
WORK="$(mktemp -d /tmp/bake-bezels.XXXXXX)"
python3 - "$PROJECT" "$LIBRASHADER" "$DUIMON" "$WORK" <<'PY'
import json, os, struct, subprocess, sys, zlib

project, librashader, duimon, work = sys.argv[1:5]
manifest = json.load(open(f"{project}/tests/targets/macos/bake_manifest.json"))
viewport = manifest["viewport"]
MAGENTA, CYAN, BLACK = (255, 0, 255), (0, 255, 255), (0, 0, 0)

def write_png(path, width, height, paint):
    rows = []
    for row_y in range(height):
        row = bytearray([0])
        for col_x in range(width):
            row += bytes(paint(col_x, row_y))
        rows.append(bytes(row))
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    blob = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(blob)

def read_png_rgb(path):
    data = open(path, "rb").read()
    offset, width, height, chunks = 8, 0, 0, []
    while offset < len(data):
        length = struct.unpack(">I", data[offset:offset+4])[0]
        tag = data[offset+4:offset+8]
        if tag == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", data[offset+8:offset+18])
            assert depth == 8, f"unexpected bit depth {depth}"
            channels = {2: 3, 6: 4}[color]
        elif tag == b"IDAT":
            chunks.append(data[offset+8:offset+8+length])
        elif tag == b"IEND":
            break
        offset += 12 + length
    raw = zlib.decompress(b"".join(chunks))
    stride = width * channels
    rows, previous = [], bytearray(stride)
    position = 0
    for row_y in range(height):
        filter_type = raw[position]; position += 1
        line = bytearray(raw[position:position+stride]); position += stride
        if filter_type == 1:
            for index in range(channels, stride):
                line[index] = (line[index] + line[index-channels]) & 0xFF
        elif filter_type == 2:
            for index in range(stride):
                line[index] = (line[index] + previous[index]) & 0xFF
        elif filter_type == 3:
            for index in range(stride):
                left = line[index-channels] if index >= channels else 0
                line[index] = (line[index] + ((left + previous[index]) >> 1)) & 0xFF
        elif filter_type == 4:
            for index in range(stride):
                left = line[index-channels] if index >= channels else 0
                above = previous[index]
                upper_left = previous[index-channels] if index >= channels else 0
                estimate = left + above - upper_left
                distances = (abs(estimate-left), abs(estimate-above), abs(estimate-upper_left))
                line[index] = (line[index] + (left, above, upper_left)[distances.index(min(distances))]) & 0xFF
        rows.append(bytes(line))
        previous = line
    return width, height, channels, rows

def detect_box(rows, width, height, channels, classify):
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for row_y in range(height):
        row = rows[row_y]
        for col_x in range(width):
            base = col_x * channels
            if classify(row[base], row[base+1], row[base+2]):
                if col_x < min_x: min_x = col_x
                if col_x > max_x: max_x = col_x
                if row_y < min_y: min_y = row_y
                if row_y > max_y: max_y = row_y
    if max_x < 0:
        return None
    return (min_x, min_y, max_x - min_x + 1, max_y - min_y + 1)

def is_magenta(red, green, blue):
    return red > 120 and blue > 120 and green < red * 0.6 and green < blue * 0.6

def is_cyan(red, green, blue):
    return green > 120 and blue > 120 and red < green * 0.6

def render(preset_path, card, out, zoom, extra_params=None):
    wrapper = f"{work}/wrapper.slangp"
    lines = [f'#reference "{preset_path}"', 'HSM_INTRO_WHEN_TO_SHOW = "0.0"']
    if zoom:
        lines.append(f'HSM_VIEWPORT_ZOOM = "{zoom}"')
    for name, value in (extra_params or {}).items():
        lines.append(f'{name} = "{value}"')
    open(wrapper, "w").write("\n".join(lines) + "\n")
    subprocess.run([librashader, "render", "-p", wrapper, "--image", card,
                    "--out", out, "--runtime", "metal", "--frame", "60",
                    "-d", f'{viewport["w"]}x{viewport["h"]}'],
                   check=True, capture_output=True)

report = []
bake_filter = os.environ.get("BAKE_ONLY", "")
for bake in manifest["bakes"]:
    tag = f'{bake["system"]}-{bake["variant"]}'
    if bake_filter and bake_filter not in (bake["system"], tag):
        continue
    input_size = bake["input"]
    screens = bake["screens"]

    def probe_paint(col_x, row_y):
        for index, screen in enumerate(screens):
            rect_x, rect_y, rect_w, rect_h = screen["rect"]
            if rect_x <= col_x < rect_x + rect_w and rect_y <= row_y < rect_y + rect_h:
                return MAGENTA if index == 0 else CYAN
        return BLACK

    probe_card = f"{work}/{tag}-probe-card.png"
    black_card = f"{work}/{tag}-black-card.png"
    write_png(probe_card, input_size["w"], input_size["h"], probe_paint)
    write_png(black_card, input_size["w"], input_size["h"], lambda col_x, row_y: BLACK)

    probe_render = f"{work}/{tag}-probe.png"
    render(f'{duimon}/{bake["preset"]}', probe_card, probe_render, bake.get("zoom"), bake.get("params"))
    width, height, channels, rows = read_png_rgb(probe_render)
    boxes = [detect_box(rows, width, height, channels, is_magenta)]
    if len(screens) > 1:
        boxes.append(detect_box(rows, width, height, channels, is_cyan))
    if any(box is None for box in boxes):
        raise SystemExit(f"{tag}: probe color not found (boxes={boxes})")

    def snap_to_native(box, screen):
        # The hole contract must share the content's native aspect exactly or
        # the seam strokes separate (glow bleed also skews the raw probe by a
        # few px). Shrink the longer axis around the box center.
        native_w, native_h = screen["rect"][2], screen["rect"][3]
        box_x, box_y, box_w, box_h = box
        native_aspect = native_w / native_h
        if box_w / box_h > native_aspect:
            snapped_w = box_h * native_aspect
            return (box_x + (box_w - snapped_w) / 2, box_y, snapped_w, box_h)
        snapped_h = box_w / native_aspect
        return (box_x, box_y + (box_h - snapped_h) / 2, box_w, snapped_h)

    boxes = [snap_to_native(box, screen) for box, screen in zip(boxes, screens)]

    baked_relative = f'src/semu/assets/baked/{bake["system"]}/{bake["variant"]}.png'
    baked_absolute = f"{project}/{baked_relative}"
    os.makedirs(os.path.dirname(baked_absolute), exist_ok=True)
    render(f'{duimon}/{bake["preset"]}', black_card, baked_absolute, bake.get("zoom"), bake.get("params"))

    def fractions(box):
        return {"x": round(box[0] / width, 4), "y": round(box[1] / height, 4),
                "w": round(box[2] / width, 4), "h": round(box[3] / height, 4)}

    union_x = min(box[0] for box in boxes)
    union_y = min(box[1] for box in boxes)
    union_hole = fractions((union_x, union_y,
                            max(box[0] + box[2] for box in boxes) - union_x,
                            max(box[1] + box[3] for box in boxes) - union_y))

    system_contract_path = f'{project}/src/semu/systems/{bake["system"]}/bezels.json'
    contract = json.load(open(system_contract_path))
    variant = next(entry for entry in contract["variants"] if entry["id"] == bake["variant"])
    variant["art"] = bake["art_key"]
    variant["hole"] = union_hole
    variant.pop("glass", None)
    if len(screens) > 1:
        variant["screen_holes"] = {screens[0]["id"]: fractions(boxes[0]),
                                   screens[1]["id"]: fractions(boxes[1])}
    else:
        variant.pop("screen_holes", None)
    if contract.get("default_variant") == bake["variant"]:
        contract["hole"] = union_hole
        if len(screens) > 1:
            contract["screen_holes"] = variant["screen_holes"]
    json.dump(contract, open(system_contract_path, "w"), indent=2)
    open(system_contract_path, "a").write("\n")

    assets_path = f"{project}/src/semu/assets/bezels.json"
    assets = json.load(open(assets_path))
    assets["assets"][bake["art_key"]] = {
        "type": "local", "path": baked_relative,
        "doc": {"provenance": f'Baked from Duimon {bake["preset"].split("/")[-1]} via bake_bezels.sh (librashader, day variant, intro off).'}}
    json.dump(assets, open(assets_path, "w"), indent=2)
    open(assets_path, "a").write("\n")

    report.append({"bake": tag, "hole": union_hole,
                   "screens": [fractions(box) for box in boxes],
                   "art": baked_relative})
    print(f"BAKED {tag}: hole={union_hole} screens={len(boxes)}")

out_dir = f"{project}/src/generated/verification"
os.makedirs(out_dir, exist_ok=True)
json.dump(report, open(f"{out_dir}/bake-report.json", "w"), indent=2)
print(f"BAKE RESULT: {len(report)} bezels baked")
PY
rm -rf "$WORK"
