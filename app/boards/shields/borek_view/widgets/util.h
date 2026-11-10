/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <lvgl.h>

/*
 * The panel is 160x68 and mounted sideways. The screen is built from three
 * 68x68 canvases next to each other; each one is drawn upright and then
 * rotated into place.
 */
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 68

#define CANVAS_SIZE 68
#define CANVAS_COLOR_FORMAT LV_COLOR_FORMAT_L8 // smallest format lv_draw_sw_rotate supports
#define CANVAS_BUF_SIZE                                                                            \
    LV_CANVAS_BUF_SIZE(CANVAS_SIZE, CANVAS_SIZE, LV_COLOR_FORMAT_GET_BPP(CANVAS_COLOR_FORMAT),     \
                       LV_DRAW_BUF_STRIDE_ALIGN)

/* Horizontal offsets of the canvases, measured from the right edge. */
#define CANVAS_OFFSET_STATUS 0
#define CANVAS_OFFSET_ART -44
#define CANVAS_OFFSET_LAYER -112

#define LVGL_BACKGROUND                                                                            \
    IS_ENABLED(CONFIG_BOREK_VIEW_WIDGET_INVERTED) ? lv_color_black() : lv_color_white()
#define LVGL_FOREGROUND                                                                            \
    IS_ENABLED(CONFIG_BOREK_VIEW_WIDGET_INVERTED) ? lv_color_white() : lv_color_black()

/* Creates one of the three canvases as a child of parent. */
lv_obj_t *create_canvas(lv_obj_t *parent, uint8_t *buf, int32_t x_offset);

void fill_background(lv_obj_t *canvas);

/* Draws centered text in the default font across the canvas at height y. */
void draw_centered_text(lv_obj_t *canvas, int32_t y, const char *text);

/* Rotates what has been drawn into the panel's orientation. Call last. */
void rotate_canvas(lv_obj_t *canvas);
