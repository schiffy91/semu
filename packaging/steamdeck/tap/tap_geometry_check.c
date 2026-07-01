#include <stdio.h>
#include <stdlib.h>

#include "tap_geometry.h"

static float absf_(float value) {
    return value < 0.0f ? -value : value;
}

static void fail_(const char *label) {
    fprintf(stderr, "FAIL tap geometry: %s\n", label);
    exit(1);
}

static void expect_close(const char *label, float actual, float expected, float epsilon) {
    if (absf_(actual - expected) > epsilon) {
        fprintf(stderr, "FAIL tap geometry: %s actual=%.3f expected=%.3f\n", label, actual, expected);
        exit(1);
    }
}

static void expect_inside(const char *label, const SemuTapGeometry *g, float hx, float hy, float hw, float hh) {
    if ((float)g->game_x + 0.75f < hx || (float)g->game_y + 0.75f < hy) {
        fail_(label);
    }
    if ((float)(g->game_x + g->game_w) - 0.75f > hx + hw) {
        fail_(label);
    }
    if ((float)(g->game_y + g->game_h) - 0.75f > hy + hh) {
        fail_(label);
    }
}

static void expect_centered(const char *label, const SemuTapGeometry *g, float hx, float hy, float hw, float hh) {
    expect_close(label, (float)g->game_x + (float)g->game_w * 0.5f, hx + hw * 0.5f, 0.75f);
    expect_close(label, (float)g->game_y + (float)g->game_h * 0.5f, hy + hh * 0.5f, 0.75f);
}

static SemuTapGeometryInput base_input(void) {
    SemuTapGeometryInput in;
    in.win_w = 1280;
    in.win_h = 800;
    in.native_w = 320;
    in.native_h = 240;
    in.display_aspect = 4.0f / 3.0f;
    in.priority_bezel = 0;
    in.fill_hole = 0;
    in.has_art = 1;
    in.art_w = 1600;
    in.art_h = 1000;
    in.hole_x = 0.25f;
    in.hole_y = 0.20f;
    in.hole_w = 0.50f;
    in.hole_h = 0.60f;
    return in;
}

static void check_game_priority_maps_hole_to_game(void) {
    SemuTapGeometryInput in = base_input();
    SemuTapGeometry g;
    float hx, hy, hw, hh;
    in.priority_bezel = 0;
    if (!semu_tap_compute_geometry(&in, &g)) { fail_("game-priority compute"); }
    semu_tap_hole_rect_gl(&in, &g, &hx, &hy, &hw, &hh);

    if (g.bezel_priority) { fail_("game-priority selected bezel priority"); }
    expect_close("game-priority x", (float)g.game_x, 160.0f, 0.01f);
    expect_close("game-priority y", (float)g.game_y, 40.0f, 0.01f);
    expect_close("game-priority w", (float)g.game_w, 960.0f, 0.01f);
    expect_close("game-priority h", (float)g.game_h, 720.0f, 0.01f);
    expect_close("game-priority hole x", hx, (float)g.game_x, 0.75f);
    expect_close("game-priority hole y", hy, (float)g.game_y, 0.75f);
    expect_close("game-priority hole w", hw, (float)g.game_w, 0.75f);
    expect_close("game-priority hole h", hh, (float)g.game_h, 0.75f);
}

static void check_bezel_priority_fits_art_and_centers_game(void) {
    SemuTapGeometryInput in = base_input();
    SemuTapGeometry g;
    float hx, hy, hw, hh;
    in.priority_bezel = 1;
    if (!semu_tap_compute_geometry(&in, &g)) { fail_("bezel-priority compute"); }
    semu_tap_hole_rect_gl(&in, &g, &hx, &hy, &hw, &hh);

    if (!g.bezel_priority) { fail_("bezel-priority not selected"); }
    expect_close("bezel-priority art x", g.bezel_x, 0.0f, 0.01f);
    expect_close("bezel-priority art y", g.bezel_y, 0.0f, 0.01f);
    expect_close("bezel-priority art w", g.bezel_w, 1280.0f, 0.01f);
    expect_close("bezel-priority art h", g.bezel_h, 800.0f, 0.01f);
    expect_close("bezel-priority game w", (float)g.game_w, 640.0f, 0.01f);
    expect_close("bezel-priority game h", (float)g.game_h, 480.0f, 0.01f);
    expect_inside("bezel-priority game inside hole", &g, hx, hy, hw, hh);
    expect_centered("bezel-priority game centered in hole", &g, hx, hy, hw, hh);
}

static void check_bezel_priority_centers_when_integer_scale_leaves_margin(void) {
    SemuTapGeometryInput in = base_input();
    SemuTapGeometry g;
    float hx, hy, hw, hh;
    in.priority_bezel = 1;
    in.hole_h = 0.70f;
    if (!semu_tap_compute_geometry(&in, &g)) { fail_("integer-margin compute"); }
    semu_tap_hole_rect_gl(&in, &g, &hx, &hy, &hw, &hh);

    expect_inside("integer-margin game inside hole", &g, hx, hy, hw, hh);
    expect_centered("integer-margin game centered in hole", &g, hx, hy, hw, hh);
    if (g.game_h % in.native_h != 0) { fail_("integer-margin height not native integer"); }
}

static void check_priority_falls_back_without_art(void) {
    SemuTapGeometryInput in = base_input();
    SemuTapGeometry g;
    in.priority_bezel = 1;
    in.has_art = 0;
    if (!semu_tap_compute_geometry(&in, &g)) { fail_("fallback compute"); }
    if (g.bezel_priority) { fail_("fallback used bezel priority without art"); }
    expect_close("fallback game w", (float)g.game_w, 960.0f, 0.01f);
    expect_close("fallback game h", (float)g.game_h, 720.0f, 0.01f);
}

static void check_fill_hole_uses_full_cutout(void) {
    SemuTapGeometryInput in = base_input();
    SemuTapGeometry g;
    float hx, hy, hw, hh;
    in.priority_bezel = 1;
    in.fill_hole = 1;
    in.native_w = 160;
    in.native_h = 144;
    in.display_aspect = 10.0f / 9.0f;
    in.hole_w = 0.42f;
    in.hole_h = 0.32f;
    if (!semu_tap_compute_geometry(&in, &g)) { fail_("fill-hole compute"); }
    semu_tap_hole_rect_gl(&in, &g, &hx, &hy, &hw, &hh);

    expect_inside("fill-hole game inside hole", &g, hx, hy, hw, hh);
    expect_centered("fill-hole game centered in hole", &g, hx, hy, hw, hh);
    if (absf_((float)g.game_h - hh) > 1.0f && absf_((float)g.game_w - hw) > 1.0f) {
        fail_("fill-hole did not fill either cutout axis");
    }
}

int main(void) {
    check_game_priority_maps_hole_to_game();
    check_bezel_priority_fits_art_and_centers_game();
    check_bezel_priority_centers_when_integer_scale_leaves_margin();
    check_priority_falls_back_without_art();
    check_fill_hole_uses_full_cutout();
    printf("OK tap geometry smoke\n");
    return 0;
}
