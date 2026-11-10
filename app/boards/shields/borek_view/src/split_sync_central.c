/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Central half of the display sync, see split_sync.h.
 *
 * Threading: the rotation timer fires in interrupt context and the Bluetooth
 * callbacks run on the Bluetooth threads. Neither touches the connection.
 * Everything that talks to the peripheral runs on the system work queue and
 * looks the connection up on the spot, so there is no connection pointer
 * here that could go stale when the halves drop and reconnect.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "split_sync.h"
#include "../widgets/art/art_list.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(ART_IMAGES_COUNT >= 2, "need at least 2 images so the two halves can differ");

/* ZMK's own split discovery starts right at connect; stay out of its way. */
#define DISCOVERY_DELAY K_SECONDS(3)

static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(SPLIT_SYNC_SERVICE_UUID);
static struct bt_uuid_128 art_chr_uuid = BT_UUID_INIT_128(SPLIT_SYNC_ART_CHAR_UUID);
static struct bt_uuid_128 layer_chr_uuid = BT_UUID_INIT_128(SPLIT_SYNC_LAYER_CHAR_UUID);

/* Attribute handles on the peripheral, 0 while unknown. */
static atomic_t art_char_handle;
static atomic_t layer_char_handle;
static uint16_t svc_end_handle;
static struct bt_gatt_discover_params discover_params;

static atomic_t rotation_counter;
static split_sync_listener_t art_listener;

uint8_t split_sync_art_index(void) {
    return (uint32_t)atomic_get(&rotation_counter) % ART_IMAGES_COUNT;
}

static uint8_t peripheral_art_index(void) {
    return ((uint32_t)atomic_get(&rotation_counter) + 1) % ART_IMAGES_COUNT;
}

void split_sync_set_art_listener(split_sync_listener_t cb) { art_listener = cb; }

/* Connection lookup */

static bool is_peripheral_link(struct bt_conn *conn) {
    struct bt_conn_info info;

    if (bt_conn_get_info(conn, &info) < 0) {
        return false;
    }

    /* The only link on which this half is the central is the one to the
     * other half. Links to hosts have this half as the peripheral. */
    return info.role == BT_CONN_ROLE_CENTRAL && info.state == BT_CONN_STATE_CONNECTED;
}

static void find_peripheral_link(struct bt_conn *conn, void *data) {
    struct bt_conn **found = data;

    if (*found == NULL && is_peripheral_link(conn)) {
        *found = bt_conn_ref(conn);
    }
}

/* Returns a new reference the caller must release, or NULL when the halves
 * are not connected. */
static struct bt_conn *peripheral_link_get(void) {
    struct bt_conn *conn = NULL;

    bt_conn_foreach(BT_CONN_TYPE_LE, find_peripheral_link, &conn);

    return conn;
}

/* Pushing state to the peripheral (system work queue) */

static void push(atomic_t *handle, const void *data, uint16_t len) {
    uint16_t attr_handle = atomic_get(handle);

    if (attr_handle == 0) {
        return;
    }

    struct bt_conn *conn = peripheral_link_get();
    if (conn == NULL) {
        return;
    }

    int err = bt_gatt_write_without_response(conn, attr_handle, data, len, false);
    if (err) {
        LOG_WRN("split sync write to handle %u failed: %d", attr_handle, err);
    }

    bt_conn_unref(conn);
}

static void push_art_work_cb(struct k_work *work) {
    uint8_t idx = peripheral_art_index();

    push(&art_char_handle, &idx, sizeof(idx));
}

static void push_layer_work_cb(struct k_work *work) {
    char label[SPLIT_SYNC_LABEL_MAX];
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index));

    if (name != NULL && name[0] != '\0') {
        strncpy(label, name, sizeof(label) - 1);
        label[sizeof(label) - 1] = '\0';
    } else {
        snprintk(label, sizeof(label), "Layer %u", index);
    }

    push(&layer_char_handle, label, strlen(label) + 1);
}

static K_WORK_DEFINE(push_art_work, push_art_work_cb);
static K_WORK_DEFINE(push_layer_work, push_layer_work_cb);

/* Art rotation */

static void rotation_timer_cb(struct k_timer *timer) {
    atomic_inc(&rotation_counter);

    if (art_listener != NULL) {
        art_listener();
    }

    k_work_submit(&push_art_work);
}

static K_TIMER_DEFINE(rotation_timer, rotation_timer_cb, NULL);

/* Layer changes */

static int layer_listener_cb(const zmk_event_t *eh) {
    k_work_submit(&push_layer_work);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(split_sync_layer, layer_listener_cb);
ZMK_SUBSCRIPTION(split_sync_layer, zmk_layer_state_changed);

/* Discovery of the service on the peripheral: service, then one
 * characteristic after the other. */

static void sync_everything(void) {
    k_work_submit(&push_art_work);
    k_work_submit(&push_layer_work);
}

static uint8_t layer_char_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      struct bt_gatt_discover_params *params) {
    if (attr != NULL) {
        atomic_set(&layer_char_handle, bt_gatt_attr_value_handle(attr));
    }

    LOG_DBG("split sync ready (art handle %u, layer handle %u)",
            (uint16_t)atomic_get(&art_char_handle), (uint16_t)atomic_get(&layer_char_handle));

    /* The peripheral has just (re)connected and knows nothing yet. */
    sync_everything();

    return BT_GATT_ITER_STOP;
}

static uint8_t art_char_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                    struct bt_gatt_discover_params *params) {
    if (attr == NULL) {
        LOG_WRN("split sync art characteristic not found");
        return BT_GATT_ITER_STOP;
    }

    atomic_set(&art_char_handle, bt_gatt_attr_value_handle(attr));

    discover_params.uuid = &layer_chr_uuid.uuid;
    discover_params.start_handle = attr->handle + 1;
    discover_params.end_handle = svc_end_handle;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    discover_params.func = layer_char_discover_cb;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_WRN("split sync layer characteristic discovery failed to start: %d", err);
        /* The art characteristic alone is still worth using. */
        sync_everything();
    }

    return BT_GATT_ITER_STOP;
}

static uint8_t svc_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               struct bt_gatt_discover_params *params) {
    if (attr == NULL) {
        LOG_WRN("split sync service not found on the peripheral");
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_service_val *svc = attr->user_data;

    svc_end_handle = svc != NULL ? svc->end_handle : 0xffff;

    discover_params.uuid = &art_chr_uuid.uuid;
    discover_params.start_handle = attr->handle + 1;
    discover_params.end_handle = svc_end_handle;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    discover_params.func = art_char_discover_cb;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_WRN("split sync art characteristic discovery failed to start: %d", err);
    }

    return BT_GATT_ITER_STOP;
}

static void discover_work_cb(struct k_work *work) {
    struct bt_conn *conn = peripheral_link_get();

    if (conn == NULL) {
        return;
    }

    discover_params.uuid = &svc_uuid.uuid;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    discover_params.func = svc_discover_cb;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_WRN("split sync service discovery failed to start: %d", err);
    }

    bt_conn_unref(conn);
}

static K_WORK_DELAYABLE_DEFINE(discover_work, discover_work_cb);

/* Connection tracking */

static bool is_central_role(struct bt_conn *conn) {
    struct bt_conn_info info;

    return bt_conn_get_info(conn, &info) == 0 && info.role == BT_CONN_ROLE_CENTRAL;
}

static void forget_handles(void) {
    atomic_set(&art_char_handle, 0);
    atomic_set(&layer_char_handle, 0);
}

static void connected_cb(struct bt_conn *conn, uint8_t err) {
    if (err || !is_central_role(conn)) {
        return;
    }

    forget_handles();
    k_work_reschedule(&discover_work, DISCOVERY_DELAY);
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason) {
    if (!is_central_role(conn)) {
        return;
    }

    forget_handles();
    k_work_cancel_delayable(&discover_work);
}

BT_CONN_CB_DEFINE(split_sync_conn_cb) = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};

static int split_sync_central_init(void) {
    k_timeout_t period = K_MINUTES(CONFIG_BOREK_VIEW_ART_ROTATE_MINUTES);

    k_timer_start(&rotation_timer, period, period);

    return 0;
}

SYS_INIT(split_sync_central_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
