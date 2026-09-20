#!/usr/bin/env nix-shell
#!nix-shell -i python3 -p python3Packages.numpy python3Packages.pillow python3Packages.scipy xorg.xorgserver xorg.xwd imagemagick
# Measures where each upstream Mega Bezel preset (Duimon, Soqueroeu) puts the picture inside its
# plate, and writes that rectangle into the Semu bezel package as the screen's "image". The
# preset itself is rendered by real RetroArch with the synthetic core in flat-color mode on a
# private Xvfb, so the placement math is the upstream author's, not a reimplementation. Device
# plates with transparent surroundings are rendered over a flat orange background so the plate's
# own bounds can be found and the picture mapped into plate pixels.
# usage: bezel-calibrate.py [--packages id,id] [--out DIR] [--semu PATH] [--display 88] [--keep]
import argparse, json, os, shutil, subprocess, sys, time
import numpy as np
from PIL import Image
from scipy import ndimage

VIEWPORT = (3840, 2160)
DUIMON = "duimon_mega_bezel"
SOQUEROEU = "soqueroeu_tv"


class Calibrator:
    def __init__(self, args):
        self.args = args
        self.repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.out = os.path.abspath(args.out)
        os.makedirs(self.out, exist_ok=True)
        self.semu = args.semu or shutil.which("semu") or "/run/current-system/sw/bin/semu"
        installed = os.path.dirname(os.path.realpath(shutil.which("semu") or "/run/current-system/sw/bin/semu"))
        self.retroarch = os.path.join(installed, "retroarch")
        self.bundle = os.path.dirname(installed)
        os.environ.setdefault("SEMU_ASSET_ROOT", self.bundle)
        self.core = args.core or self.build_core()
        self.upstreams = self.fetch_upstreams()
        self.stage()

    def build_core(self):
        path = subprocess.run(["nix", "build", f"{self.repo}#checks.x86_64-linux.synthetic-core", "--no-link", "--print-out-paths"], capture_output=True, text=True, check=True).stdout.strip()
        return os.path.join(path, "lib/retroarch/cores/synthetic_libretro.so")

    def fetch_upstreams(self):  # the pinned trees from config/assets/bezels.json, fetched into the store
        manifest = json.load(open(os.path.join(self.repo, "config/assets/bezels.json")))
        trees = {}
        for name, spec in manifest["upstreams"].items():
            expression = f'(import <nixpkgs> {{}}).fetchFromGitHub {{ owner = "{spec["owner"]}"; repo = "{spec["repo"]}"; rev = "{spec["rev"]}"; hash = "{spec["nar_hash"]}"; }}'
            trees[name] = subprocess.run(["nix-build", "-E", expression, "--no-out-link"], capture_output=True, text=True, check=True).stdout.strip()
        return trees

    def stage(self):  # the presets reference ../../../../shaders_slang next to their pack directory
        root = os.path.join(self.out, "shaders")
        packs = os.path.join(root, "Mega_Bezel_Packs")
        os.makedirs(os.path.join(packs, "semu-calib"), exist_ok=True)
        links = {os.path.join(root, "shaders_slang"): os.path.join(self.bundle, "share/libretro/shaders/shaders_slang"),
                 os.path.join(packs, "Duimon-Mega-Bezel"): self.upstreams[DUIMON], os.path.join(packs, "Soqueroeu-TV-Backgrounds_V2.0"): self.upstreams[SOQUEROEU]}
        for link, target in links.items():
            if os.path.islink(link):
                os.unlink(link)
            os.symlink(target, link)
        self.packs = packs
        self.orange = os.path.join(self.out, "orange.png")
        Image.new("RGB", (4096, 4096), (255, 128, 0)).save(self.orange)  # large, so the floor never tiles with seams

    def packages(self):  # (package id, package, system id) for every package that names an upstream preset
        systems = os.path.join(self.repo, "config/systems")
        wanted = self.args.packages.split(",") if self.args.packages else None
        seen = {}
        for system in sorted(os.listdir(systems)):
            manifest = os.path.join(systems, system, "bezels.json")
            if not os.path.exists(manifest):
                continue
            for variant in json.load(open(manifest)).get("variants", []):
                bezel = variant.get("bezel")
                path = os.path.join(self.repo, "config/bezels", bezel or "", "bezel.json")
                if not bezel or bezel in seen or not os.path.exists(path):
                    continue
                package = json.load(open(path))
                if package.get("upstream") and (not wanted or bezel in wanted):
                    seen[bezel] = (package, system, path)
        return seen

    def render_env(self, system):
        unwrapped = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(self.semu))), "lib/semu/semu-btrc")
        environment = dict(os.environ)
        executable = self.semu
        if self.args.semu and os.path.exists(unwrapped):
            executable = unwrapped
            environment["SEMU_SOURCE_ROOT"] = os.path.join(self.repo, "config")
        run = subprocess.run([executable, "render-env", "--system", system, "--emulator", "retroarch"], capture_output=True, text=True, env=environment)
        if run.returncode != 0:
            raise RuntimeError(f"render-env failed for {system}: {run.stderr.strip()}")
        return dict(line.split("=", 1) for line in run.stdout.splitlines() if "=" in line)

    def wrapper(self, package_id, upstream):  # the upstream preset, flat and quiet, over an orange floor for device plates
        pack = "Duimon-Mega-Bezel" if upstream["preset"].startswith("Presets/") else "Soqueroeu-TV-Backgrounds_V2.0"
        lines = [f'#reference "../{pack}/{upstream["preset"]}"', 'HSM_CURVATURE_MODE = "0"', 'HSM_INTRO_WHEN_TO_SHOW = "0"', 'HSM_AMBIENT_LIGHTING_OPACITY = "0"']
        if upstream["kind"] == "device":
            lines += [f'BackgroundImage = "{self.orange}"', 'HSM_BG_FILL_MODE = "2"', f'HSM_VIEWPORT_ZOOM = "{upstream.get("zoom", 40)}"']
        path = os.path.join(self.packs, "semu-calib", f"{package_id}.slangp")
        open(path, "w").write("\n".join(lines) + "\n")
        return path

    def capture(self, package_id, preset, env, lit):  # a launch occasionally never maps its window: retry on a fresh display
        for attempt in range(3):
            output = self.capture_once(package_id, preset, env, lit, self.args.display + attempt)
            if np.asarray(Image.open(output).convert("L")).mean() > 2.0:
                return output
            print(f"  retrying {package_id} lit={lit} (blank frame)", flush=True)
        raise RuntimeError(f"{package_id}: the upstream preset never rendered a frame")

    def capture_once(self, package_id, preset, env, lit, display_number):
        width, height = VIEWPORT
        config = os.path.join(self.out, f"{package_id}.cfg")
        open(config, "w").write("\n".join([
            'video_driver = "glcore"', 'video_fullscreen = "true"', f'video_fullscreen_x = "{width}"', f'video_fullscreen_y = "{height}"',
            'video_windowed_fullscreen = "false"', 'video_vsync = "false"', 'audio_driver = "null"', 'input_driver = "x"', 'input_joypad_driver = "null"',
            'menu_driver = "rgui"', 'video_font_enable = "false"', 'video_shader_enable = "true"', 'pause_nonactive = "false"',
            'aspect_ratio_index = "24"',  # Full: the whole screen is the viewport, so the plate maps 1:1; the screen aspect comes from the preset and the core
            f'video_shader_dir = "{os.path.join(self.out, "shaders")}"', 'video_window_save_positions = "false"']) + "\n")
        display = f":{display_number}"
        xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", f"{width}x{height}x24"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        output = os.path.join(self.out, f"{package_id}-lit{lit}.png")
        try:
            time.sleep(1.0)
            environment = {key: value for key, value in os.environ.items() if not key.startswith("SEMU_RENDER_")}
            keep = ("SEMU_RENDER_SURFACE_COUNT", "SEMU_RENDER_SURFACE_0_NATIVE", "SEMU_RENDER_SURFACE_1_NATIVE", "SEMU_RENDER_ASPECT")
            environment.update({key: value for key, value in env.items() if key in keep})  # geometry for the card only: a complete surface contract would wake the Semu hook inside RetroArch
            environment.update({"DISPLAY": display, "WAYLAND_DISPLAY": "", "SDL_VIDEODRIVER": "x11", "SEMU_SYNTHETIC_FLAT": str(lit)})
            log = open(os.path.join(self.out, f"{package_id}.log"), "w")
            emulator = subprocess.Popen([self.retroarch, "-v", "--config", config, "--set-shader", preset, "-L", self.core, self.rom()], env=environment, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.time() + self.args.wait
                history = []
                while time.time() < deadline:  # RetroArch shows a frame, blanks while Mega Bezel compiles its passes, then shows the scene: wait for a lit frame that holds still
                    time.sleep(2.0)
                    if not self.grab(display, output):
                        continue
                    mean = float(np.asarray(Image.open(output).convert("L")).mean())
                    history.append(mean)
                    if len(history) >= 3 and all(m > 2.0 for m in history[-3:]) and max(history[-3:]) - min(history[-3:]) < 0.5:
                        break
            finally:
                emulator.terminate()
                try:
                    emulator.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    emulator.kill()
        finally:
            xvfb.terminate()
            xvfb.wait()
            time.sleep(1.0)
        return output

    @classmethod
    def grab(cls, display, output):  # xwd occasionally fails while the window is being mapped
        for attempt in range(3):
            grab = subprocess.run(["xwd", "-root", "-silent", "-display", display], capture_output=True)
            if grab.returncode == 0 and subprocess.run(["magick", "xwd:-", output], input=grab.stdout).returncode == 0:
                return True
            time.sleep(1.0)
        return False

    def rom(self):
        path = os.path.join(self.out, "pattern.semu")
        if not os.path.exists(path):
            open(path, "w").write("semu synthetic content\n")
        return path

    @classmethod
    def bbox(cls, mask):
        ys, xs = np.where(mask)
        if len(xs) == 0:
            return None
        return (int(xs.min()), int(ys.min()), int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1))

    @classmethod
    def changed(cls, dark, lit):  # the picture is whatever lights up when the card does: a color transform cannot hide it
        difference = np.abs(lit.astype(np.int32) - dark.astype(np.int32)).sum(axis=2)
        mask = difference > 60
        mask = ndimage.binary_closing(mask, structure=np.ones((9, 9), dtype=bool))  # scanline and mask gaps close; the picture becomes one solid block
        mask = ndimage.binary_fill_holes(mask)
        labels, count = ndimage.label(mask)
        best = None
        for index, box in enumerate(ndimage.find_objects(labels), start=1):
            if box is None:
                continue
            ys, xs = box
            area = int((labels[ys, xs] == index).sum())
            w, h = xs.stop - xs.start, ys.stop - ys.start
            if w * h < 10000 or area / float(w * h) < 0.85:  # reflections and glows are sparse; the card is solid
                continue
            if best is None or area > best[0]:
                best = (area, (int(xs.start), int(ys.start), int(w), int(h)))
        return best[1] if best else None

    @classmethod
    def orange_mask(cls, pixels):
        r, g, b = (pixels[..., channel].astype(np.int32) for channel in range(3))
        return (r > 180) & (g > 70) & (g < 190) & (b < 80)

    @classmethod
    def silhouette(cls, path):
        alpha = np.asarray(Image.open(path).convert("RGBA"))[..., 3]
        return cls.bbox(alpha > 8)

    @classmethod
    def surround(cls, pixels, card):  # the plate's own color just outside the picture: the tube black edge or the LCD border
        x, y, w, h = card
        band = 10
        rows = np.concatenate([pixels[max(y - 2 * band, 0):max(y - band, 0), x:x + w].reshape(-1, 3), pixels[y + h + band:y + h + 2 * band, x:x + w].reshape(-1, 3)])
        if len(rows) == 0:
            return "#000000"
        median = np.median(rows, axis=0).astype(int)
        return "#%02x%02x%02x" % tuple(int(v) for v in median)

    def calibrate(self, package_id, package, system, path):
        upstream = package["upstream"]
        env = self.render_env(system)
        preset = self.wrapper(package_id, upstream)
        count = int(env.get("SEMU_RENDER_SURFACE_COUNT", "1"))
        dark = np.asarray(Image.open(self.capture(package_id, preset, env, 16)).convert("RGB"))  # flat mode with no screen lit
        cards = []
        lit_pixels = None
        for index in range(count):
            lit_pixels = np.asarray(Image.open(self.capture(package_id, preset, env, 1 << index)).convert("RGB"))
            card = self.changed(dark, lit_pixels)
            if card is None:
                raise RuntimeError(f"{package_id}: screen {index} never lit up in the upstream render")
            cards.append(card)
        pixels = dark
        capture = os.path.join(self.out, f"{package_id}-lit16.png")
        canvas = package["canvas"]
        provenance = {"tool": "tools/bezel-calibrate.py", "preset": upstream["preset"], "viewport": f"{VIEWPORT[0]}x{VIEWPORT[1]}", "cards_px": cards[:count]}
        if upstream["kind"] == "device":
            plate = self.silhouette(os.path.join(self.upstreams[DUIMON], upstream["silhouette"]))
            device = self.bbox(~self.orange_mask(dark))
            scale_x = device[2] / plate[2]
            scale_y = device[3] / plate[3]
            provenance.update({"device_px": device, "silhouette_px": plate, "scale": [round(scale_x, 5), round(scale_y, 5)]})
            if abs(scale_x - scale_y) / scale_x > 0.01:
                print(f"  warning: {package_id} device scale differs by axis ({scale_x:.4f} vs {scale_y:.4f})")
            to_plate = lambda c: {"x": round(plate[0] + (c[0] - device[0]) / scale_x), "y": round(plate[1] + (c[1] - device[1]) / scale_y), "w": round(c[2] / scale_x), "h": round(c[3] / scale_y)}
        else:
            sx, sy = canvas["w"] / VIEWPORT[0], canvas["h"] / VIEWPORT[1]
            to_plate = lambda c: {"x": round(c[0] * sx), "y": round(c[1] * sy), "w": round(c[2] * sx), "h": round(c[3] * sy)}
        for index, screen in enumerate(package["screens"][:count]):
            screen["image"] = to_plate(cards[index])
            screen["surround"] = self.surround(pixels, cards[index])
            screen.pop("inset", None)
            screen["measurement"] = dict(provenance, screen=index)
            print(f"  {package_id} {screen['id']}: image {screen['image']} surround {screen['surround']}")
        json.dump(package, open(path, "w"), indent=2)
        open(path, "a").write("\n")
        shutil.copyfile(capture, os.path.join(self.out, f"upstream-{package_id}.png"))

    def run(self):
        packages = self.packages()
        if not packages:
            print("no packages with an upstream preset", file=sys.stderr)
            return 1
        for package_id, (package, system, path) in packages.items():
            print(f"{package_id} ({system})", flush=True)
            self.calibrate(package_id, package, system, path)
        for package_id, (package, system, path) in packages.items():  # recolors and re-cuts share their base's geometry
            pass
        return 0

    @classmethod
    def main(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument("--packages", default="")
        parser.add_argument("--out", default="build/bezel-calibrate")
        parser.add_argument("--semu")
        parser.add_argument("--core")
        parser.add_argument("--display", type=int, default=88)
        parser.add_argument("--wait", type=float, default=60.0)
        return cls(parser.parse_args()).run()


sys.exit(Calibrator.main())
