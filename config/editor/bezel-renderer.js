// The renderer, ported for the bezel editor by name: what `semu render-env` emits for a package
// (SemuRenderEnvironment and SemuRenderLayers in src/emit/rendering.btrc) as the renderer reads it
// (RendererConfiguration), and where it places it (RendererGeometry, RendererPlacement, RendererCompositionContract in
// src/renderer). Arithmetic the C does in float is rounded to 32 bits here, so edges land on the same
// pixels; tests/visual/editor-sync.sh compares the editor with the real renderer.
"use strict";
const BezelRenderer = (() => {
  const float = Math.fround;
  const integer = value => Math.trunc(value);  // C's (int) cast and integer division

  class CompositionContract {  // RendererCompositionContract, src/renderer/renderer_abi.btrc
    static rounded(value) { return integer(float(value + 0.5)); }
    static edge(value) {  // the nearest pixel edge, below zero too
      const lifted = float(value + 0.5), truncated = integer(lifted);
      return truncated > lifted ? truncated - 1 : truncated;
    }
    static empty() { return { x: 0, y: 0, width: 0, height: 0 }; }
    static contain(viewportWidth, viewportHeight, contentWidth, contentHeight) {
      const output = CompositionContract.empty();
      if (viewportWidth < 1 || viewportHeight < 1 || contentWidth < 1 || contentHeight < 1) return output;
      const horizontal = float(viewportWidth / contentWidth), vertical = float(viewportHeight / contentHeight), scale = horizontal < vertical ? horizontal : vertical;
      output.width = float(scale * contentWidth); output.height = float(scale * contentHeight);
      output.x = float(float(viewportWidth - output.width) * 0.5); output.y = float(float(viewportHeight - output.height) * 0.5);
      return output;
    }
    static aperture(canvas, hole) {  // a normalized top-left hole on a bottom-up canvas, each edge rounded
      const output = CompositionContract.empty();
      if (!hole || !hole.set || canvas.width <= 0 || canvas.height <= 0 || hole.x < 0 || hole.y < 0 || hole.width <= 0 || hole.height <= 0
        || float(hole.x + hole.width) > float(1.0001) || float(hole.y + hole.height) > float(1.0001)) return output;
      output.x = CompositionContract.edge(float(canvas.x + float(hole.x * canvas.width)));
      output.y = CompositionContract.edge(float(canvas.y + float(float(float(1 - hole.y) - hole.height) * canvas.height)));
      output.width = CompositionContract.edge(float(canvas.x + float(float(hole.x + hole.width) * canvas.width))) - output.x;
      output.height = CompositionContract.edge(float(canvas.y + float(float(1 - hole.y) * canvas.height))) - output.y;
      return output;
    }
    static fit(area, nativeWidth, nativeHeight, rotation, declaredAspect, integerScaling) {
      const output = CompositionContract.empty();
      if (area.width <= 0 || area.height <= 0 || nativeWidth < 1 || nativeHeight < 1) return output;
      const rotatedWidth = rotation === 90 || rotation === 270 ? nativeHeight : nativeWidth, rotatedHeight = rotation === 90 || rotation === 270 ? nativeWidth : nativeHeight;
      const aspect = declaredAspect > 0.01 ? declaredAspect : float(rotatedWidth / rotatedHeight);
      let height = area.height, width = CompositionContract.rounded(float(height * aspect));
      if (width > area.width) { width = area.width; height = CompositionContract.rounded(float(width / aspect)); }
      if (integerScaling) {
        const integerWidth = CompositionContract.rounded(float(rotatedHeight * aspect));
        const scale = Math.min(integer(area.width / integerWidth), integer(area.height / rotatedHeight));
        if (scale > 0) { width = integerWidth * scale; height = rotatedHeight * scale; }
      }
      output.width = width; output.height = height;
      output.x = area.x + integer((area.width - width) / 2); output.y = area.y + integer((area.height - height) / 2);
      return output;
    }
  }

  class Environment {  // SemuRenderEnvironment as RendererConfiguration reads it back: %.6g text, then a float
    static fraction(value) { return float(Number(Number(value).toPrecision(6))); }
    static number(value, fallback) { return typeof value === "number" && Number.isFinite(value) ? value : fallback; }
    static hole(rect, canvas) {  // rect() then parseHole(): canvas pixels to a normalized top-left rectangle, unset when it does not fit
      const unset = { set: false, x: 0, y: 0, width: 0, height: 0 };
      if (!rect || !canvas || !(canvas.w > 0) || !(canvas.h > 0)) return unset;
      const x = Environment.number(rect.x, -1), y = Environment.number(rect.y, -1), w = Environment.number(rect.w, 0), h = Environment.number(rect.h, 0);
      if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > canvas.w + 0.5 || y + h > canvas.h + 0.5) return unset;
      const hole = { set: true, x: Environment.fraction(x / canvas.w), y: Environment.fraction(y / canvas.h), width: Environment.fraction(w / canvas.w), height: Environment.fraction(h / canvas.h) };
      return float(hole.x + hole.width) <= float(1.0001) && float(hole.y + hole.height) <= float(1.0001) ? hole : unset;
    }
    static color(hex, fallback) {  // #rrggbb to 0..1
      if (typeof hex !== "string" || !/^#[0-9a-fA-F]{6}$/.test(hex)) return fallback;
      const value = parseInt(hex.slice(1), 16);
      return [(value >> 16) & 255, (value >> 8) & 255, value & 255].map(channel => Environment.fraction(channel / 255));
    }
    static clamp01(value) { return value < 0 ? 0 : (value > 1 ? 1 : value); }
    static shortSide(rect) { return rect ? Math.min(Environment.number(rect.w, 0), Environment.number(rect.h, 0)) : 0; }
    static screenOf(pkg, id) {  // SemuBezelPackage.screen
      const screens = pkg && Array.isArray(pkg.screens) ? pkg.screens : [];
      return screens.find(screen => screen.id === id) || (screens.length === 1 && id === "main" ? screens[0] : null);
    }
    static screen(source, canvas) {  // loadScreen() over what variant() emits for one screen
      const screen = { id: source.id, tube: Environment.hole(source.tube, canvas), image: Environment.hole(source.image, canvas), ringSet: false, lookSet: true };
      if (!screen.tube.set) screen.tube = Environment.hole(source.image, canvas);  // a calibrated image rectangle stands in for an unmeasured opening
      const ring = source.ring;
      if (ring && typeof ring === "object") {
        const inner = Environment.hole(ring.inner, canvas), outer = Environment.hole(ring.outer, canvas);
        if (inner.set && outer.set) {
          const innerSide = Environment.shortSide(ring.inner), outerSide = Environment.shortSide(ring.outer), none = ring.color === "none";
          Object.assign(screen, {
            ringSet: true, ringInner: inner, ringOuter: outer,
            ringInnerRadius: Environment.clamp01(Environment.fraction(innerSide > 0 ? Environment.number(ring.inner_radius, 0) / innerSide : 0)),
            ringOuterRadius: Environment.clamp01(Environment.fraction(outerSide > 0 ? Environment.number(ring.outer_radius, 0) / outerSide : 0)),
            ringColor: none ? [0, 0, 0] : Environment.color(ring.color, [0.1, 0.1, 0.1].map(float)), ringOpacity: none ? 0 : 1,
            ringBevel: Environment.clamp01(Environment.fraction(Environment.number(ring.bevel, 1))),
          });
        }
      }
      const shape = source.shape && typeof source.shape === "object" ? source.shape : null, bulge = shape && shape.bulge, tube = source.tube;
      screen.bulgeX = 0; screen.bulgeY = 0;
      if (bulge && tube && tube.w > 0 && tube.h > 0 && (Environment.number(bulge.x, 0) > 0 || Environment.number(bulge.y, 0) > 0)) {
        screen.bulgeX = Environment.clamp01(Environment.fraction(Environment.number(bulge.x, 0) / (tube.w / 2)));
        screen.bulgeY = Environment.clamp01(Environment.fraction(Environment.number(bulge.y, 0) / (tube.h / 2)));
      }
      const reflection = source.reflection, strength = reflection ? Environment.number(reflection.strength, 0) : 0;
      screen.reflect = 0; screen.reflectBlur = 0; screen.reflectFade = 0;
      if (strength > 0) {
        screen.reflect = Environment.clamp01(Environment.fraction(strength));
        screen.reflectBlur = Environment.clamp01(Environment.fraction(Environment.number(reflection.blur, 0)));
        screen.reflectFade = Environment.clamp01(Environment.fraction(Environment.number(reflection.fade, 0)));
      }
      const kind = shape ? shape.kind : "", glass = source.glass && typeof source.glass === "object" ? source.glass : null;
      screen.shape = kind === "rounded" ? 1 : (kind === "squircle" ? 2 : 0);
      screen.radius = Environment.clamp01(Environment.fraction(Environment.number(shape && shape.radius, 0)));
      screen.exponent = Math.max(2, Environment.fraction(Environment.number(shape && shape.exponent, 4)));
      screen.fit = source.fit === "fill" ? 1 : (source.fit === "integer" ? 2 : 0);
      screen.inset = float(Environment.clamp01(Environment.fraction(Environment.number(source.inset, 0))) * float(0.45));
      screen.curvature = Environment.fraction(Environment.number(source.curvature, 0));
      screen.vignette = Environment.fraction(Environment.number(source.vignette, 0));
      screen.bloom = Environment.fraction(Environment.number(source.bloom, 0));
      screen.glassReflect = Environment.fraction(glass ? Environment.number(glass.reflect, 0) : 0);
      screen.glass = glass && typeof glass.asset === "string" ? glass.asset : "";
      screen.surround = Environment.color(source.surround, [0, 0, 0]);
      return screen;
    }
    static layers(pkg, available) {  // SemuRenderLayers.emit: the drawable stack bottom first, each with its extent, side, blend, opacity, tint and lift
      const canvas = pkg.canvas, layers = Array.isArray(pkg.layers) ? pkg.layers : [];
      if (!canvas || !(canvas.w > 0)) return [];
      const cutouts = layers.find(layer => layer.id === "cutouts"), screensOrder = cutouts ? Environment.number(cutouts.order, 0) : 0, recolor = pkg.recolor;
      const lines = [];
      const place = (line, order) => { let at = 0; while (at < lines.length && lines[at].order <= order) at++; lines.splice(at, 0, { ...line, order }); };
      let backgroundExtent = null, backgroundOrder = 0;
      for (const layer of layers) {
        if (layer.id === "cutouts" || layer.visible === false || !available(layer.id)) continue;
        const order = Environment.number(layer.order, 0), canvasSized = layer.size && layer.size.w === canvas.w && layer.size.h === canvas.h;
        const extent = layer.follow === "viewport" ? (canvasSized ? { set: true, x: 0, y: 0, width: 1, height: 1 } : "cover") : Environment.hole(layer.rect, canvas);
        if (extent !== "cover" && !extent.set) continue;  // the renderer is never given a layer that leaves the canvas
        if (layer.id === "background") { backgroundExtent = extent; backgroundOrder = order; }
        const tinted = recolor && Array.isArray(recolor.layers) && recolor.layers.includes(layer.id);
        place({ id: layer.id, extent, above: order > screensOrder, blend: layer.blend === "add" ? 1 : (layer.blend === "multiply" ? 2 : 0), opacity: Environment.clamp01(Environment.fraction(Environment.number(layer.opacity, 1))),
          tint: tinted ? [...Environment.color(recolor.color, [1, 1, 1]), Environment.fraction(Environment.number(recolor.brightness, 1))] : [0, 0, 0, 0], lift: 0 }, order);
      }
      const ambient = pkg.ambient;
      if (ambient && typeof ambient === "object" && backgroundExtent && available("ambient")) {  // the late-night light, multiplied over the scene below the screens
        place({ id: "ambient", extent: backgroundExtent, above: false, blend: 2, opacity: 1, tint: [0, 0, 0, 0], lift: Environment.clamp01(Environment.fraction(1 - Environment.number(ambient.opacity, 1))) }, backgroundOrder + 0.5);
      }
      return lines.length > 8 ? [] : lines;
    }
    static variant(pkg, preview, available) {  // the renderer's variant config for this package as `preview` shows it
      const layout = ["main_right", "main_left", "side_by_side", "stacked"].includes(pkg.layout) ? pkg.layout : "fixed";
      const ids = preview.screens.map(screen => screen.id), canvas = pkg.canvas && typeof pkg.canvas === "object" ? pkg.canvas : null;
      const layers = canvas ? Environment.layers(pkg, available) : [];
      const canvasLayer = (pkg.layers || []).find(layer => layer.id === pkg.canvas_layer);
      const frame = pkg.frame && typeof pkg.frame === "object" ? pkg.frame : null;
      const frameWidth = frame ? Environment.fraction(Environment.number(frame.width, preview.frame_width || 0)) : 0;
      return {
        layout, layered: layers.length > 0 && !!canvas, canvasWidth: layers.length ? canvas.w : 0, canvasHeight: layers.length ? canvas.h : 0,
        canvasCover: layers.length > 0 && !!canvasLayer && canvasLayer.follow === "viewport", layers, background: typeof pkg.background === "string" ? pkg.background : "",
        frame: { set: !!frame && frameWidth > 0, width: frameWidth,
          color: frame ? Environment.color(frame.color, [0.08, 0.08, 0.08].map(float)) : [0, 0, 0] },
        screens: ids.map(id => { const source = Environment.screenOf(pkg, ids.length === 1 ? "main" : id); return source ? Environment.screen(source, canvas) : null; }),
      };
    }
  }

  class Geometry {  // RendererGeometry (renderer_compositor.btrc) and RendererPlacement (renderer_placement.btrc); every rectangle bottom-up, as GL counts rows
    static coverKeepingTubes(variant, surfaceCount, areaWidth, areaHeight, canvas) {
      if (canvas.width <= 0 || canvas.height <= 0) return canvas;
      const horizontal = float(areaWidth / canvas.width), vertical = float(areaHeight / canvas.height), grow = horizontal > vertical ? horizontal : vertical;
      const width = float(canvas.width * grow), height = float(canvas.height * grow), x = float(float(areaWidth - width) * 0.5), y = float(float(areaHeight - height) * 0.5);
      for (let index = 0; index < surfaceCount && index < 2; index++) {
        const tube = variant.screens[index] && variant.screens[index].tube.set ? variant.screens[index].tube : { x: 0, y: 0, width: 0, height: 0 };
        const left = float(x + float(tube.x * width)), top = float(y + float(tube.y * height));
        if (left < 0 || top < 0 || float(left + float(tube.width * width)) > areaWidth || float(top + float(tube.height * height)) > areaHeight) return canvas;
      }
      return { x, y, width, height };
    }
    static integerPlacement(nativeHeight, screen, placement, aspect, areaWidth, areaHeight, canvas, fractionalBelow) {  // the picture at a whole multiple of its native height; FRACTIONAL_BELOW: a 3D-era system's opt-in for game
      let area = screen.image;
      if (!area.set) {
        const tubeWidth = float(screen.tube.width * canvas.width), tubeHeight = float(screen.tube.height * canvas.height);
        const inset = float(screen.inset * (tubeWidth < tubeHeight ? tubeWidth : tubeHeight));
        area = { set: true, x: float(screen.tube.x + float(inset / canvas.width)), y: float(screen.tube.y + float(inset / canvas.height)),
          width: float(screen.tube.width - float(float(2 * inset) / canvas.width)), height: float(screen.tube.height - float(float(2 * inset) / canvas.height)) };
      }
      if (area.width <= 0 || area.height <= 0) return canvas;
      let pictureWidth = float(area.width * canvas.width), pictureHeight = float(area.height * canvas.height);
      const shown = aspect > 0.01 ? aspect : float(pictureWidth / pictureHeight);
      if (float(pictureWidth / pictureHeight) > shown) pictureWidth = float(pictureHeight * shown); else pictureHeight = float(pictureWidth / shown);
      const native = float(nativeHeight);
      if (native < 1 || pictureHeight < 1) return canvas;
      let multiple = integer(float(pictureHeight / native));
      if (placement === "game") multiple = Math.min(integer(float(areaHeight / native)), integer(float(areaWidth / float(native * shown))));
      let steps = multiple;
      if (multiple < 1 || (placement === "game" && multiple < (fractionalBelow || 0))) {  // a system that opts in (display.scaling.game_fractional_below) fills the screen in game placement; bezel stays whole
        const down = float(areaHeight / native), across = float(areaWidth / float(native * shown));
        steps = placement === "game" ? (down < across ? down : across) : float(pictureHeight / native);
      }
      const grow = float(float(steps * native) / pictureHeight);
      const centerX = float(canvas.x + float(float(area.x + float(area.width * 0.5)) * canvas.width));
      const centerY = float(canvas.y + float(float(float(1 - area.y) - float(area.height * 0.5)) * canvas.height));
      const anchorX = placement === "game" ? float(areaWidth * 0.5) : float(float(canvas.x + float(canvas.width * 0.5)) + float(float(float(centerX - canvas.x) - float(canvas.width * 0.5)) * grow));
      const anchorY = placement === "game" ? float(areaHeight * 0.5) : float(float(canvas.y + float(canvas.height * 0.5)) + float(float(float(centerY - canvas.y) - float(canvas.height * 0.5)) * grow));
      const placed = { x: float(anchorX - float(float(centerX - canvas.x) * grow)), y: float(anchorY - float(float(centerY - canvas.y) * grow)), width: float(canvas.width * grow), height: float(canvas.height * grow) };
      if (placement === "game") {  // on an axis the whole shell fits, the shell is centred so the backdrop shows evenly; a whole-pixel shift keeps the picture on the grid
        if (placed.width <= areaWidth) placed.x = float(placed.x + Geometry.wholePixels(float(float(float(areaWidth - placed.width) * 0.5) - placed.x)));
        if (placed.height <= areaHeight) placed.y = float(placed.y + Geometry.wholePixels(float(float(float(areaHeight - placed.height) * 0.5) - placed.y)));
      }
      return placed;
    }
    static wholePixels(shift) {  // RendererPlacement.wholePixels: rounded half away from zero
      return shift >= 0 ? integer(float(shift + 0.5)) : -integer(float(0.5 - shift));
    }
    static placeInTube(screen, tube, image, nativeWidth, nativeHeight, declaredAspect) {  // the game inside the calibrated image rectangle, else inside the inset opening
      const insetPixels = integer(float(float(screen.inset * Math.min(tube.width, tube.height)) + 0.5));
      let area = { x: tube.x + insetPixels, y: tube.y + insetPixels, width: tube.width - 2 * insetPixels, height: tube.height - 2 * insetPixels };
      if (image && image.width > 0 && image.height > 0) area = { ...image };
      if (area.width < 1 || area.height < 1) area = { ...tube };
      if (screen.fit === 1) return area;
      return CompositionContract.fit(area, nativeWidth, nativeHeight, 0, declaredAspect, screen.fit === 2 ? 1 : 0);
    }
    static canvasOn(variant, preview, areaWidth, areaHeight) {  // resolve()'s canvas for a fixed package on an areaWidth x areaHeight screen
      let canvas = CompositionContract.contain(areaWidth, areaHeight, variant.canvasWidth, variant.canvasHeight);
      if (variant.layered && variant.canvasCover) canvas = Geometry.coverKeepingTubes(variant, preview.screens.length, areaWidth, areaHeight, canvas);
      const first = variant.screens[0];
      if (preview.placement !== "fit" && preview.screens.length === 1 && first && (first.image.set || first.tube.set)) {
        canvas = Geometry.integerPlacement(preview.screens[0].h, first, preview.placement, Geometry.singleAspect(preview), areaWidth, areaHeight, canvas, preview.game_fractional_below);
      }
      return canvas;
    }
    static singleAspect(preview) {  // the frame's presentation aspect, else SEMU_RENDER_ASPECT
      if (preview.presentation > 0.01) return float(preview.presentation);
      return preview.aspect && preview.aspect.w > 0 && preview.aspect.h > 0 ? float(float(preview.aspect.w) / float(preview.aspect.h)) : 0;
    }
    static lanes(variant, preview, canvas) {  // each screen's opening and fitted picture on that canvas
      return variant.screens.map((screen, index) => {
        if (!screen) return null;
        const tube = CompositionContract.aperture(canvas, screen.tube), image = screen.image.set ? CompositionContract.aperture(canvas, screen.image) : null;
        const native = preview.screens[index];
        return { tube, output: Geometry.placeInTube(screen, tube, image, native.w, native.h, preview.screens.length === 1 ? Geometry.singleAspect(preview) : 0), native };
      });
    }
  }

  class TestCard {  // RENDER_HOST_CARD=editor in tests/visual/render_host.btrc: the same pixels, rows bottom first as GL stores them
    static pixels(width, height, screen, kind) {
      const data = new Uint8Array(width * height * 4);
      if (kind === "white" || kind === "black") { data.fill(kind === "white" ? 255 : 0); return data; }
      const cell = Math.max(1, float(width / 32)), block = float(cell * 4);
      for (let row = 0; row < height; row++) for (let column = 0; column < width; column++) {
        const upper = row * 3 < height * 2;
        let red = upper ? (screen === 0 ? 48 : 200) : (screen === 0 ? 48 : 232), green = upper ? (screen === 0 ? 96 : 72) : (screen === 0 ? 200 : 176), blue = upper ? (screen === 0 ? 224 : 48) : (screen === 0 ? 64 : 40);
        if (column === 0 || row === 0 || column === width - 1 || row === height - 1 || column === integer(width / 2) || row === integer(height / 2)) { red = 255; green = 255; blue = 255; }
        const cornerColumn = column < block || column >= float(width - block), cornerRow = row < block || row >= float(height - block);
        if (cornerColumn && cornerRow) {  // each corner's 4x4 checker starts white at its own top left
          const across = column < block ? column : float(column - float(width - block)), down = row < block ? row : float(row - float(height - block));
          red = green = blue = (integer(float(across / cell)) + integer(float(down / cell))) % 2 === 0 ? 255 : 16;
        }
        const offset = ((height - 1 - row) * width + column) * 4;
        data[offset] = red; data[offset + 1] = green; data[offset + 2] = blue; data[offset + 3] = 255;
      }
      return data;
    }
    static canvas(width, height, screen, kind) {  // the same card top row first, for a 2D canvas
      const pixels = TestCard.pixels(width, height, screen, kind), out = document.createElement("canvas");
      out.width = width; out.height = height;
      const context = out.getContext("2d"), image = context.createImageData(width, height);
      for (let row = 0; row < height; row++) image.data.set(pixels.subarray((height - 1 - row) * width * 4, (height - row) * width * 4), row * width * 4);
      context.putImageData(image, 0, 0);
      return out;
    }
  }

  class Compositor {  // config/render/compositor.{vert,frag} in WebGL2, drawn pass by pass as RendererCompositor.game draws them
    constructor() {
      this.canvas = document.createElement("canvas");
      this.gl = this.canvas.getContext("webgl2", { alpha: false, antialias: false, depth: false, stencil: false, premultipliedAlpha: false, preserveDrawingBuffer: true });
      this.textures = new Map();
      this.failure = this.gl ? "" : "this browser has no WebGL2";
    }
    async load() {  // the renderer's own shader text, served with the GLSL ES header
      if (!this.gl) return false;
      const gl = this.gl, [vertex, fragment] = await Promise.all(["vert", "frag"].map(name => fetch(`/render/compositor.${name}`).then(response => response.ok ? response.text() : Promise.reject(new Error(`compositor.${name}: HTTP ${response.status}`)))));
      const compile = (type, source) => { const shader = gl.createShader(type); gl.shaderSource(shader, source); gl.compileShader(shader); if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader)); return shader; };
      const program = gl.createProgram();
      gl.attachShader(program, compile(gl.VERTEX_SHADER, vertex)); gl.attachShader(program, compile(gl.FRAGMENT_SHADER, fragment)); gl.linkProgram(program);
      if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program));
      this.program = program; this.vertexArray = gl.createVertexArray(); this.uniforms = {};
      gl.useProgram(program);
      const units = { uGame: 0, uBezel: 1, uGlass: 2, uMenu: 3, uGame2: 4, uGlass2: 5, uBackground: 6, uRaw: 7, uRaw2: 8 };  // bindSamplers
      for (const [name, unit] of Object.entries(units)) { const location = gl.getUniformLocation(program, name); if (location) gl.uniform1i(location, unit); }
      this.black = this.upload(null, new Uint8Array([0, 0, 0, 255]), 1, 1, false);
      return true;
    }
    uniform(name) { if (!(name in this.uniforms)) this.uniforms[name] = this.gl.getUniformLocation(this.program, name); return this.uniforms[name]; }
    set4(name, a, b, c, d) { const location = this.uniform(name); if (location) this.gl.uniform4f(location, a, b, c, d); }
    setRect(name, rect) { this.set4(name, rect.x, rect.y, rect.width, rect.height); }
    upload(key, source, width, height, nearest) {  // configureTexture + mipmap: clamped, trilinear when minified; magnified linear for plates, point-sampled for a raw frame
      const gl = this.gl, texture = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, texture);
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false); gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false); gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
      gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.NONE);  // the stored values, as stb_image reads them: an embedded Adobe RGB profile is ignored, as the renderer, RetroArch and Mega Bezel ignore it
      if (source instanceof Uint8Array) gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, source);
      else gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, gl.RGBA, gl.UNSIGNED_BYTE, source);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, nearest ? gl.NEAREST : gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
      gl.generateMipmap(gl.TEXTURE_2D);
      const entry = { texture, width: width || source.width, height: height || source.height };
      if (key !== null) { if (this.textures.has(key)) gl.deleteTexture(this.textures.get(key).texture); this.textures.set(key, entry); }
      return entry;
    }
    image(key, image) { return this.upload(key, image, image.naturalWidth || image.width, image.naturalHeight || image.height, false); }  // a plate: decoded here, in the same task
    frame(key, pixels, width, height) { return this.textures.get(key) || this.upload(key, pixels, width, height, true); }  // a native frame, as extract() leaves it
    forget(prefix) { for (const [key, entry] of [...this.textures]) if (key.startsWith(prefix)) { this.gl.deleteTexture(entry.texture); this.textures.delete(key); } }
    magnify(nearest) {  // the editor's own choice above 1x: plates point-sampled for pixel work (the renderer never magnifies them)
      const gl = this.gl;
      for (const [key, entry] of this.textures) if (key.startsWith("layer:")) { gl.bindTexture(gl.TEXTURE_2D, entry.texture); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, nearest ? gl.NEAREST : gl.LINEAR); }
    }
    bind(unit, entry) { const gl = this.gl; gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, (entry || this.black).texture); }
    laneUniforms(screen, lane, canvas, framed, index) {  // RendererCompositor.laneUniforms
      const suffix = index === 0 ? "" : "2", look = !!screen && screen.lookSet;
      let ring = look && screen.ringSet && canvas.width > 0, inner = CompositionContract.empty(), outer = CompositionContract.empty();
      if (ring) { inner = CompositionContract.aperture(canvas, screen.ringInner); outer = CompositionContract.aperture(canvas, screen.ringOuter); if (outer.width <= 0 || outer.height <= 0) ring = false; }
      const value = (name, fallback) => screen && typeof screen[name] === "number" ? screen[name] : fallback;
      this.setRect("uRingIn" + suffix, inner); this.setRect("uRingOut" + suffix, outer);
      this.set4("uRingLook" + suffix, value("ringInnerRadius", 0), value("ringOuterRadius", 0), ring ? 1 : 0, value("ringBevel", 0));
      const color = screen && screen.ringColor ? screen.ringColor : [0, 0, 0];
      this.set4("uRingColor" + suffix, color[0], color[1], color[2], value("ringOpacity", 0));
      this.set4("uRingReflect" + suffix, ring || (look && framed === 1) ? value("reflect", 0) : 0, value("reflectBlur", 0), value("reflectFade", 0), 0);
      this.setRect("uRect" + suffix, lane ? lane.output : CompositionContract.empty()); this.setRect("uTube" + suffix, lane ? lane.tube : CompositionContract.empty());
      this.set4("uShape" + suffix, look ? screen.shape : 0, look ? screen.radius : 0, look ? screen.exponent : 2, 0);
      this.set4("uFx" + suffix, look ? screen.curvature : 0, look ? screen.vignette : 0, look ? screen.bloom : 0, 0);
      const surround = look ? screen.surround : [0, 0, 0];
      this.set4("uSurround" + suffix, surround[0], surround[1], surround[2], 1);
    }
    pass(pass, blend) {  // drawPass
      const gl = this.gl;
      if (blend) { gl.enable(gl.BLEND); gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA); gl.blendEquation(gl.FUNC_ADD); }
      else gl.disable(gl.BLEND);
      gl.uniform1f(this.uniform("uPass"), pass);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
    }
    scissor(rect) {  // the target screen on the view: what the renderer draws over its whole framebuffer stays inside it
      const gl = this.gl;
      if (!rect) { gl.disable(gl.SCISSOR_TEST); return; }
      const left = Math.max(0, Math.floor(rect.x)), bottom = Math.max(0, Math.floor(rect.y));
      gl.enable(gl.SCISSOR_TEST); gl.scissor(left, bottom, Math.max(0, Math.ceil(rect.x + rect.width) - left), Math.max(0, Math.ceil(rect.y + rect.height) - bottom));
    }
    cover(screen, imageWidth, imageHeight) {  // RendererGeometry.background and drawLayers' cover branch, over the target screen
      const horizontal = float(screen.width / imageWidth), vertical = float(screen.height / imageHeight), scale = horizontal > vertical ? horizontal : vertical;
      const width = float(scale * imageWidth), height = float(scale * imageHeight);
      return { x: float(screen.x + float(float(screen.width - width) * 0.5)), y: float(screen.y + float(float(screen.height - height) * 0.5)), width, height };
    }
    layers(variant, canvas, screen, above) {  // RendererCompositor.drawLayers
      const gl = this.gl;
      for (const layer of variant.layers) {
        const entry = this.textures.get("layer:" + layer.id);
        if (layer.above !== above || !entry || layer.opacity <= 0) continue;
        const rect = layer.extent === "cover" ? this.cover(screen, entry.width, entry.height) : CompositionContract.aperture(canvas, layer.extent);
        if (rect.width <= 0 || rect.height <= 0) continue;
        this.scissor(layer.extent === "cover" ? screen : null);
        this.bind(1, entry);
        this.setRect("uBezelRect", rect);
        this.set4("uLayer", layer.opacity, layer.blend, layer.lift, 0);
        this.set4("uLayerTint", layer.tint[0], layer.tint[1], layer.tint[2], layer.tint[3]);
        gl.enable(gl.BLEND);
        if (layer.blend === 1) gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE, gl.ZERO, gl.ONE);  // add: black adds nothing
        else if (layer.blend === 2) gl.blendFuncSeparate(gl.DST_COLOR, gl.ONE_MINUS_SRC_ALPHA, gl.ZERO, gl.ONE);  // multiply
        else gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
        gl.blendEquation(gl.FUNC_ADD);
        gl.uniform1f(this.uniform("uPass"), 4);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
      }
      this.scissor(null);
      this.setRect("uBezelRect", canvas);
    }
    paintedLens(variant, count) { return variant.screens.slice(0, Math.min(count, 2)).some(screen => screen && screen.lookSet && screen.ringSet && screen.ringOpacity <= 0 && screen.reflect > 0); }
    render(scene) {  // scene: width, height, clear, screen and canvas (bottom-up), variant, lanes, frames[] and glass[] texture keys, cutouts
      const gl = this.gl, { variant, lanes, canvas, screen } = scene, count = lanes.length, second = count > 1 ? 1 : 0;
      if (this.canvas.width !== scene.width || this.canvas.height !== scene.height) { this.canvas.width = scene.width; this.canvas.height = scene.height; }
      gl.viewport(0, 0, scene.width, scene.height);
      gl.disable(gl.DEPTH_TEST); gl.disable(gl.CULL_FACE); gl.disable(gl.STENCIL_TEST); gl.disable(gl.SCISSOR_TEST); gl.colorMask(true, true, true, true);
      gl.clearColor(scene.clear[0], scene.clear[1], scene.clear[2], 1); gl.clear(gl.COLOR_BUFFER_BIT);
      gl.useProgram(this.program); gl.bindVertexArray(this.vertexArray);
      const frames = scene.frames.map(key => this.textures.get(key)), glass = scene.glass.map(key => key && this.textures.get(key));
      this.bind(0, frames[0]); this.bind(4, frames[second]); this.bind(1, null); this.bind(2, glass[0]); this.bind(5, glass[second]);
      const background = scene.background ? this.textures.get(scene.background) : null;
      this.bind(6, background); this.bind(7, frames[0]); this.bind(8, frames[second]); this.bind(3, null);
      const framed = scene.framed || 0;
      for (let index = 0; index < 2; index++) { const lane = index < count ? index : second; this.laneUniforms(variant.screens[lane], lanes[lane], canvas, framed, index); }
      this.set4("uRotation", 0, 0, 0, 0);
      const glass0 = !!glass[0] && !!variant.screens[0] && variant.screens[0].lookSet, glass1 = count > 1 && !!glass[1] && !!variant.screens[1] && variant.screens[1].lookSet;
      this.set4("uReflect", glass0 ? variant.screens[0].glassReflect : 0, glass1 ? variant.screens[1].glassReflect : 0, glass0 ? 1 : 0, glass1 ? 1 : 0);
      this.setRect("uBezelRect", canvas);
      this.setRect("uBackgroundRect", background ? this.cover(screen, background.width, background.height) : CompositionContract.empty());
      this.set4("uFlags", count > 1 ? 1 : 0, 0, background ? 1 : 0, variant.layered ? 1 : 0);
      this.set4("uFrame", scene.frameWidth || 0, framed, 0, 0);
      this.set4("uFrameColor", variant.frame.color[0], variant.frame.color[1], variant.frame.color[2], 1);
      const bulge = index => variant.screens[index] ? [variant.screens[index].bulgeX, variant.screens[index].bulgeY] : [0, 0];
      this.set4("uBulge", ...bulge(0), ...bulge(second));
      gl.uniform1f(this.uniform("uMenuOn"), 0);
      this.scissor(screen); this.pass(0, false); this.scissor(null);
      if (variant.layered) this.layers(variant, canvas, screen, false);
      if (variant.layered && this.paintedLens(variant, count) && scene.cutouts) {  // drawLensMirror: screen blend, the plate stays and the mirror lightens it
        gl.enable(gl.BLEND); gl.blendFuncSeparate(gl.ONE, gl.ONE_MINUS_SRC_COLOR, gl.ZERO, gl.ONE); gl.blendEquation(gl.FUNC_ADD);
        gl.uniform1f(this.uniform("uPass"), 5); gl.drawArrays(gl.TRIANGLES, 0, 3);
      }
      if (framed) this.pass(2, true);  // plate and frames first, so the screen edge blends over them
      if (scene.cutouts) this.pass(1, true);  // alpha carries the screen shape mask
      if (variant.layered) this.layers(variant, canvas, screen, true);
      gl.disable(gl.BLEND);
      return this.canvas;
    }
  }

  return { CompositionContract, Environment, Geometry, TestCard, Compositor };
})();
