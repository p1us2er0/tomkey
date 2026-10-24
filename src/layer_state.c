/* Read-only layer telemetry. Protocol v1: version, active-host, highest,
 * reserved, active-mask LE32, sequence LE32. No key events or keymap writes. */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/atomic.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>

#define TOMKEY_UUID(n) BT_UUID_128_ENCODE(0xa4f15d00 + (n), 0x9c46, 0x4d66, 0xa255, 0x746f6d6b6579)
static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(TOMKEY_UUID(0));
static struct bt_uuid_128 state_uuid = BT_UUID_INIT_128(TOMKEY_UUID(1));
static atomic_t sequence;

static void encode_state(struct bt_conn *conn, uint8_t payload[12]) {
    struct bt_conn *active = zmk_ble_active_profile_conn();
    payload[0] = 1;
    payload[1] = active && active == conn &&
                 zmk_endpoint_get_selected().transport == ZMK_TRANSPORT_BLE ? 1 : 0;
    uint32_t raw = (uint32_t)zmk_keymap_layer_state() | BIT(zmk_keymap_layer_default());
    uint32_t mask = 0;
    uint8_t highest = 0;
    for (uint8_t i = 0; i < ZMK_KEYMAP_LAYERS_LEN && i < 32; i++) {
        uint8_t id = zmk_keymap_layer_index_to_id(i);
        if (id < 32 && (raw & BIT(id))) { mask |= BIT(i); highest = i; }
    }
    payload[2] = highest;
    payload[3] = 0;
    sys_put_le32(mask, &payload[4]);
    sys_put_le32((uint32_t)atomic_get(&sequence), &payload[8]);
    if (active) bt_conn_unref(active);
}

static ssize_t read_state(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset) {
    uint8_t payload[12];
    encode_state(conn, payload);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, payload, sizeof(payload));
}

BT_GATT_SERVICE_DEFINE(tomkey_layer_service,
    BT_GATT_PRIMARY_SERVICE(&service_uuid),
    BT_GATT_CHARACTERISTIC(&state_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                            BT_GATT_PERM_READ_ENCRYPT, read_state, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT)
);

static void notify_state(struct k_work *work) {
    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!conn) return;
    if (bt_gatt_is_subscribed(conn, &tomkey_layer_service.attrs[2], BT_GATT_CCC_NOTIFY)) {
        uint8_t payload[12];
        encode_state(conn, payload);
        bt_gatt_notify(conn, &tomkey_layer_service.attrs[2], payload, sizeof(payload));
    }
    bt_conn_unref(conn);
}
K_WORK_DEFINE(notify_work, notify_state);

static int layer_changed(const zmk_event_t *event) {
    atomic_inc(&sequence);
    k_work_submit(&notify_work);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(tomkey_layer_state, layer_changed);
ZMK_SUBSCRIPTION(tomkey_layer_state, zmk_layer_state_changed);
