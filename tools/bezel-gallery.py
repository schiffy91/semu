#!/usr/bin/env nix-shell
#!nix-shell -i python3 -p python3Packages.numpy python3Packages.pillow python3Packages.scipy xorg.xorgserver xorg.xwd imagemagick
# Renders every system's bezel variants for each screen configuration, copies the packages' art and
# background plates, and writes a static gallery site. Two renderers: "fake" (tools/bezel_fake.py,
# the compositor's geometry in numpy, seconds for everything) for iteration, and "real" (RetroArch +
# the synthetic test-card core through libsemurenderer on a private Xvfb, about nine seconds a cell).
# Real mode runs a worker pool (one RetroArch per private display, --jobs at once) with one launch per
# cell: the synthetic core's control file walks that launch through a dark frame, one lit flat card per
# screen (measured against the package geometry, 2 px) and the test card the gallery shows.
# usage: bezel-gallery.py OUT_DIR [--mode fake|real|both] [--configs deck,pc4k] [--systems nes,snes] [--jobs 6] [--display 80]
import argparse, json, os, queue, shutil, subprocess, sys, threading, time
from concurrent.futures import ThreadPoolExecutor
import numpy as np
from PIL import Image
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bezel_fake import FakeCompositor

CONFIGS = {"deck": (1280, 800, "Steam Deck 1280x800"), "pc4k": (3840, 2160, "PC 4K 3840x2160")}


class Gallery:
    def __init__(self, args):
        self.args = args
        self.repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.out = os.path.abspath(args.out)
        self.semu = args.semu or shutil.which("semu") or "/run/current-system/sw/bin/semu"
        installed = os.path.dirname(os.path.realpath(shutil.which("semu") or "/run/current-system/sw/bin/semu"))  # the installed bundle carries the assets and RetroArch
        self.retroarch = args.retroarch or os.path.join(installed, "retroarch")
        os.environ.setdefault("SEMU_ASSET_ROOT", os.path.dirname(installed))  # a locally built CLI (--semu) still resolves the bundle's assets
        self.modes = ["fake", "real"] if args.mode == "both" else [args.mode]
        self.core = args.core or (self.build_core() if "real" in self.modes else None)
        self.configs = {name: CONFIGS[name] for name in args.configs.split(",")}
        os.makedirs(os.path.join(self.out, "plates"), exist_ok=True)
        for name in self.configs:
            for mode in ("fake", "real"):
                os.makedirs(os.path.join(self.out, name, mode), exist_ok=True)
        self.work = os.path.join(self.out, ".work")
        os.makedirs(self.work, exist_ok=True)
        self.rom = os.path.join(self.work, "pattern.semu")
        open(self.rom, "w").write("semu synthetic content\n")

    def build_core(self):  # the synthetic core is a flake check output; build it from the repo
        path = subprocess.run(["nix", "build", f"{self.repo}#checks.x86_64-linux.synthetic-core", "--no-link", "--print-out-paths"], capture_output=True, text=True, check=True).stdout.strip()
        return os.path.join(path, "lib/retroarch/cores/synthetic_libretro.so")

    def systems(self):
        root = os.path.join(self.repo, "config/systems")
        wanted = self.args.systems.split(",") if self.args.systems else None
        result = []
        for system in sorted(os.listdir(root)):
            manifest = os.path.join(root, system, "bezels.json")
            if not os.path.exists(manifest) or (wanted and system not in wanted):
                continue
            bezels = json.load(open(manifest))
            if not bezels.get("enabled") or not bezels.get("variants"):
                continue
            definition = json.load(open(os.path.join(root, system, "system.json")))
            shaders_path = os.path.join(root, system, "shaders.json")
            shaders = json.load(open(shaders_path)) if os.path.exists(shaders_path) else {}
            result.append((system, definition, bezels, shaders))
        return result

    def package(self, bezel_id):
        path = os.path.join(self.repo, "config/bezels", bezel_id, "bezel.json")
        return json.load(open(path)) if os.path.exists(path) else None

    def render_env(self, system, settings):  # a --semu build runs unwrapped against the working tree's config, so package edits show without a rebuild
        executable, environment = self.semu, dict(os.environ)
        unwrapped = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(self.semu))), "lib/semu/semu-btrc")
        if self.args.semu and os.path.exists(unwrapped):
            executable = unwrapped
            environment["SEMU_SOURCE_ROOT"] = os.path.join(self.repo, "config")
        command = [executable, "render-env", "--system", system, "--emulator", "retroarch", "--settings-json", json.dumps(settings)]
        run = subprocess.run(command, capture_output=True, text=True, env=environment)
        if run.returncode != 0:
            raise RuntimeError(f"render-env failed for {system}: {run.stderr.strip()}")
        return dict(line.split("=", 1) for line in run.stdout.splitlines() if "=" in line)

    def config_file(self, width, height):
        path = os.path.join(self.work, f"retroarch-{width}x{height}.cfg")
        open(path, "w").write("\n".join([
            'video_driver = "glcore"', 'video_fullscreen = "true"', f'video_fullscreen_x = "{width}"', f'video_fullscreen_y = "{height}"',
            'video_windowed_fullscreen = "false"', 'video_vsync = "false"', 'audio_driver = "null"', 'input_driver = "x"',
            'input_joypad_driver = "null"', 'menu_driver = "rgui"', 'video_shader_enable = "false"', 'video_font_enable = "false"',
            'network_cmd_enable = "false"', 'pause_nonactive = "false"', f'screenshot_directory = "{self.work}"', 'video_window_save_positions = "false"',
        ]) + "\n")
        return path

    @classmethod
    def picture(cls, dark, lit):  # the solid region that lights up with the flat card
        difference = np.abs(lit.astype(np.int32) - dark.astype(np.int32)).sum(axis=2) > 60
        difference = ndimage.binary_fill_holes(ndimage.binary_closing(difference, structure=np.ones((7, 7), dtype=bool)))
        labels, _ = ndimage.label(difference)
        best = None
        for label, box in enumerate(ndimage.find_objects(labels), start=1):
            if box is None:
                continue
            ys, xs = box
            w, h = xs.stop - xs.start, ys.stop - ys.start
            area = int((labels[ys, xs] == label).sum())
            if w * h >= 2500 and area / float(w * h) >= 0.85 and (best is None or area > best[0]):
                best = (area, (int(xs.start), int(ys.start), int(w), int(h)))
        return best[1] if best else None

    def verify(self, env, width, height, grabs):  # the renderer's picture per screen against the package geometry
        count = int(env.get("SEMU_RENDER_SURFACE_COUNT", "1"))
        dark = np.asarray(Image.open(grabs[16]).convert("RGB"))
        expected = FakeCompositor(env, width, height)
        report = []
        for index in range(count):
            got = self.picture(dark, np.asarray(Image.open(grabs[1 << index]).convert("RGB")))
            want = expected.lanes[index].out
            delta = [got[k] - want[k] for k in range(4)] if got else None
            report.append({"screen": index, "expected": list(want), "rendered": list(got) if got else None, "delta": delta, "ok": got is not None and max(abs(d) for d in delta) <= 2})
        return report

    @classmethod
    def grab(cls, display, output):
        for attempt in range(3):
            run = subprocess.run(["xwd", "-root", "-silent", "-display", display], capture_output=True)
            if run.returncode == 0 and subprocess.run(["magick", "xwd:-", output], input=run.stdout).returncode == 0:
                return True
            time.sleep(1.0)
        return False

    def settle(self, display, output, first):  # a lit frame that holds still: three matching grabs after the launch, two after a state change
        deadline = time.time() + (self.args.wait if first else 20.0)
        history, needed = [], 3 if first else 2
        while time.time() < deadline:
            time.sleep(1.0 if first else 0.7)
            if not self.grab(display, output):
                continue
            history.append(float(np.asarray(Image.open(output).convert("L")).mean()))
            if len(history) >= needed and all(m > 1.0 for m in history[-needed:]) and max(history[-needed:]) - min(history[-needed:]) < 0.5:
                return True
        return False

    def session(self, env, width, height, name, states, display_number):  # one RetroArch launch on a private Xvfb; the control file walks it through the states
        display = f":{display_number}"
        control = os.path.join(self.work, f"{name}.control")
        open(control, "w").write(f"{states[0]}\n")
        xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", f"{width}x{height}x24"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        grabs = {}
        try:
            time.sleep(1.0)
            environment = dict(os.environ)
            environment.update(env)
            environment.update({"DISPLAY": display, "WAYLAND_DISPLAY": "", "SDL_VIDEODRIVER": "x11", "GDK_BACKEND": "x11", "SEMU_RENDER_STATE_DIR": self.work,
                                "SEMU_SYNTHETIC_FLAT": str(states[0]), "SEMU_SYNTHETIC_CONTROL": control})
            log = open(os.path.join(self.work, f"{name}.log"), "w")
            emulator = subprocess.Popen([self.retroarch, "-f", "--config", self.config_file(width, height), "-L", self.core, self.rom], env=environment, stdout=log, stderr=subprocess.STDOUT)
            try:
                for state in states:
                    open(control, "w").write(f"{state}\n")
                    output = os.path.join(self.work, f"{name}-state{state}.png")
                    if not self.settle(display, output, first=state == states[0]):
                        raise RuntimeError(f"{name}: no stable frame for state {state}")
                    grabs[state] = output
            finally:
                emulator.terminate()
                try:
                    emulator.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    emulator.kill()
        finally:
            xvfb.terminate()
            xvfb.wait()
        return grabs

    def real_cell(self, cell):  # one launch: dark, each screen lit (when the package has a picture rectangle), then the test card
        display_number = self.displays.get()
        try:
            states = ([16] + [1 << index for index in range(cell["count"])] if cell["verify"] else []) + [0]
            with self.lock:
                print(f"{cell['name']} @ {cell['config']} (real{', verify' if cell['verify'] else ''}) on :{display_number}", flush=True)
            grabs = self.session(cell["env"], cell["width"], cell["height"], f"{cell['name']}-{cell['config']}", states, display_number)
            shutil.copyfile(grabs[0], cell["output"])
            checks = self.verify(cell["env"], cell["width"], cell["height"], grabs) if cell["verify"] else None
            if checks:
                with self.lock:
                    for item in checks:
                        print(f"  {cell['name']} @ {cell['config']} screen {item['screen']}: {'ok' if item['ok'] else 'OFF'} delta {item['delta']}", flush=True)
            return cell, checks, None
        except Exception as error:
            with self.lock:
                print(f"  FAILED {cell['name']} @ {cell['config']}: {error}", flush=True)
            return cell, None, str(error)
        finally:
            self.displays.put(display_number)

    def plate(self, logical):  # copy a package plate into the site, downscaled for the page
        if not logical:
            return None
        name = logical.replace("assets/bezels/", "").replace("/", "-")
        source = os.path.join(self.repo, "config", logical)
        if not os.path.exists(source):
            return None
        target = os.path.join(self.out, "plates", name)
        if not os.path.exists(target):
            subprocess.run(["magick", source, "-resize", "1280x1280>", "-background", "#202020", "-alpha", "background", target], check=True)
        width, height = subprocess.run(["magick", "identify", "-format", "%w %h", source], capture_output=True, text=True, check=True).stdout.split()
        return {"file": f"plates/{name}", "source": logical, "width": int(width), "height": int(height)}

    def sheets(self):  # the dimension sheets from tools/bezel-dimensions.py, copied next to the gallery when they exist
        source = os.path.join(self.repo, "build/bezel-dimensions")
        if not os.path.isfile(os.path.join(source, "dimensions.html")):
            return None
        target = os.path.join(self.out, "sheets")
        shutil.rmtree(target, ignore_errors=True)
        shutil.copytree(source, target, ignore=shutil.ignore_patterns("shaders", "*.params.json"))
        return {"dimensions": "sheets/dimensions.html", "overlay": "sheets/overlay.html"}

    def run(self):
        manifest = {"generated": time.strftime("%Y-%m-%d %H:%M"), "configs": {name: {"width": w, "height": h, "label": label} for name, (w, h, label) in self.configs.items()}, "systems": [], "sheets": self.sheets()}
        self.lock = threading.Lock()
        self.displays = queue.Queue()
        for offset in range(self.args.jobs):
            self.displays.put(self.args.display + offset)
        cells = []
        for system, definition, bezels, shaders in self.systems():
            entry = {"id": system, "name": definition.get("name", system), "screens": definition.get("display", {}).get("screens", []),
                     "shader_default": shaders.get("default_variant"), "shader_variants": [v.get("id") for v in shaders.get("variants", [])], "variants": []}
            choices = [(v["id"], v.get("label", v["id"]), v.get("bezel")) for v in bezels["variants"]] + [("none", "No bezel", None)]
            for variant_id, label, bezel_id in choices:
                package = self.package(bezel_id) if bezel_id else None
                settings = {"visual": {"bezels": False}} if variant_id == "none" else {"visual": {"systems": {system: {"bezel_variant": variant_id}}}}
                env = self.render_env(system, settings)
                captures = {}
                checks = {}
                for config, (width, height, _) in self.configs.items():
                    captures[config] = {}
                    for mode in ("fake", "real"):
                        relative = f"{config}/{mode}/{system}-{variant_id}.png"
                        output = os.path.join(self.out, relative)
                        if mode in self.modes and (not os.path.exists(output) or self.args.force):
                            if mode == "fake":
                                print(f"{system} {variant_id} @ {config} (fake)", flush=True)
                                FakeCompositor(env, width, height).render().save(output)
                            else:
                                verify = self.args.verify and package is not None and any(s.get("image") for s in package.get("screens", []))
                                cells.append({"name": f"{system}-{variant_id}", "config": config, "env": env, "width": width, "height": height, "output": output,
                                              "count": int(env.get("SEMU_RENDER_SURFACE_COUNT", "1")), "verify": verify, "checks": checks})
                        captures[config][mode] = relative
                entry["variants"].append({
                    "id": variant_id, "label": label, "bezel": bezel_id, "default": bezels.get("default_variant") == variant_id,
                    "layout": package.get("layout") if package else None, "family": package.get("family") if package else None,
                    "frame": package.get("frame") if package else None, "canvas": package.get("canvas") if package else None,
                    "screens": package.get("screens") if package else None,
                    "art": self.plate(package.get("art")) if package else None,
                    "background": self.plate(package.get("background")) if package else None,
                    "captures": captures, "checks": checks,
                    "env": {key: value for key, value in env.items() if key.startswith("SEMU_RENDER_SCREEN") or key in ("SEMU_RENDER_LAYOUT", "SEMU_RENDER_FRAME", "SEMU_RENDER_SHADER_PRESET")},
                })
            manifest["systems"].append(entry)
        failures = []
        if cells:
            with ThreadPoolExecutor(max_workers=self.args.jobs) as pool:
                for cell, checks, error in pool.map(self.real_cell, cells):
                    if checks:
                        cell["checks"][cell["config"]] = checks
                    if error or (checks and any(not item["ok"] for item in checks)):
                        failures.append(f"{cell['name']} @ {cell['config']}")
        for entry in manifest["systems"]:
            for variant in entry["variants"]:
                for config in variant["captures"]:
                    for mode in ("fake", "real"):
                        relative = variant["captures"][config][mode]
                        if relative and not os.path.exists(os.path.join(self.out, relative)):
                            variant["captures"][config][mode] = None
        json.dump(manifest, open(os.path.join(self.out, "manifest.json"), "w"), indent=1)
        page = open(os.path.join(self.repo, "tools/bezel-gallery.html")).read().replace("/*MANIFEST*/null", json.dumps(manifest))
        open(os.path.join(self.out, "index.html"), "w").write(page)
        print(os.path.join(self.out, "index.html") + (f"; verification failed: {', '.join(failures)}" if failures else ""))
        return 1 if failures else 0

    @classmethod
    def main(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument("out")
        parser.add_argument("--mode", default="fake", choices=["fake", "real", "both"])
        parser.add_argument("--configs", default="deck,pc4k")
        parser.add_argument("--systems", default="")
        parser.add_argument("--wait", type=float, default=60.0, help="seconds to wait for the first stable frame of a launch")
        parser.add_argument("--display", type=int, default=80)
        parser.add_argument("--jobs", type=int, default=6, help="real mode: RetroArch instances at once, each on its own display")
        parser.add_argument("--semu")
        parser.add_argument("--retroarch")
        parser.add_argument("--core")
        parser.add_argument("--force", action="store_true")
        parser.add_argument("--verify", action="store_true", help="real mode: light the flat card in the Semu renderer and check the picture lands where the package says (2 px)")
        return cls(parser.parse_args()).run()


sys.exit(Gallery.main())
