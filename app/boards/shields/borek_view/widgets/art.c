/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "art.h"
#include "util.h"
#include "art/art_list.h"

BUILD_ASSERT(ART_IMG_W == CANVAS_SIZE && ART_IMG_H == CANVAS_SIZE,
             "art images must match the canvas size, re-run images/convert.py");

void draw_art(lv_obj_t *canvas, uint8_t idx) {
    const uint8_t *pixels = art_images[idx % ART_IMAGES_COUNT];
    const lv_color_t color = LVGL_FOREGROUND;

    /* The images are packed 1 bit per pixel, most significant bit first.
     * lv_canvas_set_px writes straight into the canvas buffer, which keeps a
     * full redraw cheap. */
    for (int32_t y = 0; y < ART_IMG_H; y++) {
        const uint8_t *row = pixels + y * ART_IMG_ROW_BYTES;

        for (int32_t x = 0; x < ART_IMG_W; x++) {
            if (row[x / 8] & (0x80 >> (x % 8))) {
                lv_canvas_set_px(canvas, x, y, color, LV_OPA_COVER);
            }
        }
    }
}
