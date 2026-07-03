// semu_tap.h — the Semu compositor "tap" interface. ONE contract, every emulator.
//
// We build each emulator from source. At its present point ("tap-in") the emulator
// tells us EXACTLY where the live game content sits in the output framebuffer, and
// whether content is active at all. The Semu compositor (loaded into the emulator
// process) uses ONLY this report — never pixel inspection, never black-detection,
// never centering guesses — to draw the bezel + CRT/shaders around the game, then
// presents ("tap-out"). In menus / with no content the emulator reports inactive and
// the compositor passes the frame straight through.
//
// The REPORTING side (this header + one call per frame) is graphics-API-agnostic.
// The compositor has per-API back ends: OpenGL (wrap glXSwapBuffers) and Vulkan
// (wrap vkQueuePresentKHR). Each supported emulator implements the reporting side
// exactly once, wherever it computes its output viewport.
//
//   Emulator each frame:   build SemuTapState  ->  semu_tap_report_safe(&s)  ->  present
//   Compositor present hook: read last state -> if !active pass through, else composite
//
// An un-tapped build is unaffected: semu_tap_report is resolved at runtime, so when
// the compositor isn't loaded the call is a no-op.
//
// ENV CONTRACT (SEMU_TAP_*): the per-launch presentation settings the compositor
// reads (SEMU_TAP_NATIVE_W/H, _ASPECT, _STYLE, _PRIORITY, _DUAL, _SYSKIND, _FILL,
// _REFLECT, _CURVE, _CORNER, _SHELL, _ART, _GLASS, _SCREEN, ...) are PRODUCED by
// RenderPlanner.tapEnvironment in src/semu/emulators/rendering/semu_rendering.btrc
// and CONSUMED here by libsemutap.c. That btrc class is the single source of truth
// for key names and wire formats; keep the two sides in lockstep.

#ifndef SEMU_TAP_H
#define SEMU_TAP_H

#ifdef __cplusplus
extern "C" {
#endif

#define SEMU_TAP_ABI 1

// Coordinate origin of content_x/content_y within the framebuffer.
enum {
    SEMU_TAP_ORIGIN_BOTTOM_LEFT = 0, // OpenGL convention (glViewport)
    SEMU_TAP_ORIGIN_TOP_LEFT    = 1  // Vulkan / most window-system conventions
};

typedef struct {
    int abi;          // = SEMU_TAP_ABI. Compositor ignores reports with a mismatched ABI.
    int active;       // 1 = live game content this frame; 0 = menu / no content -> pass through.

    int fb_width;     // output framebuffer size in pixels (the surface we present)
    int fb_height;

    int content_x;    // the game content rectangle inside the framebuffer, in pixels,
    int content_y;    // measured from `origin`. This is the emulator's REAL viewport —
    int content_w;    // exactly where it drew the game this frame. No guessing.
    int content_h;

    int native_w;     // source/core native resolution (e.g. 320x240). Drives scanline
    int native_h;     // density + integer-scale math in the compositor's shader.

    int rotation;     // content rotation, degrees clockwise: 0 / 90 / 180 / 270.
    int origin;       // one of SEMU_TAP_ORIGIN_*.
    int reserved[4];  // future use; zero-initialize.
} SemuTapState;

// Implemented and EXPORTED by the compositor (libsemutap). The emulator should not
// link this directly — use semu_tap_report_safe() below, which resolves it at runtime.
void semu_tap_report(const SemuTapState* state);

// ---- emulator-side convenience: resolve once, call every frame, no-op if untapped ----
#if !defined(SEMU_TAP_NO_HELPER)
#include <dlfcn.h>
static inline void semu_tap_report_safe(const SemuTapState* s) {
    static void (*fn)(const SemuTapState*);
    static int resolved;
    if (!resolved) { resolved = 1; fn = (void (*)(const SemuTapState*))dlsym(RTLD_DEFAULT, "semu_tap_report"); }
    if (fn && s) fn(s);
}
#endif

#ifdef __cplusplus
}
#endif
#endif // SEMU_TAP_H
