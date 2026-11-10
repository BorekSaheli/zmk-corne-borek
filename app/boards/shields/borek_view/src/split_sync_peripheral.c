/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Peripheral half of the display sync, see split_sync.h.
 *
 * Exposes two write-only characteristics. The write callbacks run on a
 * Bluetooth thread, so they only store the value and poke the listener; the
 * display thread reads the stored value back when it redraws.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include "split_sync.h"
#include "../widgets/art/art_list.h"

BUILD_ASSERT(ART_IMAGES_COUNT >= 2, "need at least 2 images so the two halves can differ");

/* Starts at 1 so a freshly booted peripheral differs from a freshly booted
 * central (which starts at 0) until the first write arrives. */
static atomic_t art_index = ATOMIC_INIT(1);
static split_sync_listener_t art_listener;

static struct k_spinlock label_lock;
static char layer_label[SPLIT_SYNC_LABEL_MAX];
static split_sync_listener_t layer_listener;

uint8_t split_sync_art_index(void) { return atomic_get(&art_index); }

void split_sync_set_art_listener(split_sync_listener_t cb) { art_listener = cb; }

void split_sync_copy_layer_label(char *dst, size_t len) {
    if (len == 0) {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&label_lock);

    strncpy(dst, layer_label, len - 1);
    dst[len - 1] = '\0';

    k_spin_unlock(&label_lock, key);
}

void split_sync_set_layer_listener(split_sync_listener_t cb) { layer_listener = cb; }

static ssize_t write_art_index(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    if (len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
        return 0;
    }

    uint8_t idx = *(const uint8_t *)buf;

    /* Both halves are built from the same image list, so this only triggers
     * if they run different firmware. */
    if (idx >= ART_IMAGES_COUNT) {
        idx = 0;
    }

    if (atomic_set(&art_index, idx) != idx && art_listener != NULL) {
        art_listener();
    }

    return len;
}

static ssize_t write_layer_label(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    if (len == 0 || len > SPLIT_SYNC_LABEL_MAX) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
        return 0;
    }

    size_t copy = MIN(len, SPLIT_SYNC_LABEL_MAX - 1);
    k_spinlock_key_t key = k_spin_lock(&label_lock);

    memcpy(layer_label, buf, copy);
    layer_label[copy] = '\0';

    k_spin_unlock(&label_lock, key);

    if (layer_listener != NULL) {
        layer_listener();
    }

    return len;
}

/* Writes need an encrypted link, which the link between the halves always is. */
BT_GATT_SERVICE_DEFINE(
    split_sync_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(SPLIT_SYNC_SERVICE_UUID)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(SPLIT_SYNC_ART_CHAR_UUID),
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_art_index, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(SPLIT_SYNC_LAYER_CHAR_UUID),
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_layer_label, NULL));
