#ifndef SEMU_TAP_GEOMETRY_H
#define SEMU_TAP_GEOMETRY_H

/* Display priority placement (the user's spec, verbatim):
 *   GAME priority  - the game takes the largest integer scale that fits the
 *                    window; the bezel is scaled so its hole lands exactly on
 *                    the game and may be cut off by the window edges. When
 *                    even 2x does not fit (640x480-class systems on a
 *                    1280x800 deck), integer scaling would waste half the
 *                    screen, so the game falls back to an aspect-preserving
 *                    fill - fractional is visually lossless on 3D-era output.
 *   BEZEL priority - the game still takes an integer scale; the bezel is
 *                    scaled around it (hole == game, always flush) as large
 *                    as possible WITHOUT being cut off. k is maximized under
 *                    that constraint. If not even 1x keeps the bezel inside
 *                    the window, the bezel contain-fits and the game fills
 *                    the hole fractionally (flush beats slack).
 * The preview projector (tests/targets/macos/priority_matrix.sh) ports this
 * file line for line; change both together. */

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

    int use_bezel_priority = in->priority_bezel && in->has_art && in->art_w > 0 && in->art_h > 0;
    out->bezel_priority = use_bezel_priority ? 1 : 0;

    if (!use_bezel_priority) {
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
        if (scale < 2) {
            /* integer scaling is degenerate here: fill the window instead */
            out->game_h = in->win_h;
            out->game_w = semu_tap_round_px((float)in->win_h * aspect);
            if (out->game_w > in->win_w) {
                out->game_w = in->win_w;
                out->game_h = semu_tap_round_px((float)in->win_w / aspect);
            }
        }
        out->game_x = (in->win_w - out->game_w) / 2;
        out->game_y = (in->win_h - out->game_h) / 2;
        out->bezel_w = (float)out->game_w / hole_w;
        out->bezel_h = (float)out->game_h / hole_h;
        float game_top = (float)(in->win_h - out->game_y - out->game_h);
        float bezel_left = (float)out->game_x - hole_x * out->bezel_w;
        float bezel_top = game_top - hole_y * out->bezel_h;
        out->bezel_x = bezel_left;
        out->bezel_y = (float)in->win_h - bezel_top - out->bezel_h;
        return 1;
    }

    /* BEZEL priority: game at integer scale k, bezel scaled so hole == game
     * (flush by construction); maximize k while the bezel stays fully inside
     * the window. */
    int scale = 0;
    int candidate = 1;
    for (;;) {
        float game_h = (float)(candidate * in->native_h);
        float game_w = (float)semu_tap_round_px(game_h * aspect);
        float bezel_w = game_w / hole_w;
        float bezel_h = game_h / hole_h;
        /* allow <=1% overflow: a few-pixel art sliver off-screen is invisible,
         * losing integer scaling to it is not (wii: 1x needs a 1288px TV) */
        if (bezel_w <= (float)in->win_w * 1.01f + 0.5f && bezel_h <= (float)in->win_h * 1.01f + 0.5f) {
            scale = candidate;
            candidate += 1;
        } else {
            break;
        }
    }

    if (scale >= 1 && !in->fill_hole) {
        out->game_h = scale * in->native_h;
        out->game_w = semu_tap_round_px((float)out->game_h * aspect);
    } else {
        /* not even 1x keeps the bezel uncut (or explicit fill requested):
         * contain-fit the art and fill its hole exactly - flush, fractional */
        float art_aspect = (float)in->art_w / (float)in->art_h;
        float contain_w;
        float contain_h;
        if ((float)in->win_w / (float)in->win_h > art_aspect) {
            contain_h = (float)in->win_h;
            contain_w = contain_h * art_aspect;
        } else {
            contain_w = (float)in->win_w;
            contain_h = contain_w / art_aspect;
        }
        float hole_px_w = hole_w * contain_w;
        float hole_px_h = hole_h * contain_h;
        if (hole_px_w / hole_px_h > aspect) {
            out->game_h = semu_tap_round_px(hole_px_h);
            out->game_w = semu_tap_round_px((float)out->game_h * aspect);
        } else {
            out->game_w = semu_tap_round_px(hole_px_w);
            out->game_h = semu_tap_round_px((float)out->game_w / aspect);
        }
    }

    out->bezel_w = (float)out->game_w / hole_w;
    out->bezel_h = (float)out->game_h / hole_h;
    float bezel_left = ((float)in->win_w - out->bezel_w) * 0.5f;
    float bezel_top = ((float)in->win_h - out->bezel_h) * 0.5f;
    float game_left = bezel_left + hole_x * out->bezel_w;
    float game_top = bezel_top + hole_y * out->bezel_h;
    out->game_x = semu_tap_round_px(game_left);
    out->game_y = semu_tap_round_px((float)in->win_h - game_top - (float)out->game_h);
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
