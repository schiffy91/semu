#!/usr/bin/env nix-shell
#!nix-shell -i python3 -p python3 xorg.xorgserver xorg.xwd imagemagick
# Renders every system's bezel variants through the real Semu renderer (RetroArch + the synthetic
# test-card core on a private Xvfb display) for each screen configuration, copies the packages'
# art and background plates, and writes a static gallery site.
# usage: bezel-gallery.py OUT_DIR [--configs deck,pc4k] [--systems nes,snes] [--wait 6] [--display 80]
import argparse, json, os, shutil, subprocess, sys, time

CONFIGS = {"deck": (1280, 800, "Steam Deck 1280x800"), "pc4k": (3840, 2160, "PC 4K 3840x2160")}


class Gallery:
    def __init__(self, args):
        self.args = args
        self.repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.out = os.path.abspath(args.out)
        self.semu = args.semu or shutil.which("semu") or "/run/current-system/sw/bin/semu"
        bundle = os.path.dirname(os.path.realpath(self.semu))
        self.retroarch = args.retroarch or os.path.join(bundle, "retroarch")
        self.core = args.core or self.build_core()
        self.configs = {name: CONFIGS[name] for name in args.configs.split(",")}
        os.makedirs(os.path.join(self.out, "plates"), exist_ok=True)
        for name in self.configs:
            os.makedirs(os.path.join(self.out, name), exist_ok=True)
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

    def render_env(self, system, settings):
        command = [self.semu, "render-env", "--system", system, "--emulator", "retroarch", "--settings-json", json.dumps(settings)]
        run = subprocess.run(command, capture_output=True, text=True)
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

    def capture(self, env, width, height, output):  # one RetroArch launch on a private Xvfb, then a root-window grab
        display = f":{self.args.display}"
        xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", f"{width}x{height}x24"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            time.sleep(1.0)
            environment = dict(os.environ)
            environment.update(env)
            environment.update({"DISPLAY": display, "WAYLAND_DISPLAY": "", "SDL_VIDEODRIVER": "x11", "GDK_BACKEND": "x11", "SEMU_RENDER_STATE_DIR": self.work})
            log = open(os.path.join(self.work, "retroarch.log"), "w")
            emulator = subprocess.Popen([self.retroarch, "-f", "--config", self.config_file(width, height), "-L", self.core, self.rom], env=environment, stdout=log, stderr=subprocess.STDOUT)
            try:
                time.sleep(self.args.wait)
                grab = subprocess.run(["xwd", "-root", "-silent", "-display", display], capture_output=True, check=True)
                subprocess.run(["magick", "xwd:-", output], input=grab.stdout, check=True)
            finally:
                emulator.terminate()
                try:
                    emulator.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    emulator.kill()
        finally:
            xvfb.terminate()
            xvfb.wait()

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

    def run(self):
        manifest = {"generated": time.strftime("%Y-%m-%d %H:%M"), "configs": {name: {"width": w, "height": h, "label": label} for name, (w, h, label) in self.configs.items()}, "systems": []}
        for system, definition, bezels, shaders in self.systems():
            entry = {"id": system, "name": definition.get("name", system), "screens": definition.get("display", {}).get("screens", []),
                     "shader_default": shaders.get("default_variant"), "shader_variants": [v.get("id") for v in shaders.get("variants", [])], "variants": []}
            choices = [(v["id"], v.get("label", v["id"]), v.get("bezel")) for v in bezels["variants"]] + [("none", "No bezel", None)]
            for variant_id, label, bezel_id in choices:
                package = self.package(bezel_id) if bezel_id else None
                settings = {"visual": {"bezels": False}} if variant_id == "none" else {"visual": {"systems": {system: {"bezel_variant": variant_id}}}}
                env = self.render_env(system, settings)
                captures = {}
                for config, (width, height, _) in self.configs.items():
                    output = os.path.join(self.out, config, f"{system}-{variant_id}.png")
                    if not os.path.exists(output) or self.args.force:
                        print(f"{system} {variant_id} @ {config}", flush=True)
                        self.capture(env, width, height, output)
                    captures[config] = f"{config}/{system}-{variant_id}.png"
                entry["variants"].append({
                    "id": variant_id, "label": label, "bezel": bezel_id, "default": bezels.get("default_variant") == variant_id,
                    "layout": package.get("layout") if package else None, "family": package.get("family") if package else None,
                    "frame": package.get("frame") if package else None, "canvas": package.get("canvas") if package else None,
                    "screens": package.get("screens") if package else None,
                    "art": self.plate(package.get("art")) if package else None,
                    "background": self.plate(package.get("background")) if package else None,
                    "captures": captures, "env": {key: value for key, value in env.items() if key.startswith("SEMU_RENDER_SCREEN") or key in ("SEMU_RENDER_LAYOUT", "SEMU_RENDER_FRAME", "SEMU_RENDER_SHADER_PRESET")},
                })
            manifest["systems"].append(entry)
        json.dump(manifest, open(os.path.join(self.out, "manifest.json"), "w"), indent=1)
        page = open(os.path.join(self.repo, "tools/bezel-gallery.html")).read().replace("/*MANIFEST*/null", json.dumps(manifest))
        open(os.path.join(self.out, "index.html"), "w").write(page)
        print(os.path.join(self.out, "index.html"))

    @classmethod
    def main(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument("out")
        parser.add_argument("--configs", default="deck,pc4k")
        parser.add_argument("--systems", default="")
        parser.add_argument("--wait", type=float, default=6.0)
        parser.add_argument("--display", type=int, default=80)
        parser.add_argument("--semu")
        parser.add_argument("--retroarch")
        parser.add_argument("--core")
        parser.add_argument("--force", action="store_true")
        cls(parser.parse_args()).run()
        return 0


sys.exit(Gallery.main())
