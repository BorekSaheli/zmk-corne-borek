/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>

#include "util.h"

/*
 * The whole status screen: battery and connection on the right, art in the
 * middle, layer name on the left. Implemented by central_status.c on the
 * central half and by peripheral_status.c on the peripheral half.
 */
struct zmk_widget_status {
    lv_obj_t *obj;
    lv_obj_t *status_canvas;
    lv_obj_t *art_canvas;
    lv_obj_t *layer_canvas;
    uint8_t status_buf[CANVAS_BUF_SIZE];
    uint8_t art_buf[CANVAS_BUF_SIZE];
    uint8_t layer_buf[CANVAS_BUF_SIZE];
};

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget);
