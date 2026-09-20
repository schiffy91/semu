# Fast stand-in for the Semu compositor: the same geometry (plate contain, measured tubes, fit
# policies, computed dual layouts, drawn frames, shape masks, surround, glass, vignette) from
# the same SEMU_RENDER_* environment the launcher emits, drawn in seconds with numpy and Pillow.
# Shader chains are the one thing it does not run: the test card is shown raw.
import math
import numpy as np
from PIL import Image

FIXED, MAIN_RIGHT, MAIN_LEFT, SIDE_BY_SIDE, STACKED = "fixed", "main_right", "main_left", "side_by_side", "stacked"


class Lane:
    def __init__(self, native_w, native_h):
        self.native_w, self.native_h = native_w, native_h
        self.out = (0, 0, 0, 0)
        self.tube = (0, 0, 0, 0)


class Screen:  # one SEMU_RENDER_SCREEN_<n>* group
    def __init__(self, env, index, suffix=""):
        key = lambda name: env.get(f"SEMU_RENDER_SCREEN_{index}{name}{suffix}")
        tube = key("")
        self.tube = tuple(float(v) for v in tube.split(",")) if tube else None
        look = key("_LOOK")
        values = [float(v) for v in look.split(",")] if look else [0, 0, 2, 0, 0, 0, 0, 0, 0, 0]
        self.shape, self.radius, self.exponent, self.fit, self.inset = int(values[0]), values[1], max(values[2], 2.0), int(values[3]), min(max(values[4], 0.0), 1.0) * 0.45
        self.curvature, self.vignette, self.bloom, self.glow, self.reflect = values[5:10]
        self.look_set = look is not None
        surround = key("_SURROUND")
        self.surround = tuple(float(v) for v in surround.split(",")) if surround else (0.0, 0.0, 0.0)
        self.glass = key("_GLASS")
        self.shader = key("_SHADER")


class Package:  # one variant's environment (suffix "" is the primary, "_B" the widescreen alternate)
    def __init__(self, env, suffix=""):
        self.art = env.get(f"SEMU_RENDER_ART{suffix}")
        self.background = env.get(f"SEMU_RENDER_BACKGROUND{suffix}")
        self.layout = env.get(f"SEMU_RENDER_LAYOUT{suffix}", FIXED)
        frame = env.get(f"SEMU_RENDER_FRAME{suffix}")
        values = [float(v) for v in frame.split(",")] if frame else None
        self.frame = {"width": values[0], "radius": values[1], "color": tuple(values[2:5]), "around_game": len(values) > 5 and values[5] > 0.5} if values and values[0] > 0 else None
        self.screens = [Screen(env, 0, suffix), Screen(env, 1, suffix)]
        self.bezels = env.get("SEMU_RENDER_BEZELS", "0") == "1"


class Geometry:
    @classmethod
    def contain(cls, area_w, area_h, w, h):
        scale = min(area_w / w, area_h / h)
        cw, ch = scale * w, scale * h
        return ((area_w - cw) / 2, (area_h - ch) / 2, cw, ch)

    @classmethod
    def fit(cls, area, native_w, native_h, aspect, integer):
        ax, ay, aw, ah = area
        aspect = aspect if aspect and aspect > 0.01 else native_w / native_h
        h = ah
        w = round(h * aspect)
        if w > aw:
            w = aw
            h = round(w / aspect)
        if integer:
            iw = round(native_h * aspect)
            scale = min(aw // iw, ah // native_h)
            if scale > 0:
                w, h = iw * scale, native_h * scale
        return (ax + (aw - w) // 2, ay + (ah - h) // 2, int(w), int(h))

    @classmethod
    def place_in_tube(cls, lane, screen, tube, aspect, integer):
        lane.tube = tube
        tx, ty, tw, th = tube
        inset = int(screen.inset * min(tw, th) + 0.5)
        area = (tx + inset, ty + inset, tw - 2 * inset, th - 2 * inset)
        if area[2] < 1 or area[3] < 1:
            area = tube
        if screen.fit == 1:
            lane.out = area
            return
        lane.out = cls.fit(area, lane.native_w, lane.native_h, aspect, screen.fit == 2 or integer)

    @classmethod
    def exact(cls, lane, x, y, w, h):
        lane.out = (int(x), int(y), int(w), int(h))
        lane.tube = lane.out

    @classmethod
    def main_beside(cls, lanes, area_w, area_h, frame, gap, right):
        main, second = lanes
        tallest = (area_h - 2 * frame) // main.native_h
        centered_scale, centered_side = tallest, 0
        while centered_scale >= 1:
            centered_side = (area_w - centered_scale * main.native_w) // 2 - 3 * frame - 2 * gap
            if centered_side >= second.native_w:
                break
            centered_scale -= 1
        pair_scale, pair_side = tallest, 0
        while pair_scale >= 1:
            pair_side = area_w - pair_scale * main.native_w - 4 * frame - 3 * gap
            if pair_side >= second.native_w:
                break
            pair_scale -= 1
        centered = centered_scale >= 1 and centered_scale >= pair_scale
        scale, side = (centered_scale, centered_side) if centered else (pair_scale, pair_side)
        if scale < 1:
            return False
        second_scale = min(side / second.native_w, (scale * main.native_h) / second.native_h)
        second_w, second_h = int(second.native_w * second_scale + 0.5), int(second.native_h * second_scale + 0.5)
        main_x = (area_w - scale * main.native_w) // 2
        main_y = (area_h - scale * main.native_h) // 2
        if not centered:
            pair = scale * main.native_w + 2 * frame + gap + second_w
            main_x = (area_w - pair) // 2 if right else (area_w + pair) // 2 - scale * main.native_w
        cls.exact(main, main_x, main_y, scale * main.native_w, scale * main.native_h)
        second_x = main_x + scale * main.native_w + 2 * frame + gap if right else main_x - 2 * frame - gap - second_w
        cls.exact(second, second_x, main_y + (scale * main.native_h - second_h) // 2, second_w, second_h)
        return True

    @classmethod
    def side_by_side(cls, lanes, area_w, area_h, frame, gap, integer):
        main, second = lanes
        scale = min((area_w - 4 * frame - gap) / (main.native_w + second.native_w), (area_h - 2 * frame) / max(main.native_h, second.native_h))
        if integer and scale >= 1:
            scale = float(int(scale))
        scale = max(scale, 0.01)
        mw, mh = int(main.native_w * scale + 0.5), int(main.native_h * scale + 0.5)
        sw, sh = int(second.native_w * scale + 0.5), int(second.native_h * scale + 0.5)
        x = (area_w - (mw + sw + 2 * frame + gap)) // 2
        cls.exact(main, x, (area_h - mh) // 2, mw, mh)
        cls.exact(second, x + mw + 2 * frame + gap, (area_h - sh) // 2, sw, sh)

    @classmethod
    def stacked(cls, lanes, area_w, area_h, frame, gap, integer):
        main, second = lanes
        scale = min((area_w - 2 * frame) / max(main.native_w, second.native_w), (area_h - 4 * frame - gap) / (main.native_h + second.native_h))
        if integer and scale >= 1:
            scale = float(int(scale))
        scale = max(scale, 0.01)
        mw, mh = int(main.native_w * scale + 0.5), int(main.native_h * scale + 0.5)
        sw, sh = int(second.native_w * scale + 0.5), int(second.native_h * scale + 0.5)
        y = (area_h - (mh + sh + 2 * frame + gap)) // 2  # top-left space: main above second
        cls.exact(main, (area_w - mw) // 2, y, mw, mh)
        cls.exact(second, (area_w - sw) // 2, y + mh + 2 * frame + gap, sw, sh)

    @classmethod
    def computed(cls, lanes, layout, area_w, area_h, frame, integer):
        gap = area_w // 64
        if layout in (MAIN_RIGHT, MAIN_LEFT):
            if cls.main_beside(lanes, area_w, area_h, frame, gap, layout == MAIN_RIGHT):
                return
            cls.side_by_side(lanes, area_w, area_h, frame, gap, integer)
        elif layout == SIDE_BY_SIDE:
            cls.side_by_side(lanes, area_w, area_h, frame, gap, integer)
        else:
            cls.stacked(lanes, area_w, area_h, frame, gap, integer)


class FakeCompositor:
    def __init__(self, env, width, height, wide=False):
        self.env, self.width, self.height = env, width, height
        self.package = Package(env, "_B" if wide and env.get("SEMU_RENDER_ART_B") else "")
        count = int(env.get("SEMU_RENDER_SURFACE_COUNT", "1"))
        self.lanes = []
        for index in range(count):
            w, h = env.get(f"SEMU_RENDER_SURFACE_{index}_NATIVE", "256x240").split("x")
            self.lanes.append(Lane(int(w), int(h)))
        aspect = env.get("SEMU_RENDER_ASPECT")
        self.aspect = (lambda a, b: a / b)(*(float(v) for v in aspect.split(":"))) if aspect and ":" in aspect else None
        self.integer = env.get("SEMU_RENDER_INTEGER_SCALE", "1") == "1"
        self.art = Image.open(self.package.art).convert("RGBA") if self.package.bezels and self.package.art else None
        self.background = Image.open(self.package.background).convert("RGB") if self.package.bezels and self.package.background else None
        self.canvas = None
        self.frame_px = 0
        self.resolve()

    def resolve(self):
        p, W, H = self.package, self.width, self.height
        bezel = p.bezels
        fixed = p.layout == FIXED
        if bezel and fixed and (self.art is None or any(p.screens[i].tube is None for i in range(len(self.lanes)))):
            bezel = False
        if bezel and p.frame:
            self.frame_px = int(p.frame["width"] * H + 0.5)
        layout_frame = 0 if (p.frame and p.frame["around_game"]) else self.frame_px
        if bezel and fixed:
            self.canvas = Geometry.contain(W, H, self.art.width, self.art.height)
            cx, cy, cw, ch = self.canvas
            for index, lane in enumerate(self.lanes):
                nx, ny, nw, nh = p.screens[index].tube
                tube = (round(cx + nx * cw), round(cy + ny * ch), round(nw * cw), round(nh * ch))
                Geometry.place_in_tube(lane, p.screens[index], tube, self.aspect if len(self.lanes) == 1 else None, False)
            return
        if len(self.lanes) == 1:
            margin = layout_frame if bezel and p.frame else 0
            self.lanes[0].out = Geometry.fit((margin, margin, W - 2 * margin, H - 2 * margin), self.lanes[0].native_w, self.lanes[0].native_h, self.aspect, self.integer)
            self.lanes[0].tube = self.lanes[0].out
            return
        layout = p.layout if bezel else (STACKED if p.layout == FIXED else p.layout)
        Geometry.computed(self.lanes, STACKED if layout == FIXED else layout, W, H, layout_frame if bezel else 0, self.integer)

    @classmethod
    def card(cls, screen, w, h):  # the synthetic core's test card
        sky, ground = ((48, 96, 224), (48, 200, 64)) if screen == 0 else ((200, 72, 48), (232, 176, 40))
        img = np.zeros((h, w, 3), dtype=np.uint8)
        img[: h * 2 // 3] = sky
        img[h * 2 // 3:] = ground
        cell = 16 if w >= 320 else 8
        ys, xs = np.mgrid[0:h, 0:w]
        corner = ((xs < cell * 4) | (xs >= w - cell * 4)) & ((ys < cell * 4) | (ys >= h - cell * 4))
        checker = ((xs // cell + ys // cell) % 2 == 0)
        img[corner & checker] = (255, 255, 255)
        img[corner & ~checker] = (16, 16, 16)
        img[(xs == w // 2) | (ys == h // 2)] = (255, 255, 255)
        img[(xs < 2) | (ys < 2) | (xs >= w - 2) | (ys >= h - 2)] = (255, 255, 255)
        img[h // 2 - 12:h // 2 + 12, 60:92] = (240, 224, 24)
        return img

    @classmethod
    def shape_mask(cls, xs, ys, rect, shape, radius, exponent):
        x, y, w, h = rect
        if w <= 0 or h <= 0:
            return np.zeros(xs.shape, dtype=np.float32)
        if shape == 0:
            return ((xs >= x) & (xs < x + w) & (ys >= y) & (ys < y + h)).astype(np.float32)
        rad = min(max(radius, 0.0), 0.5) * min(w, h)
        hx, hy = w / 2, h / 2
        dx = np.maximum(np.abs(xs - (x + hx)) - (hx - rad), 0.0)
        dy = np.maximum(np.abs(ys - (y + hy)) - (hy - rad), 0.0)
        n = max(exponent, 2.0) if shape == 2 else 2.0
        sd = np.power(np.power(dx, n) + np.power(dy, n), 1.0 / n) - rad
        return np.clip(1.0 - (sd + 0.75) / 1.5, 0.0, 1.0).astype(np.float32)

    def cover(self, image):
        scale = max(self.width / image.width, self.height / image.height)
        w, h = int(image.width * scale + 0.5), int(image.height * scale + 0.5)
        plate = image.resize((w, h), Image.LANCZOS)
        return plate.crop(((w - self.width) // 2, (h - self.height) // 2, (w - self.width) // 2 + self.width, (h - self.height) // 2 + self.height))

    def render(self):
        W, H = self.width, self.height
        p = self.package
        bezel = self.canvas is not None or (p.bezels and p.layout != FIXED)
        base = np.zeros((H, W, 3), dtype=np.float32)
        if bezel and self.background is not None:
            base[:] = np.asarray(self.cover(self.background), dtype=np.float32) / 255.0
        ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
        tube_masks = []
        for index, lane in enumerate(self.lanes):
            screen = p.screens[index]
            look = bezel and screen.look_set
            tube_masks.append(self.shape_mask(xs, ys, lane.tube, screen.shape if look else 0, screen.radius if look else 0, screen.exponent))
        if self.canvas is not None:  # art plate, contain-fit
            cx, cy, cw, ch = self.canvas
            plate = self.art.resize((int(round(cw)), int(round(ch))), Image.LANCZOS)
            layer = np.zeros((H, W, 4), dtype=np.float32)
            px, py = int(round(cx)), int(round(cy))
            sub = np.asarray(plate, dtype=np.float32) / 255.0
            layer[py:py + sub.shape[0], px:px + sub.shape[1]] = sub[: H - py, : W - px]
            alpha = layer[..., 3:4] * (1.0 - np.maximum.reduce(tube_masks)[..., None])
            base = base * (1 - alpha) + layer[..., :3] * alpha
        if bezel and p.frame and self.frame_px > 0 and not p.frame["around_game"]:  # drawn frame rings around the openings
            for index, lane in enumerate(self.lanes):
                screen = p.screens[index]
                tx, ty, tw, th = lane.tube
                fw = self.frame_px
                outer = (tx - fw, ty - fw, tw + 2 * fw, th + 2 * fw)
                mo = self.shape_mask(xs, ys, outer, screen.shape if screen.shape else 1, p.frame["radius"], screen.exponent)
                ring = mo * (1.0 - tube_masks[index])
                ex = np.abs(xs - (outer[0] + outer[2] / 2)) / (outer[2] / 2)
                ey = np.abs(ys - (outer[1] + outer[3] / 2)) / (outer[3] / 2)
                edge = np.maximum(ex, ey)
                shade = 0.7 + 0.4 * np.clip((edge - 0.82) / 0.18, 0, 1) ** 2
                bevel = np.clip((edge - 0.985) / 0.015, 0, 1) * 0.22
                color = np.array(p.frame["color"], dtype=np.float32)[None, None, :] * shade[..., None] + bevel[..., None]
                base = base * (1 - ring[..., None]) + color * ring[..., None]
        for index, lane in enumerate(self.lanes):  # tubes: surround, card, vignette, glass
            screen = p.screens[index]
            look = bezel and screen.look_set
            tx, ty, tw, th = lane.tube
            if tw <= 0 or th <= 0:
                continue
            ox, oy, ow, oh = lane.out
            region = np.zeros((th, tw, 3), dtype=np.float32)
            region[:] = np.array(screen.surround if look else (0, 0, 0), dtype=np.float32)
            card = Image.fromarray(self.card(index, lane.native_w, lane.native_h)).resize((max(ow, 1), max(oh, 1)), Image.NEAREST if ow >= lane.native_w else Image.BILINEAR)
            card = np.asarray(card, dtype=np.float32) / 255.0
            x0, y0 = ox - tx, oy - ty
            region[max(y0, 0):max(y0, 0) + card.shape[0], max(x0, 0):max(x0, 0) + card.shape[1]] = card[: th - max(y0, 0), : tw - max(x0, 0)]
            if bezel and p.frame and p.frame["around_game"] and self.frame_px > 0:  # inset bezel ring around the fitted game
                fw = self.frame_px
                ly, lx = np.mgrid[0:th, 0:tw].astype(np.float32)
                outer = (x0 - fw, y0 - fw, ow + 2 * fw, oh + 2 * fw)
                ring = self.shape_mask(lx, ly, outer, 1, p.frame["radius"], 2.0) * (1.0 - self.shape_mask(lx, ly, (x0, y0, ow, oh), 1, p.frame["radius"], 2.0))
                ex = np.abs(lx - (outer[0] + outer[2] / 2)) / (outer[2] / 2)
                ey = np.abs(ly - (outer[1] + outer[3] / 2)) / (outer[3] / 2)
                edge = np.maximum(ex, ey)
                shade = 0.7 + 0.4 * np.clip((edge - 0.82) / 0.18, 0, 1) ** 2
                bevel = np.clip((edge - 0.985) / 0.015, 0, 1) * 0.22
                color = np.array(p.frame["color"], dtype=np.float32)[None, None, :] * shade[..., None] + bevel[..., None]
                region = region * (1 - ring[..., None]) + color * ring[..., None]
            if look and screen.vignette > 0:
                ly, lx = np.mgrid[0:th, 0:tw].astype(np.float32)
                cc = ((lx / tw) * 2 - 1) ** 2 + ((ly / th) * 2 - 1) ** 2
                region *= (1.0 - screen.vignette * np.clip((cc - 0.45) / 1.25, 0, 1) ** 2)[..., None]
            if look and screen.glass and screen.reflect > 0:
                glass = np.asarray(Image.open(screen.glass).convert("RGBA").resize((tw, th), Image.LANCZOS), dtype=np.float32) / 255.0
                region = 1.0 - (1.0 - region) * (1.0 - glass[..., :3] * glass[..., 3:4] * min(screen.reflect, 1.0))
            mask = tube_masks[index][ty:ty + th, tx:tx + tw, None]
            patch = base[ty:ty + th, tx:tx + tw]
            base[ty:ty + th, tx:tx + tw] = patch * (1 - mask) + region[: patch.shape[0], : patch.shape[1]] * mask
        return Image.fromarray(np.clip(base * 255.0, 0, 255).astype(np.uint8))
