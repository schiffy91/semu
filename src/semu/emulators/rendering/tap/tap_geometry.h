#ifndef SEMU_TAP_GEOMETRY_H
#define SEMU_TAP_GEOMETRY_H

/* Display priority placement (user spec):
 *   GAME priority  - largest integer scale that fits the window (falling back
 *                    to an aspect-preserving fill when even 2x does not fit -
 *                    fractional is visually lossless on 3D-era output); the
 *                    bezel follows the game and may be cut off.
 *   BEZEL priority - the game still takes an integer scale; the bezel is
 *                    grown around it as large as possible WITHOUT being cut
 *                    off (<=1% overflow tolerated).
 *   FIT            - non-integer: with art, the bezel contain-fits the window
 *                    and the game aspect-fits inside the hole; without art,
 *                    the game aspect-fills the window. (fill_hole == fit)
 * The bezel is always scaled UNIFORMLY (art aspect preserved) so the hole
 * CONTAINS the game: flush on the limiting axis, any remainder shows the
 * art's own glass/letterbox (authentic for handheld lenses, sub-pixel for
 * TVs whose painted tubes match the display aspect). The preview projector
 * (tests/targets/macos/priority_matrix.sh) ports this file line for line;
 * change both together. */

typedef struct SemuTapGeometryInput {
    int win_w;
    int win_h;
    int native_w;
    int native_h;
    float display_aspect;
    int priority_bezel;
    int fill_hole;
    int has_art;
    int art_w;
    int art_h;
    float hole_x;
    float hole_y;
    float hole_w;
    float hole_h;
} SemuTapGeometryInput;

typedef struct SemuTapGeometry {
    int game_x;
    int game_y;
    int game_w;
    int game_h;
    float bezel_x;
    float bezel_y;
    float bezel_w;
    float bezel_h;
    int bezel_priority;
} SemuTapGeometry;

static int semu_tap_round_px(float value) {
    return (int)(value + 0.5f);
}

/* Uniform art scale such that the hole contains the game exactly on the
 * limiting axis. */
static float semu_tap_art_scale_for_game(
    const SemuTapGeometryInput *in, float hole_w, float hole_h,
    float game_w, float game_h
) {
    float hole_px_w = hole_w * (float)in->art_w;
    float hole_px_h = hole_h * (float)in->art_h;
    float scale_w = game_w / hole_px_w;
    float scale_h = game_h / hole_px_h;
    return scale_w > scale_h ? scale_w : scale_h;
}

static int semu_tap_compute_geometry(const SemuTapGeometryInput *in, SemuTapGeometry *out) {
    if (!in || !out || in->win_w <= 0 || in->win_h <= 0 || in->native_w <= 0 || in->native_h <= 0) {
        return 0;
    }

    float aspect = in->display_aspect > 0.01f
        ? in->display_aspect
        : (float)in->native_w / (float)in->native_h;
    float hole_x = in->hole_x;
    float hole_y = in->hole_y;
    float hole_w = in->hole_w;
    float hole_h = in->hole_h;
    if (hole_w <= 0.001f) { hole_w = 1.0f; }
    if (hole_h <= 0.001f) { hole_h = 1.0f; }

    int has_art = in->has_art && in->art_w > 0 && in->art_h > 0;
    int use_bezel_priority = in->priority_bezel && has_art;
    out->bezel_priority = use_bezel_priority ? 1 : 0;

    float art_scale = 0.0f;

    if (!use_bezel_priority) {
        /* GAME priority (also FIT without art when fill_hole is set) */
        int scale = in->win_h / in->native_h;
        if (scale < 1) { scale = 1; }
        out->game_h = scale * in->native_h;
        out->game_w = semu_tap_round_px((float)out->game_h * aspect);
        if (out->game_w > in->win_w) {
            int scale2 = (int)((float)in->win_w / ((float)in->native_h * aspect));
            if (scale2 < 1) { scale2 = 1; }
            scale = scale2;
            out->game_h = scale * in->native_h;
            out->game_w = semu_tap_round_px((float)out->game_h * aspect);
        }
        if (scale < 2 || in->fill_hole) {
            /* degenerate integer (640x480-class) or explicit FIT: fill */
            out->game_h = in->win_h;
            out->game_w = semu_tap_round_px((float)in->win_h * aspect);
            if (out->game_w > in->win_w) {
                out->game_w = in->win_w;
                out->game_h = semu_tap_round_px((float)in->win_w / aspect);
            }
        }
        if (has_art) {
            art_scale = semu_tap_art_scale_for_game(in, hole_w, hole_h,
                                                    (float)out->game_w, (float)out->game_h);
        }
    } else if (!in->fill_hole) {
        /* BEZEL priority: maximize integer k with the (uniformly scaled)
         * bezel uncut; <=1% overflow tolerated - a sliver of off-screen art
         * beats losing integer scaling. */
        int scale = 0;
        int candidate = 1;
        for (;;) {
            float game_h = (float)(candidate * in->native_h);
            float game_w = (float)semu_tap_round_px(game_h * aspect);
            float trial = semu_tap_art_scale_for_game(in, hole_w, hole_h, game_w, game_h);
            if (trial * (float)in->art_w <= (float)in->win_w * 1.01f + 0.5f
                && trial * (float)in->art_h <= (float)in->win_h * 1.01f + 0.5f) {
                scale = candidate;
                candidate += 1;
            } else {
                break;
            }
        }
        if (scale >= 1) {
            out->game_h = scale * in->native_h;
            out->game_w = semu_tap_round_px((float)out->game_h * aspect);
            art_scale = semu_tap_art_scale_for_game(in, hole_w, hole_h,
                                                    (float)out->game_w, (float)out->game_h);
        }
        /* scale == 0 falls through to the FIT construction below */
    }

    if (use_bezel_priority && art_scale <= 0.0f) {
        /* FIT with art (or bezel priority where not even 1x stays uncut):
         * contain-fit the art, aspect-fit the game inside the hole. */
        float contain_w = (float)in->win_w / (float)in->art_w;
        float contain_h = (float)in->win_h / (float)in->art_h;
        art_scale = contain_w < contain_h ? contain_w : contain_h;
        float hole_px_w = hole_w * (float)in->art_w * art_scale;
        float hole_px_h = hole_h * (float)in->art_h * art_scale;
        if (hole_px_w / hole_px_h > aspect) {
            out->game_h = semu_tap_round_px(hole_px_h);
            out->game_w = semu_tap_round_px((float)out->game_h * aspect);
        } else {
            out->game_w = semu_tap_round_px(hole_px_w);
            out->game_h = semu_tap_round_px((float)out->game_w / aspect);
        }
    }

    if (has_art) {
        out->bezel_w = art_scale * (float)in->art_w;
        out->bezel_h = art_scale * (float)in->art_h;
    } else {
        out->bezel_w = 0.0f;
        out->bezel_h = 0.0f;
    }

    if (!use_bezel_priority) {
        /* game centered on the window; art positioned so the hole center
         * lands on the game center (cut off as needed) */
        out->game_x = (in->win_w - out->game_w) / 2;
        out->game_y = (in->win_h - out->game_h) / 2;
        if (has_art) {
            float game_center_x = (float)out->game_x + (float)out->game_w * 0.5f;
            float game_center_y_top = (float)out->game_y + (float)out->game_h * 0.5f;
            float bezel_left = game_center_x - (hole_x + hole_w * 0.5f) * out->bezel_w;
            float bezel_top = game_center_y_top - (hole_y + hole_h * 0.5f) * out->bezel_h;
            out->bezel_x = bezel_left;
            out->bezel_y = (float)in->win_h - bezel_top - out->bezel_h;
        } else {
            out->bezel_x = 0.0f;
            out->bezel_y = 0.0f;
        }
        return 1;
    }

    /* bezel centered on the window; game centered in the hole */
    float bezel_left = ((float)in->win_w - out->bezel_w) * 0.5f;
    float bezel_top = ((float)in->win_h - out->bezel_h) * 0.5f;
    float hole_left = bezel_left + hole_x * out->bezel_w;
    float hole_top = bezel_top + hole_y * out->bezel_h;
    float hole_px_w = hole_w * out->bezel_w;
    float hole_px_h = hole_h * out->bezel_h;
    out->game_x = semu_tap_round_px(hole_left + (hole_px_w - (float)out->game_w) * 0.5f);
    out->game_y = semu_tap_round_px(
        (float)in->win_h - (hole_top + (hole_px_h - (float)out->game_h) * 0.5f) - (float)out->game_h);
    out->bezel_x = bezel_left;
    out->bezel_y = (float)in->win_h - bezel_top - out->bezel_h;
    return 1;
}

static void semu_tap_hole_rect_gl(
    const SemuTapGeometryInput *in,
    const SemuTapGeometry *geometry,
    float *x,
    float *y,
    float *w,
    float *h
) {
    *x = geometry->bezel_x + in->hole_x * geometry->bezel_w;
    *y = geometry->bezel_y + (1.0f - in->hole_y - in->hole_h) * geometry->bezel_h;
    *w = in->hole_w * geometry->bezel_w;
    *h = in->hole_h * geometry->bezel_h;
}

#endif
