#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tap_menu.h"

static void expect_int(const char *label, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "FAIL tap menu: %s actual=%d expected=%d\n", label, actual, expected);
        exit(1);
    }
}

static void expect_text(const char *label, const char *actual, const char *expected) {
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL tap menu: %s actual=%s expected=%s\n", label, actual, expected);
        exit(1);
    }
}

static void expect_short(const char *label, SemuTapMenuState *state, int index, const char *expected) {
    char value[64];
    semu_tap_menu_short(state, index, value, sizeof(value));
    expect_text(label, value, expected);
}

static void expect_value(const char *label, SemuTapMenuState *state, int index, const char *expected) {
    char value[64];
    semu_tap_menu_value(state, index, value, sizeof(value));
    expect_text(label, value, expected);
}

static SemuTapMenuState base_state(void) {
    SemuTapMenuState state;
    memset(&state, 0, sizeof(state));
    state.level = SEMU_TAP_MENU_ROOT;
    state.system_kind = SEMU_TAP_SYSTEM_GENERIC;
    state.bezel_count = 3;
    state.nds_primary_scale = 3;
    state.nds_secondary_scale = 3;
    semu_tap_menu_normalize(&state);
    return state;
}

static void check_root_menu_shape(void) {
    SemuTapMenuState generic = base_state();
    expect_int("generic root count", semu_tap_menu_count(&generic), 2);
    expect_short("generic root display", &generic, 0, "Display");
    expect_short("generic root save/load", &generic, 1, "Save/Load");

    SemuTapMenuState dual = base_state();
    dual.system_kind = SEMU_TAP_SYSTEM_DUAL_SCREEN;
    semu_tap_menu_normalize(&dual);
    expect_int("dual root count", semu_tap_menu_count(&dual), 3);
    expect_short("dual root system label", &dual, 2, "Screens");

    SemuTapMenuState wii = base_state();
    wii.system_kind = SEMU_TAP_SYSTEM_WII;
    semu_tap_menu_normalize(&wii);
    expect_int("wii root count", semu_tap_menu_count(&wii), 3);
    expect_short("wii root system label", &wii, 2, "Controls");
}

static void check_rendering_menu(void) {
    SemuTapMenuState state = base_state();
    state.level = SEMU_TAP_MENU_RENDERING;
    state.selected = 0;
    semu_tap_menu_normalize(&state);

    expect_int("rendering count", semu_tap_menu_count(&state), 4);
    expect_short("display priority label", &state, 0, "Priority");
    expect_short("rendering bezel label", &state, 1, "Next Bezel");
    expect_short("rendering shader label", &state, 2, "Next Shader");
    expect_short("rendering back label", &state, 3, "Back");
    expect_value("default priority value", &state, 0, "Game Priority");

    semu_tap_menu_activate(&state);
    expect_value("toggled priority value", &state, 0, "Bezel Priority");

    state.selected = 1;
    expect_value("bezel A", &state, 1, "A");
    semu_tap_menu_activate(&state);
    expect_value("bezel B", &state, 1, "B");
    semu_tap_menu_activate(&state);
    expect_value("bezel C", &state, 1, "C");
    semu_tap_menu_activate(&state);
    expect_value("bezel off", &state, 1, "Off");
    semu_tap_menu_activate(&state);
    expect_value("bezel wraps to A", &state, 1, "A");

    state.selected = 2;
    expect_value("shader A", &state, 2, "A");
    semu_tap_menu_activate(&state);
    expect_value("shader B", &state, 2, "B");
    semu_tap_menu_activate(&state);
    expect_value("shader C", &state, 2, "C");
    semu_tap_menu_activate(&state);
    expect_value("shader off", &state, 2, "Off");
    semu_tap_menu_activate(&state);
    expect_value("shader wraps to A", &state, 2, "A");
}

static void check_save_load_menu(void) {
    SemuTapMenuState state = base_state();
    state.level = SEMU_TAP_MENU_SAVE_LOAD;
    semu_tap_menu_normalize(&state);

    expect_int("save/load count", semu_tap_menu_count(&state), 4);
    expect_short("slot label", &state, 0, "Slot");
    expect_short("save label", &state, 1, "Save");
    expect_short("load label", &state, 2, "Load");
    expect_value("slot one", &state, 0, "1");

    state.selected = 0;
    semu_tap_menu_activate(&state);
    expect_value("slot two", &state, 0, "2");
    semu_tap_menu_activate(&state);
    expect_value("slot three", &state, 0, "3");
    semu_tap_menu_activate(&state);
    expect_value("slot wraps", &state, 0, "1");

    state.save_slot = 2;
    state.selected = 1;
    semu_tap_menu_activate(&state);
    expect_int("save action", state.action, SEMU_TAP_MENU_ACTION_SAVE);
    expect_int("save slot", state.action_slot, 3);

    state.selected = 2;
    semu_tap_menu_activate(&state);
    expect_int("load action", state.action, SEMU_TAP_MENU_ACTION_LOAD);
    expect_int("load slot", state.action_slot, 3);
}

static void check_dual_screen_menu(void) {
    SemuTapMenuState state = base_state();
    state.system_kind = SEMU_TAP_SYSTEM_DUAL_SCREEN;
    state.level = SEMU_TAP_MENU_SYSTEM;
    semu_tap_menu_normalize(&state);

    expect_int("dual screen count", semu_tap_menu_count(&state), 4);
    expect_short("dual layout label", &state, 0, "Layout");
    expect_short("dual top label", &state, 1, "Top");
    expect_short("dual bottom label", &state, 2, "Bottom");
    expect_value("dual layout vertical", &state, 0, "Vertical");
    expect_value("dual top default", &state, 1, "2x");
    expect_value("dual bottom default", &state, 2, "2x");

    state.selected = 0;
    semu_tap_menu_activate(&state);
    expect_value("dual layout horizontal", &state, 0, "Horizontal");
    state.selected = 1;
    semu_tap_menu_activate(&state);
    expect_value("dual top 3x", &state, 1, "3x");
    semu_tap_menu_activate(&state);
    expect_value("dual top wraps", &state, 1, "0.25x");
    state.selected = 2;
    semu_tap_menu_activate(&state);
    expect_value("dual bottom 3x", &state, 2, "3x");
}

static void check_wii_menu(void) {
    SemuTapMenuState state = base_state();
    state.system_kind = SEMU_TAP_SYSTEM_WII;
    state.level = SEMU_TAP_MENU_SYSTEM;
    semu_tap_menu_normalize(&state);

    expect_int("wii count", semu_tap_menu_count(&state), 2);
    expect_short("wii pad label", &state, 0, "Pad");
    expect_short("wii back label", &state, 1, "Back");
    expect_value("wii wiimote", &state, 0, "Wiimote");
    state.selected = 0;
    semu_tap_menu_activate(&state);
    expect_value("wii nunchuk", &state, 0, "Wiimote+Nunchuk");
    semu_tap_menu_activate(&state);
    expect_value("wii pro", &state, 0, "Pro Controller");
    semu_tap_menu_activate(&state);
    expect_value("wii classic", &state, 0, "Classic");
    semu_tap_menu_activate(&state);
    expect_value("wii wraps", &state, 0, "Wiimote");
}

static void check_back_navigation(void) {
    SemuTapMenuState state = base_state();
    state.level = SEMU_TAP_MENU_RENDERING;
    state.selected = 3;
    semu_tap_menu_activate(&state);
    expect_int("render back level", state.level, SEMU_TAP_MENU_ROOT);
    expect_int("render back selected", state.selected, 0);

    state.system_kind = SEMU_TAP_SYSTEM_WII;
    state.level = SEMU_TAP_MENU_SYSTEM;
    state.selected = 1;
    semu_tap_menu_activate(&state);
    expect_int("system back level", state.level, SEMU_TAP_MENU_ROOT);
}

int main(void) {
    check_root_menu_shape();
    check_rendering_menu();
    check_save_load_menu();
    check_dual_screen_menu();
    check_wii_menu();
    check_back_navigation();
    printf("OK tap menu smoke\n");
    return 0;
}
