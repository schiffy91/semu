// macos_overlay.m — Semu compositor, macOS overlay-window back end.
//
// macOS emulators present via Metal, so the linux GL tap (libsemutap.c,
// LD_PRELOAD at the swap boundary) has no injection-free darwin equivalent.
// This is the injection-free answer: a standalone process that floats a
// transparent, click-through, non-activating NSWindow above the emulator's
// window and draws the bezel art around the computed content rect. The
// emulator process is never touched — Metal, OpenGL and software presents
// all look identical from up here.
//
// Geometry is the SHARED contract (tap_geometry.h): largest-integer game rect
// (or hole-fill / bezel-priority, same env grammar libsemutap.c reads), bezel
// art mapped by its screen hole, hole cleared back to transparency so the
// emulator content below shows through untouched.
//
// Env (names + grammar identical to the getenv calls in libsemutap.c):
//   SEMU_TAP_NATIVE_W/H  integer-scale base (per system)
//   SEMU_TAP_ASPECT      "W:H" or a float; 0/absent = native ratio
//   SEMU_TAP_PRIORITY    leading 'b'/'B' selects bezel-priority mapping
//   SEMU_TAP_FILL        '1' = fill the art's screen hole at display aspect
//   SEMU_TAP_ART         bezel PNG path
//   SEMU_TAP_SCREEN      "x,y,w,h" normalized top-left screen hole in the art
//   SEMU_TAP_DISABLE     non-'0' first char = do nothing
// plus the overlay-only exec-edge key set by the macos launch wrapper
// (platforms/macos/macos_tap.btrc):
//   SEMU_TAP_TARGET_PID  the emulator process id to shadow.
// Diagnostics (mirrors the GL tap's SEMU_TAP_DEBUG/ALIGN affordances):
//   SEMU_TAP_SNAPSHOT    write one PNG of the composited overlay content
//                        after the first shadowed frame — runtime proof that
//                        works without the Screen Recording TCC grant an
//                        external window capture would require.
//
// Lifecycle: poll CGWindowListCopyWindowInfo for the target pid's largest
// layer-0 window, mirror that frame (Quartz top-left global coords -> Cocoa
// bottom-left), hide while the emulator has no on-screen window, exit when
// the pid dies.

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
// Vendored stb: the PNG-only build leaves two overflow helpers unused.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "stb_image.h"
#pragma clang diagnostic pop
#include "tap_geometry.h"

static pid_t target_pid;
static int env_nw, env_nh;                       // SEMU_TAP_NATIVE_W/H
static float disp_aspect;                        // display aspect (0 = native square nw/nh)
static int priority_bezel, fill_hole;            // SEMU_TAP_PRIORITY / SEMU_TAP_FILL
static float scr_x, scr_y, scr_w, scr_h;         // the art's screen HOLE (norm, top-left)
static CGImageRef art_image;                     // bezel art, loaded once via stb
static int art_w, art_h;
static NSWindow *overlay_window;
static const NSTimeInterval poll_interval = 0.25;
static const CGFloat minimum_window_side = 64.0; // ignore tooltips/utility slivers
static char snapshot_path[1024];                 // SEMU_TAP_SNAPSHOT diagnostic
static int snapshot_written;

// ---- env (grammar mirrors libsemutap.c exactly) ----

static void read_environment(void) {
    const char *enw = getenv("SEMU_TAP_NATIVE_W"); if (enw && atoi(enw) > 0) env_nw = atoi(enw);
    const char *enh = getenv("SEMU_TAP_NATIVE_H"); if (enh && atoi(enh) > 0) env_nh = atoi(enh);
    const char *pri = getenv("SEMU_TAP_PRIORITY"); if (pri && (pri[0] == 'b' || pri[0] == 'B')) priority_bezel = 1;
    const char *fh = getenv("SEMU_TAP_FILL"); if (fh && fh[0] == '1') fill_hole = 1;
    const char *sc = getenv("SEMU_TAP_SCREEN"); if (sc) { sscanf(sc, "%f,%f,%f,%f", &scr_x, &scr_y, &scr_w, &scr_h); }
    const char *as = getenv("SEMU_TAP_ASPECT");
    if (as) {
        float aw = 0, ah = 0;
        if (sscanf(as, "%f:%f", &aw, &ah) == 2 && ah > 0.0f) disp_aspect = aw / ah;
        else { float v = (float)atof(as); if (v > 0.01f) disp_aspect = v; }
    }
    const char *tp = getenv("SEMU_TAP_TARGET_PID"); if (tp) target_pid = (pid_t)atoi(tp);
    const char *sp = getenv("SEMU_TAP_SNAPSHOT");
    if (sp && sp[0]) { strncpy(snapshot_path, sp, sizeof(snapshot_path) - 1); snapshot_path[sizeof(snapshot_path) - 1] = 0; }
}

// ---- bezel art: stb PNG -> CGImage (non-premultiplied RGBA, stb's layout) ----

static void release_art_pixels(void *info, const void *data, size_t size) {
    (void)info; (void)size;
    stbi_image_free((void *)data);
}

static CGImageRef load_art_image(const char *path) {
    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = stbi_load(path, &width, &height, &channels, 4);
    if (!pixels) { return NULL; }
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL, pixels, (size_t)width * (size_t)height * 4, release_art_pixels);
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(
        (size_t)width, (size_t)height, 8, 32, (size_t)width * 4, color_space,
        (CGBitmapInfo)kCGImageAlphaLast, provider, NULL, true, kCGRenderingIntentDefault);
    CGColorSpaceRelease(color_space);
    CGDataProviderRelease(provider);
    if (image) { art_w = width; art_h = height; }
    return image;
}

// ---- the overlay view: bezel art per the shared geometry, hole cleared ----

@interface SemuOverlayView : NSView
@end

@implementation SemuOverlayView

// Belt + braces on top of ignoresMouseEvents: never participate in hit tests.
- (NSView *)hitTest:(NSPoint)point {
    (void)point;
    return nil;
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    CGContextRef context = [NSGraphicsContext currentContext].CGContext;
    CGContextClearRect(context, NSRectToCGRect(self.bounds));
    if (!art_image) { return; }

    // The window frame is ALREADY the art's geometry: poll_target expands the
    // overlay around the emulator window by the hole fractions, so the art
    // stretches to the full bounds and the hole sits at exactly those
    // fractions. (SEMU_TAP_SCREEN is top-left normalized; this view is
    // bottom-left, so the hole's vertical origin is 1 - y - h.)
    CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
    CGContextDrawImage(context, NSRectToCGRect(self.bounds), art_image);

    // The content cutout: clear the hole back to full transparency — the
    // emulator's own window shows through untouched.
    CGFloat bounds_width = self.bounds.size.width;
    CGFloat bounds_height = self.bounds.size.height;
    CGContextClearRect(context, CGRectMake(bounds_width * scr_x,
        bounds_height * (1.0f - scr_y - scr_h),
        bounds_width * scr_w, bounds_height * scr_h));
}

@end

// ---- target-window tracking ----

// Largest on-screen layer-0 window owned by the target pid, in Quartz global
// coordinates (top-left origin). Returns 0 when the pid has none visible.
static int copy_target_window_bounds(CGRect *out_bounds) {
    CFArrayRef window_list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!window_list) { return 0; }
    int found = 0;
    CGFloat best_area = 0;
    CFIndex window_count = CFArrayGetCount(window_list);
    for (CFIndex window_index = 0; window_index < window_count; window_index++) {
        CFDictionaryRef window_info = CFArrayGetValueAtIndex(window_list, window_index);
        CFNumberRef owner_ref = CFDictionaryGetValue(window_info, kCGWindowOwnerPID);
        int owner_pid = 0;
        if (!owner_ref || !CFNumberGetValue(owner_ref, kCFNumberIntType, &owner_pid)
            || owner_pid != (int)target_pid) { continue; }
        CFNumberRef layer_ref = CFDictionaryGetValue(window_info, kCGWindowLayer);
        int window_layer = 0;
        if (layer_ref) { CFNumberGetValue(layer_ref, kCFNumberIntType, &window_layer); }
        if (window_layer != 0) { continue; }   // menus, status items, popovers
        CFDictionaryRef bounds_ref = CFDictionaryGetValue(window_info, kCGWindowBounds);
        CGRect bounds = CGRectZero;
        if (!bounds_ref || !CGRectMakeWithDictionaryRepresentation(bounds_ref, &bounds)) {
            continue;
        }
        if (bounds.size.width < minimum_window_side
            || bounds.size.height < minimum_window_side) { continue; }
        CGFloat area = bounds.size.width * bounds.size.height;
        if (area > best_area) { best_area = area; *out_bounds = bounds; found = 1; }
    }
    CFRelease(window_list);
    return found;
}

// Quartz global coords are top-left-origin with the primary display as the
// reference; Cocoa is bottom-left over the same reference, so only y flips.
static NSRect cocoa_frame_from_quartz(CGRect quartz_bounds) {
    CGFloat primary_height = CGDisplayBounds(CGMainDisplayID()).size.height;
    return NSMakeRect(quartz_bounds.origin.x,
        primary_height - quartz_bounds.origin.y - quartz_bounds.size.height,
        quartz_bounds.size.width, quartz_bounds.size.height);
}

// SEMU_TAP_SNAPSHOT diagnostic: re-render the live content view (the same
// drawRect the screen shows) into a bitmap and write it as PNG, once.
static void write_snapshot_when_requested(void) {
    if (!snapshot_path[0] || snapshot_written) { return; }
    if (!overlay_window.visible) {
        fprintf(stderr, "semu-overlay: snapshot deferred (window not visible)\n");
        return;
    }
    NSView *content_view = overlay_window.contentView;
    NSBitmapImageRep *bitmap =
        [content_view bitmapImageRepForCachingDisplayInRect:content_view.bounds];
    if (!bitmap) {
        fprintf(stderr, "semu-overlay: snapshot failed (no caching bitmap)\n");
        return;
    }
    [content_view cacheDisplayInRect:content_view.bounds toBitmapImageRep:bitmap];
    NSData *png_data = [bitmap representationUsingType:NSBitmapImageFileTypePNG
                                            properties:@{}];
    if (!png_data) {
        fprintf(stderr, "semu-overlay: snapshot failed (no png encoding)\n");
        return;
    }
    if ([png_data writeToFile:[NSString stringWithUTF8String:snapshot_path]
                   atomically:YES]) {
        snapshot_written = 1;
        fprintf(stderr, "semu-overlay: wrote snapshot %s\n", snapshot_path);
    } else {
        fprintf(stderr, "semu-overlay: snapshot failed (write %s)\n", snapshot_path);
    }
}

static void poll_target(void) {
    if (kill(target_pid, 0) != 0 && errno == ESRCH) {
        fprintf(stderr, "semu-overlay: target pid %d gone, exiting\n", (int)target_pid);
        exit(0);
    }
    CGRect quartz_bounds = CGRectZero;
    if (!copy_target_window_bounds(&quartz_bounds)) {
        [overlay_window orderOut:nil];   // emulator minimized/hidden: follow it
        return;
    }
    // Only over the GAME: when the emulator's app is not frontmost (the user
    // switched to another window, or the emulator shows a settings dialog in
    // front), the bezel hides instead of floating over foreign windows.
    NSRunningApplication *frontmost_application =
        [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (frontmost_application == nil
        || (pid_t)frontmost_application.processIdentifier != target_pid) {
        [overlay_window orderOut:nil];
        return;
    }
    NSRect target_frame = cocoa_frame_from_quartz(quartz_bounds);
    // The overlay cannot rescale the emulator's pixels the way the GL tap
    // does, so the bezel lives OUTSIDE the emulator window: expand the frame
    // by the art's hole fractions (SEMU_TAP_SCREEN) so the transparent hole
    // lands exactly on the window and the art surrounds it. Mirroring 1:1
    // left the hole covering the whole frame — observed live as a bezel
    // reduced to thin edges. Recomputed every poll, so window resizes and
    // moves track dynamically. Clamped to the visible screen; the hole stays
    // glued to the window even when the ring crops at an edge.
    if (scr_w > 0.05f && scr_h > 0.05f && scr_w < 1.0f && scr_h < 1.0f) {
        CGFloat expanded_width = target_frame.size.width / scr_w;
        CGFloat expanded_height = target_frame.size.height / scr_h;
        CGFloat hole_left = expanded_width * scr_x;
        // SEMU_TAP_SCREEN is top-left normalized; Cocoa origins are
        // bottom-left, so the offset below the hole is (1 - y - h).
        CGFloat hole_bottom = expanded_height * (1.0f - scr_y - scr_h);
        target_frame = NSMakeRect(target_frame.origin.x - hole_left,
            target_frame.origin.y - hole_bottom, expanded_width, expanded_height);
        target_frame = NSIntersectionRect(target_frame,
            [[NSScreen mainScreen] visibleFrame]);
    }
    if (!NSEqualRects(overlay_window.frame, target_frame)) {
        [overlay_window setFrame:target_frame display:YES];
        [overlay_window.contentView setNeedsDisplay:YES];
    }
    if (!overlay_window.visible) { [overlay_window orderFrontRegardless]; }
    write_snapshot_when_requested();
}

int main(void) {
    @autoreleasepool {
        const char *disable_env = getenv("SEMU_TAP_DISABLE");
        if (disable_env && disable_env[0] && disable_env[0] != '0') { return 0; }
        read_environment();
        if (target_pid <= 0) {
            fprintf(stderr, "semu-overlay: SEMU_TAP_TARGET_PID missing or invalid\n");
            return 2;
        }
        const char *art_path = getenv("SEMU_TAP_ART");
        if (!art_path || !art_path[0]) {
            fprintf(stderr, "semu-overlay: no SEMU_TAP_ART, nothing to composite\n");
            return 0;
        }
        art_image = load_art_image(art_path);
        if (!art_image) {
            fprintf(stderr, "semu-overlay: failed to load art '%s'\n", art_path);
            return 2;
        }

        NSApplication *application = [NSApplication sharedApplication];
        [application setActivationPolicy:NSApplicationActivationPolicyAccessory];

        overlay_window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, 640, 480)
                      styleMask:NSWindowStyleMaskBorderless
                        backing:NSBackingStoreBuffered
                          defer:NO];
        overlay_window.opaque = NO;
        overlay_window.backgroundColor = [NSColor clearColor];
        overlay_window.hasShadow = NO;
        overlay_window.ignoresMouseEvents = YES;   // fully click-through
        overlay_window.level = NSFloatingWindowLevel + 1;
        overlay_window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
            | NSWindowCollectionBehaviorFullScreenAuxiliary
            | NSWindowCollectionBehaviorIgnoresCycle;
        overlay_window.releasedWhenClosed = NO;
        overlay_window.contentView = [[SemuOverlayView alloc] init];

        fprintf(stderr, "semu-overlay: shadowing pid %d art %dx%d window id %ld\n",
            (int)target_pid, art_w, art_h, (long)overlay_window.windowNumber);

        poll_target();
        [NSTimer scheduledTimerWithTimeInterval:poll_interval
                                        repeats:YES
                                          block:^(NSTimer *timer) {
            (void)timer;
            poll_target();
        }];
        [application run];
    }
    return 0;
}
