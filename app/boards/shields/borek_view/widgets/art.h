/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <stdint.h>

/*
 * Draws one of the baked-in images (widgets/art/) onto a 68x68 canvas in the
 * foreground colour. The caller fills the background first. idx is taken
 * modulo the number of images.
 */
void draw_art(lv_obj_t *canvas, uint8_t idx);
