/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Status screen of the peripheral (right) half: battery and link to the
 * other half on the right, art in the middle, active layer on the left.
 *
 * The art index and the layer name are pushed by the central half, see
 * ../src/split_sync.h.
 *
 * LVGL is not thread safe. Everything here that draws runs on the display
 * work queue: the ZMK listener below gets there through
 * ZMK_DISPLAY_WIDGET_LISTENER, and the pushed values, which arrive on a
 * Bluetooth thread, are queued there by hand.
 */

#include <stdio.h>

#include <zephyr/kernel.h>

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>

#include "status.h"
#include "art.h"
#include "../src/split_sync.h"

/* There is only ever one status screen. */
static struct zmk_widget_status *widget;

/* Whether the other half is connected. Only touched on the display thread. */
static bool link_up;

/* Active layer, as last reported by the central half */

static void redraw_layer(void) {
    if (widget == NULL) {
        return;
    }

    lv_obj_t *canvas = widget->layer_canvas;
    char name[SPLIT_SYNC_LABEL_MAX] = "";

    /* Without the other half there is no way to know the layer. */
    if (link_up) {
        split_sync_copy_layer_label(name, sizeof(name));
    }

    fill_background(canvas);
    draw_centered_text(canvas, 24, name[0] != '\0' ? name : "---");
    rotate_canvas(canvas);
}

static void layer_work_cb(struct k_work *work) { redraw_layer(); }

static K_WORK_DEFINE(layer_work, layer_work_cb);

/* Called on a Bluetooth thread. */
static void on_layer_changed(void) { k_work_submit_to_queue(zmk_display_work_q(), &layer_work); }

/* Battery and link */

struct link_state {
    uint8_t battery;
    bool connected;
};

static struct link_state get_link_state(const zmk_event_t *eh) {
    return (struct link_state){
        .battery = zmk_battery_state_of_charge(),
        .connected = zmk_split_bt_peripheral_is_connected(),
    };
}

static void draw_link(struct link_state state) {
    if (widget == NULL) {
        return;
    }

    lv_obj_t *canvas = widget->status_canvas;
    char text[8];

    fill_background(canvas);

    snprintf(text, sizeof(text), "%u%%", state.battery);
    draw_centered_text(canvas, 2, text);
    draw_centered_text(canvas, 18, state.connected ? "LINK" : "----");

    rotate_canvas(canvas);

    if (state.connected != link_up) {
        link_up = state.connected;
        redraw_layer();
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(link_listener, struct link_state, draw_link, get_link_state)
ZMK_SUBSCRIPTION(link_listener, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(link_listener, zmk_split_peripheral_status_changed);

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

/* Called on a Bluetooth thread. */
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

    /* Draws the status canvas once with the current state. */
    link_listener_init();
    redraw_layer();
    redraw_art();

    split_sync_set_art_listener(on_art_changed);
    split_sync_set_layer_listener(on_layer_changed);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *w) { return w->obj; }
