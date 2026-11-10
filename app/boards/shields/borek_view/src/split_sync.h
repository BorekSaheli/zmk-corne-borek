/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Keeps the two displays of the split in step.
 *
 * ZMK only tracks layers on the central half, and the two halves would pick
 * art independently, so the central pushes two things to the peripheral over
 * a small private GATT service:
 *
 *   - the art index the peripheral should show, and
 *   - the name of the active layer.
 *
 * The central owns a rotation counter. With N images it shows
 * counter % N itself and tells the peripheral to show (counter + 1) % N, so
 * the two halves never show the same image (which needs N >= 2).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* Random 128-bit UUIDs, private to this keyboard. */
#define SPLIT_SYNC_SERVICE_UUID                                                                    \
    BT_UUID_128_ENCODE(0xf1a4b000, 0x6b1f, 0x4d6e, 0xa5b4, 0x5a6e2f4b6a01)
#define SPLIT_SYNC_ART_CHAR_UUID                                                                   \
    BT_UUID_128_ENCODE(0xf1a4b001, 0x6b1f, 0x4d6e, 0xa5b4, 0x5a6e2f4b6a01)
#define SPLIT_SYNC_LAYER_CHAR_UUID                                                                 \
    BT_UUID_128_ENCODE(0xf1a4b002, 0x6b1f, 0x4d6e, 0xa5b4, 0x5a6e2f4b6a01)

/* Longest layer name sent to the peripheral, including the terminator. */
#define SPLIT_SYNC_LABEL_MAX 16

/*
 * Called when this half's art index (or, on the peripheral, layer label)
 * changes. May run in interrupt or Bluetooth context, so it must only
 * schedule work and never draw.
 */
typedef void (*split_sync_listener_t)(void);

/* Art index this half should currently show. */
uint8_t split_sync_art_index(void);

/* Only one listener is supported; a later call replaces the earlier one. */
void split_sync_set_art_listener(split_sync_listener_t cb);

/* Peripheral only: the layer name last pushed by the central. */
void split_sync_copy_layer_label(char *dst, size_t len);
void split_sync_set_layer_listener(split_sync_listener_t cb);
