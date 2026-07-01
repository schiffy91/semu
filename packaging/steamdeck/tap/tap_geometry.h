#ifndef SEMU_TAP_GEOMETRY_H
#define SEMU_TAP_GEOMETRY_H

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

    float art_aspect = (float)in->art_w / (float)in->art_h;
    if ((float)in->win_w / (float)in->win_h > art_aspect) {
        out->bezel_h = (float)in->win_h;
        out->bezel_w = (float)in->win_h * art_aspect;
    } else {
        out->bezel_w = (float)in->win_w;
        out->bezel_h = (float)in->win_w / art_aspect;
    }

    float bezel_left = ((float)in->win_w - out->bezel_w) * 0.5f;
    float bezel_top = ((float)in->win_h - out->bezel_h) * 0.5f;
    float hole_left = bezel_left + hole_x * out->bezel_w;
    float hole_top = bezel_top + hole_y * out->bezel_h;
    float hole_px_w = hole_w * out->bezel_w;
    float hole_px_h = hole_h * out->bezel_h;

    if (in->fill_hole) {
        if (hole_px_w / hole_px_h > aspect) {
            out->game_h = semu_tap_round_px(hole_px_h);
            out->game_w = semu_tap_round_px((float)out->game_h * aspect);
        } else {
            out->game_w = semu_tap_round_px(hole_px_w);
            out->game_h = semu_tap_round_px((float)out->game_w / aspect);
        }
    } else {
        int scale = (int)(hole_px_h / (float)in->native_h);
        if (scale < 1) { scale = 1; }
        out->game_h = scale * in->native_h;
        out->game_w = semu_tap_round_px((float)out->game_h * aspect);
        if ((float)out->game_w > hole_px_w) {
            int scale2 = (int)(hole_px_w / ((float)in->native_h * aspect));
            if (scale2 < 1) { scale2 = 1; }
            scale = scale2;
            out->game_h = scale * in->native_h;
            out->game_w = semu_tap_round_px((float)out->game_h * aspect);
        }
    }

    float game_left = hole_left + (hole_px_w - (float)out->game_w) * 0.5f;
    float game_top = hole_top + (hole_px_h - (float)out->game_h) * 0.5f;
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
