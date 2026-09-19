#!/usr/bin/env nix-shell
#!nix-shell -i python3 -p python3Packages.numpy python3Packages.pillow python3Packages.scipy
# Pixel-exact screen-opening measurement for bezel packages: finds every display opening in a
# bezel image (dark or transparent region of at least --min-area pixels), reports its rectangle,
# corner radius, and fill in canvas pixels, and optionally writes an overlay PNG for review.
# usage: bezel-measure.py IMAGE [--mode dark|alpha|both|opaque|seed] [--threshold N] [--count N] [--overlay OUT.png] [--json]
import argparse, json, sys
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage


class Opening:
    def __init__(self, label, x, y, w, h, fill, radius):
        self.label, self.x, self.y, self.w, self.h, self.fill, self.radius = label, x, y, w, h, fill, radius

    def rect(self):
        return {"x": int(self.x), "y": int(self.y), "w": int(self.w), "h": int(self.h)}

    def normalized(self, width, height):
        return {"x": self.x / width, "y": self.y / height, "w": self.w / width, "h": self.h / height}


class Measurer:
    @classmethod
    def load(cls, path):
        image = Image.open(path).convert("RGBA")
        return np.asarray(image, dtype=np.uint8)

    @classmethod
    def mask(cls, pixels, mode, threshold, seeds, smooth):  # candidate opening pixels: near-black opaque, transparent, or either
        rgb = pixels[..., :3].astype(np.int32)
        if smooth > 1:  # textured plastics: a median filter removes the grain before the color test
            rgb = np.stack([ndimage.median_filter(rgb[..., channel], size=smooth) for channel in range(3)], axis=2)
        alpha = pixels[..., 3]
        dark = (rgb.max(axis=2) <= threshold) & (alpha > 128)
        clear = alpha <= 8
        if mode == "dark":
            return dark
        if mode == "alpha":
            return clear
        if mode == "opaque":  # glass layers: the lens is the opaque region
            return alpha > 128
        if mode == "seed":  # uniform regions around seed points, within --threshold of each seed color
            height, width = alpha.shape
            result = np.zeros(alpha.shape, dtype=bool)
            for sx, sy in seeds:
                seed = rgb[int(sy * height), int(sx * width)]
                result |= (np.abs(rgb - seed).max(axis=2) <= threshold) & (alpha > 128)
            return result
        return dark | clear

    @classmethod
    def corner_radius(cls, component, x, y, w, h):  # pixels missing from the top-left corner quadrant fit r*r*(1-pi/4)
        quadrant = component[y:y + max(1, h // 2), x:x + max(1, w // 2)]
        side = min(quadrant.shape)
        missing = 0
        for r in range(1, side):
            block = quadrant[:r, :r]
            missing = int(block.size - block.sum())
            if block[r - 1, 0] and block[0, r - 1]:
                break
        return round((missing / (1.0 - np.pi / 4.0)) ** 0.5, 1) if missing > 0 else 0.0

    @classmethod
    def openings(cls, pixels, mode, threshold, count, min_area, border, min_fill, seeds, smooth):
        height, width = pixels.shape[:2]
        mask = cls.mask(pixels, mode, threshold, seeds, smooth)
        labels, total = ndimage.label(mask)
        if mode == "seed":  # only the components under the seeds are screens
            keep = np.zeros(mask.shape, dtype=bool)
            for sx, sy in seeds:
                keep |= labels == labels[int(sy * height), int(sx * width)]
            mask = keep & (labels > 0)
            labels, total = ndimage.label(mask)
        edge = set()
        if border > 0:  # the device silhouette itself is transparent: drop components touching the canvas edge
            edge = set(np.unique(labels[0, :])) | set(np.unique(labels[-1, :])) | set(np.unique(labels[:, 0])) | set(np.unique(labels[:, -1]))
        found = []
        for index, box in enumerate(ndimage.find_objects(labels), start=1):
            if box is None or index in edge:
                continue
            ys, xs = box
            x, y, w, h = xs.start, ys.start, xs.stop - xs.start, ys.stop - ys.start
            if w * h < min_area:  # bounding box first: the pixel test below is the expensive part
                continue
            component = labels[ys, xs] == index
            area = int(component.sum())
            if area < min_area:
                continue
            fill = area / float(w * h)
            if fill < min_fill:  # not a screen: text, vents, and grilles are sparse
                continue
            found.append(Opening(f"opening{len(found)}", x, y, w, h, round(fill, 4), cls.corner_radius(component, 0, 0, w, h)))
        found.sort(key=lambda opening: opening.w * opening.h, reverse=True)
        return found[:count], width, height

    @classmethod
    def raycast(cls, pixels, threshold, seeds, smooth, edge):  # outlined screens: march from each seed until the color leaves the seed's tolerance (or, with edge, until a local step)
        height, width = pixels.shape[:2]
        rgb = pixels[..., :3].astype(np.int32)
        if smooth > 1:
            rgb = np.stack([ndimage.uniform_filter(rgb[..., channel].astype(np.float32), size=smooth) for channel in range(3)], axis=2)
        luma = rgb.mean(axis=2)
        found = []
        for sx, sy in seeds:
            cx, cy = int(sx * width), int(sy * height)
            seed = rgb[cy, cx]
            if edge:  # a drawn outline is a step in the smoothed luma; the body tone may match the screen
                step = max(threshold, 1)
                same = (np.abs(np.gradient(luma, axis=1)) < step) & (np.abs(np.gradient(luma, axis=0)) < step)
            else:
                same = np.abs(rgb - seed).max(axis=2) <= threshold
            row, column = same[cy, :], same[:, cx]
            left = cx
            while left > 0 and row[left - 1]:
                left -= 1
            right = cx
            while right < width - 1 and row[right + 1]:
                right += 1
            top = cy
            while top > 0 and column[top - 1]:
                top -= 1
            bottom = cy
            while bottom < height - 1 and column[bottom + 1]:
                bottom += 1
            w, h = right - left + 1, bottom - top + 1
            block = same[top:bottom + 1, left:right + 1]
            found.append(Opening(f"opening{len(found)}", left, top, w, h, round(float(block.mean()), 4), cls.corner_radius(block, 0, 0, w, h)))
        return found, width, height

    @classmethod
    def overlay(cls, pixels, openings, path):
        image = Image.fromarray(pixels).convert("RGBA")
        draw = ImageDraw.Draw(image)
        for opening in openings:
            draw.rectangle([opening.x, opening.y, opening.x + opening.w - 1, opening.y + opening.h - 1], outline=(255, 0, 255, 255), width=max(2, image.width // 800))
        image.save(path)

    @classmethod
    def main(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument("image")
        parser.add_argument("--mode", default="both", choices=["dark", "alpha", "both", "opaque", "seed", "raycast"])
        parser.add_argument("--min-fill", type=float, default=0.6, help="reject sparse components (text, vents, grilles)")
        parser.add_argument("--threshold", type=int, default=40)
        parser.add_argument("--count", type=int, default=2)
        parser.add_argument("--min-area", type=int, default=20000)
        parser.add_argument("--border", type=int, default=1, help="1 drops components touching the canvas edge")
        parser.add_argument("--seed", action="append", default=[], help="x,y in canvas fractions; seed mode grows one opening per seed (default: the center)")
        parser.add_argument("--smooth", type=int, default=1, help="median (box for raycast) filter size applied before the color test")
        parser.add_argument("--edge", action="store_true", help="raycast stops at a luma step of --threshold instead of a color distance from the seed")
        parser.add_argument("--overlay")
        parser.add_argument("--json", action="store_true")
        args = parser.parse_args()
        pixels = cls.load(args.image)
        seeds = [tuple(float(part) for part in seed.split(",")) for seed in args.seed] or [(0.5, 0.5)]
        if args.mode == "raycast":
            openings, width, height = cls.raycast(pixels, args.threshold, seeds, args.smooth, args.edge)
        else:
            openings, width, height = cls.openings(pixels, args.mode, args.threshold, args.count, args.min_area, args.border, args.min_fill, seeds, args.smooth)
        if args.overlay:
            cls.overlay(pixels, openings, args.overlay)
        report = {"image": args.image, "canvas": {"w": width, "h": height},
                  "openings": [{"label": o.label, "pixels": o.rect(), "normalized": o.normalized(width, height), "fill": o.fill, "corner_radius_px": o.radius, "aspect": round(o.w / o.h, 4)} for o in openings]}
        if args.json:
            print(json.dumps(report, indent=2))
        else:
            print(f"{args.image}: {width}x{height}")
            for o in openings:
                print(f"  {o.label}: x={o.x} y={o.y} w={o.w} h={o.h} aspect={o.w / o.h:.4f} fill={o.fill} corner_r={o.radius}px")
        return 0 if openings else 1


sys.exit(Measurer.main())
