/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/kernel.h>

#include "util.h"

lv_obj_t *create_canvas(lv_obj_t *parent, uint8_t *buf, int32_t x_offset) {
    lv_obj_t *canvas = lv_canvas_create(parent);

    lv_obj_align(canvas, LV_ALIGN_TOP_RIGHT, x_offset, 0);
    lv_canvas_set_buffer(canvas, buf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    return canvas;
}

void fill_background(lv_obj_t *canvas) { lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER); }

void draw_centered_text(lv_obj_t *canvas, int32_t y, const char *text) {
    lv_draw_label_dsc_t label_dsc;

    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = LVGL_FOREGROUND;
    label_dsc.font = lv_font_default();
    label_dsc.align = LV_TEXT_ALIGN_CENTER;
    label_dsc.text = text;

    lv_layer_t layer;
    lv_area_t coords = {0, y, CANVAS_SIZE - 1, y + 20};

    lv_canvas_init_layer(canvas, &layer);
    lv_draw_label(&layer, &label_dsc, &coords);
    lv_canvas_finish_layer(canvas, &layer);
}

void rotate_canvas(lv_obj_t *canvas) {
    /* Only ever used from the display thread, so one scratch buffer is enough. */
    static uint8_t scratch[CANVAS_BUF_SIZE];
    uint8_t *buf = lv_canvas_get_draw_buf(canvas)->data;

    memcpy(scratch, buf, CANVAS_BUF_SIZE);

    /* Also marks the canvas as needing a redraw. */
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    const int32_t stride = lv_draw_buf_width_to_stride(CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    lv_draw_sw_rotate(scratch, buf, CANVAS_SIZE, CANVAS_SIZE, stride, stride,
                      LV_DISPLAY_ROTATION_270, CANVAS_COLOR_FORMAT);
}
