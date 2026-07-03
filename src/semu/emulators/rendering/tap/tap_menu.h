#ifndef SEMU_TAP_MENU_H
#define SEMU_TAP_MENU_H

#include <stddef.h>
#include <stdio.h>

#define SEMU_TAP_MENU_ROOT 0
#define SEMU_TAP_MENU_RENDERING 1
#define SEMU_TAP_MENU_SAVE_LOAD 2
#define SEMU_TAP_MENU_SYSTEM 3

#define SEMU_TAP_SYSTEM_GENERIC 0
#define SEMU_TAP_SYSTEM_DUAL_SCREEN 1
#define SEMU_TAP_SYSTEM_WII 2

#define SEMU_TAP_MENU_ACTION_NONE 0
#define SEMU_TAP_MENU_ACTION_SAVE 1
#define SEMU_TAP_MENU_ACTION_LOAD 2

typedef struct SemuTapMenuState {
    int level;
    int selected;
    int system_kind;
    int priority_bezel;
    int bezel_index;
    int bezel_count;
    int shader_index;
    int save_slot;
    int nds_layout;
    int nds_primary_scale;
    int nds_secondary_scale;
    int wii_controller;
    int action;
    int action_slot;
} SemuTapMenuState;

static int semu_tap_menu_visible_bezel_count(int bezel_count) {
    if (bezel_count < 0) { return 0; }
    if (bezel_count > 3) { return 3; }
    return bezel_count;
}

static int semu_tap_menu_clamp(int value, int min, int max) {
    if (value < min) { return min; }
    if (value > max) { return max; }
    return value;
}

static void semu_tap_menu_copy(char *out, size_t out_len, const char *value) {
    if (!out || out_len == 0) { return; }
    snprintf(out, out_len, "%s", value ? value : "");
}

static void semu_tap_menu_normalize(SemuTapMenuState *state) {
    if (!state) { return; }
    state->level = semu_tap_menu_clamp(state->level, SEMU_TAP_MENU_ROOT, SEMU_TAP_MENU_SYSTEM);
    if (state->system_kind != SEMU_TAP_SYSTEM_DUAL_SCREEN && state->system_kind != SEMU_TAP_SYSTEM_WII) {
        state->system_kind = SEMU_TAP_SYSTEM_GENERIC;
    }
    int bezel_count = semu_tap_menu_visible_bezel_count(state->bezel_count);
    state->bezel_count = bezel_count;
    state->bezel_index = semu_tap_menu_clamp(state->bezel_index, 0, bezel_count);
    state->shader_index = semu_tap_menu_clamp(state->shader_index, 0, 3);
    state->save_slot = semu_tap_menu_clamp(state->save_slot, 0, 2);
    state->nds_layout = state->nds_layout ? 1 : 0;
    state->nds_primary_scale = semu_tap_menu_clamp(state->nds_primary_scale, 0, 4);
    state->nds_secondary_scale = semu_tap_menu_clamp(state->nds_secondary_scale, 0, 4);
    state->wii_controller = semu_tap_menu_clamp(state->wii_controller, 0, 3);
    int count = 1;
    if (state->level == SEMU_TAP_MENU_ROOT) {
        count = 2 + ((state->system_kind == SEMU_TAP_SYSTEM_DUAL_SCREEN || state->system_kind == SEMU_TAP_SYSTEM_WII) ? 1 : 0);
    } else if (state->level == SEMU_TAP_MENU_RENDERING || state->level == SEMU_TAP_MENU_SAVE_LOAD) {
        count = 4;
    } else if (state->level == SEMU_TAP_MENU_SYSTEM) {
        count = state->system_kind == SEMU_TAP_SYSTEM_DUAL_SCREEN ? 4 : state->system_kind == SEMU_TAP_SYSTEM_WII ? 2 : 1;
    }
    if (state->selected < 0) { state->selected = 0; }
    if (state->selected >= count) { state->selected = count - 1; }
}

static int semu_tap_menu_count(const SemuTapMenuState *state) {
    if (!state) { return 1; }
    if (state->level == SEMU_TAP_MENU_ROOT) {
        return 2 + ((state->system_kind == SEMU_TAP_SYSTEM_DUAL_SCREEN || state->system_kind == SEMU_TAP_SYSTEM_WII) ? 1 : 0);
    }
    if (state->level == SEMU_TAP_MENU_RENDERING || state->level == SEMU_TAP_MENU_SAVE_LOAD) {
        return 4;
    }
    if (state->level == SEMU_TAP_MENU_SYSTEM) {
        if (state->system_kind == SEMU_TAP_SYSTEM_DUAL_SCREEN) { return 4; }
        if (state->system_kind == SEMU_TAP_SYSTEM_WII) { return 2; }
    }
    return 1;
}

static const char *semu_tap_menu_title(const SemuTapMenuState *state) {
    if (!state) { return "SEMU"; }
    if (state->level == SEMU_TAP_MENU_RENDERING) { return "RENDER"; }
    if (state->level == SEMU_TAP_MENU_SAVE_LOAD) { return "SAVE"; }
    if (state->level == SEMU_TAP_MENU_SYSTEM) { return "SYSTEM"; }
    return "SEMU";
}

static void semu_tap_menu_short(const SemuTapMenuState *state, int index, char *out, size_t out_len) {
    if (!state) {
        semu_tap_menu_copy(out, out_len, "");
        return;
    }
    if (state->level == SEMU_TAP_MENU_ROOT) {
        if (index == 0) { semu_tap_menu_copy(out, out_len, "Rendering"); return; }
        if (index == 1) { semu_tap_menu_copy(out, out_len, "Save/Load"); return; }
        if (index == 2 && state->system_kind == SEMU_TAP_SYSTEM_DUAL_SCREEN) { semu_tap_menu_copy(out, out_len, "Screens"); return; }
        if (index == 2 && state->system_kind == SEMU_TAP_SYSTEM_WII) { semu_tap_menu_copy(out, out_len, "Controls"); return; }
    } else if (state->level == SEMU_TAP_MENU_RENDERING) {
        const char *labels[4] = { "Aspect", "Bezel", "Shader", "Back" };
        if (index >= 0 && index < 4) { semu_tap_menu_copy(out, out_len, labels[index]); return; }
    } else if (state->level == SEMU_TAP_MENU_SAVE_LOAD) {
        const char *labels[4] = { "Slot", "Save", "Load", "Back" };
        if (index >= 0 && index < 4) { semu_tap_menu_copy(out, out_len, labels[index]); return; }
    } else if (state->level == SEMU_TAP_MENU_SYSTEM) {
        if (state->system_kind == SEMU_TAP_SYSTEM_DUAL_SCREEN) {
            const char *labels[4] = { "Layout", "Top", "Bottom", "Back" };
            if (index >= 0 && index < 4) { semu_tap_menu_copy(out, out_len, labels[index]); return; }
        } else if (state->system_kind == SEMU_TAP_SYSTEM_WII) {
            const char *labels[2] = { "Pad", "Back" };
            if (index >= 0 && index < 2) { semu_tap_menu_copy(out, out_len, labels[index]); return; }
        } else if (index == 0) {
            semu_tap_menu_copy(out, out_len, "Back");
            return;
        }
    }
    semu_tap_menu_copy(out, out_len, "");
}

static const char *semu_tap_menu_bezel_value(const SemuTapMenuState *state) {
    int count = state ? semu_tap_menu_visible_bezel_count(state->bezel_count) : 0;
    int index = state ? state->bezel_index : 0;
    if (index >= count) { return "Off"; }
    if (index == 0) { return "A"; }
    if (index == 1) { return "B"; }
    return "C";
}

static const char *semu_tap_menu_shader_value(const SemuTapMenuState *state) {
    int index = state ? state->shader_index : 3;
    if (index == 0) { return "A"; }
    if (index == 1) { return "B"; }
    if (index == 2) { return "C"; }
    return "Off";
}

static void semu_tap_menu_value(const SemuTapMenuState *state, int index, char *out, size_t out_len) {
    if (!state) {
        semu_tap_menu_copy(out, out_len, "");
        return;
    }
    if (state->level == SEMU_TAP_MENU_RENDERING) {
        if (index == 0) { semu_tap_menu_copy(out, out_len, state->priority_bezel ? "Bezel-priority" : "Game-priority"); return; }
        if (index == 1) { semu_tap_menu_copy(out, out_len, semu_tap_menu_bezel_value(state)); return; }
        if (index == 2) { semu_tap_menu_copy(out, out_len, semu_tap_menu_shader_value(state)); return; }
    } else if (state->level == SEMU_TAP_MENU_SAVE_LOAD) {
        if (index == 0) {
            char slot[8];
            snprintf(slot, sizeof(slot), "%d", state->save_slot + 1);
            semu_tap_menu_copy(out, out_len, slot);
            return;
        }
    } else if (state->level == SEMU_TAP_MENU_SYSTEM) {
        if (state->system_kind == SEMU_TAP_SYSTEM_DUAL_SCREEN) {
            const char *scales[5] = { "0.25x", "0.5x", "1x", "2x", "3x" };
            if (index == 0) { semu_tap_menu_copy(out, out_len, state->nds_layout ? "Horizontal" : "Vertical"); return; }
            if (index == 1) { semu_tap_menu_copy(out, out_len, scales[semu_tap_menu_clamp(state->nds_primary_scale, 0, 4)]); return; }
            if (index == 2) { semu_tap_menu_copy(out, out_len, scales[semu_tap_menu_clamp(state->nds_secondary_scale, 0, 4)]); return; }
        } else if (state->system_kind == SEMU_TAP_SYSTEM_WII) {
            const char *controllers[4] = { "Wiimote", "Wiimote+Nunchuk", "Pro Controller", "Classic" };
            if (index == 0) { semu_tap_menu_copy(out, out_len, controllers[semu_tap_menu_clamp(state->wii_controller, 0, 3)]); return; }
        }
    }
    semu_tap_menu_copy(out, out_len, "");
}

static void semu_tap_menu_activate(SemuTapMenuState *state) {
    if (!state) { return; }
    semu_tap_menu_normalize(state);
    state->action = SEMU_TAP_MENU_ACTION_NONE;
    state->action_slot = 0;
    if (state->level == SEMU_TAP_MENU_ROOT) {
        state->level = state->selected == 0 ? SEMU_TAP_MENU_RENDERING : state->selected == 1 ? SEMU_TAP_MENU_SAVE_LOAD : SEMU_TAP_MENU_SYSTEM;
        state->selected = 0;
    } else if (state->level == SEMU_TAP_MENU_RENDERING) {
        if (state->selected == 0) {
            state->priority_bezel = !state->priority_bezel;
        } else if (state->selected == 1) {
            int count = semu_tap_menu_visible_bezel_count(state->bezel_count);
            state->bezel_index = (state->bezel_index + 1) % (count + 1);
        } else if (state->selected == 2) {
            state->shader_index = (state->shader_index + 1) % 4;
        } else {
            state->level = SEMU_TAP_MENU_ROOT;
            state->selected = 0;
        }
    } else if (state->level == SEMU_TAP_MENU_SAVE_LOAD) {
        if (state->selected == 0) {
            state->save_slot = (state->save_slot + 1) % 3;
        } else if (state->selected == 1) {
            state->action = SEMU_TAP_MENU_ACTION_SAVE;
            state->action_slot = state->save_slot + 1;
        } else if (state->selected == 2) {
            state->action = SEMU_TAP_MENU_ACTION_LOAD;
            state->action_slot = state->save_slot + 1;
        } else {
            state->level = SEMU_TAP_MENU_ROOT;
            state->selected = 0;
        }
    } else if (state->level == SEMU_TAP_MENU_SYSTEM) {
        if (state->system_kind == SEMU_TAP_SYSTEM_DUAL_SCREEN) {
            if (state->selected == 0) {
                state->nds_layout = !state->nds_layout;
            } else if (state->selected == 1) {
                state->nds_primary_scale = (state->nds_primary_scale + 1) % 5;
            } else if (state->selected == 2) {
                state->nds_secondary_scale = (state->nds_secondary_scale + 1) % 5;
            } else {
                state->level = SEMU_TAP_MENU_ROOT;
                state->selected = 0;
            }
        } else if (state->system_kind == SEMU_TAP_SYSTEM_WII) {
            if (state->selected == 0) {
                state->wii_controller = (state->wii_controller + 1) % 4;
            } else {
                state->level = SEMU_TAP_MENU_ROOT;
                state->selected = 0;
            }
        } else {
            state->level = SEMU_TAP_MENU_ROOT;
            state->selected = 0;
        }
    }
    semu_tap_menu_normalize(state);
}

#endif
