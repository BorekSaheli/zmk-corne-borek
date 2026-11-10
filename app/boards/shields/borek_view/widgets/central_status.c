/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Status screen of the central (left) half: battery and output on the right,
 * art in the middle, active layer on the left.
 *
 * LVGL is not thread safe. Everything here that draws runs on the display
 * work queue: the two ZMK listeners below get there through
 * ZMK_DISPLAY_WIDGET_LISTENER, and art changes are queued there by hand.
 */

#include <stdio.h>

#include <zephyr/kernel.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "status.h"
#include "art.h"
#include "../src/split_sync.h"

/* There is only ever one status screen. */
static struct zmk_widget_status *widget;

/* Battery and output */

struct output_state {
    uint8_t battery;
    bool usb;
    uint8_t profile_index;
    bool profile_connected;
};

static struct output_state get_output_state(const zmk_event_t *eh) {
    return (struct output_state){
        .battery = zmk_battery_state_of_charge(),
        .usb = zmk_endpoint_get_selected().transport == ZMK_TRANSPORT_USB,
        .profile_index = zmk_ble_active_profile_index(),
        .profile_connected = zmk_ble_active_profile_is_connected(),
    };
}

static void draw_output(struct output_state state) {
    if (widget == NULL) {
        return;
    }

    lv_obj_t *canvas = widget->status_canvas;
    char text[12];

    fill_background(canvas);

    snprintf(text, sizeof(text), "%u%%", state.battery);
    draw_centered_text(canvas, 2, text);

    /* "BT2 *" means profile 2 is connected, "BT2" that it is not. */
    if (state.usb) {
        snprintf(text, sizeof(text), "USB");
    } else if (state.profile_connected) {
        snprintf(text, sizeof(text), "BT%u *", state.profile_index + 1);
    } else {
        snprintf(text, sizeof(text), "BT%u", state.profile_index + 1);
    }
    draw_centered_text(canvas, 18, text);

    rotate_canvas(canvas);
}

ZMK_DISPLAY_WIDGET_LISTENER(output_listener, struct output_state, draw_output, get_output_state)
ZMK_SUBSCRIPTION(output_listener, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(output_listener, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(output_listener, zmk_endpoint_changed);

/* Active layer */

struct layer_state {
    zmk_keymap_layer_index_t index;
    const char *name;
};

static struct layer_state get_layer_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();

    return (struct layer_state){
        .index = index,
        .name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index)),
    };
}

static void draw_layer(struct layer_state state) {
    if (widget == NULL) {
        return;
    }

    lv_obj_t *canvas = widget->layer_canvas;
    char text[16];

    fill_background(canvas);

    if (state.name != NULL && state.name[0] != '\0') {
        snprintf(text, sizeof(text), "%s", state.name);
    } else {
        snprintf(text, sizeof(text), "Layer %u", state.index);
    }
    draw_centered_text(canvas, 24, text);

    rotate_canvas(canvas);
}

ZMK_DISPLAY_WIDGET_LISTENER(layer_listener, struct layer_state, draw_layer, get_layer_state)
ZMK_SUBSCRIPTION(layer_listener, zmk_layer_state_changed);

/* Art */

static void redraw_art(void) {
    if (widget == NULL) {
        return;
    }

    lv_obj_t *canvas = widget->art_canvas;

    fill_background(canvas);
    draw_art(canvas, split_sync_art_index());
    rotate_canvas(canvas);
}

static void art_work_cb(struct k_work *work) { redraw_art(); }

static K_WORK_DEFINE(art_work, art_work_cb);

/* Called from the rotation timer, i.e. in interrupt context. */
static void on_art_changed(void) { k_work_submit_to_queue(zmk_display_work_q(), &art_work); }

int zmk_widget_status_init(struct zmk_widget_status *w, lv_obj_t *parent) {
    w->obj = lv_obj_create(parent);
    lv_obj_set_size(w->obj, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(w->obj, LVGL_BACKGROUND, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(w->obj, LV_OPA_COVER, LV_PART_MAIN);

    w->status_canvas = create_canvas(w->obj, w->status_buf, CANVAS_OFFSET_STATUS);
    w->art_canvas = create_canvas(w->obj, w->art_buf, CANVAS_OFFSET_ART);
    w->layer_canvas = create_canvas(w->obj, w->layer_buf, CANVAS_OFFSET_LAYER);

    widget = w;

    /* Each of these draws its canvas once with the current state. */
    output_listener_init();
    layer_listener_init();
    redraw_art();

    split_sync_set_art_listener(on_art_changed);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *w) { return w->obj; }
