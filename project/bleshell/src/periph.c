#include <assert.h>
#include <string.h>
#include "console/console.h"
#include "host/ble_hs.h"

#define PERIPH_SVC1_UUID      0x1234
#define PERIPH_SVC2_UUID      0x5678
#define PERIPH_CHR1_UUID      0x1111
#define PERIPH_CHR2_UUID      0x1112
#define PERIPH_CHR3_UUID      0x5555

static int
periph_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
               union ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def periph_svcs[] = {
    [0] = {
        /*** Service 0x1234. */
        .type = BLE_GATT_SVC_TYPE_SECONDARY,
        .uuid128 = BLE_UUID16(PERIPH_SVC1_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic 0x1111. */
            .uuid128 = BLE_UUID16(PERIPH_CHR1_UUID),
            .access_cb = periph_gatt_cb,
            .flags = 0,
        }, {
            /*** Characteristic 0x1112. */
            .uuid128 = BLE_UUID16(PERIPH_CHR2_UUID),
            .access_cb = periph_gatt_cb,
            .flags = 0,
        }, {
            .uuid128 = NULL, /* No more characteristics in this service. */
        } },
    },

    [1] = {
        /*** Service 0x5678. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(PERIPH_SVC2_UUID),
        .includes = (const struct ble_gatt_svc_def *[]) {
            &periph_svcs[0],
            NULL,
        },
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic 0x5555. */
            .uuid128 = BLE_UUID16(PERIPH_CHR3_UUID),
            .access_cb = periph_gatt_cb,
            .flags = 0,
        }, {
            .uuid128 = NULL, /* No more characteristics in this service. */
        } },
    },

    {
        .type = BLE_GATT_SVC_TYPE_END, /* No more services. */
    },
};

static int
periph_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
               union ble_gatt_access_ctxt *ctxt, void *arg)
{
    static uint8_t buf[128];
    uint16_t uuid16;

    assert(op == BLE_GATT_ACCESS_OP_READ_CHR);

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    switch (uuid16) {
    case PERIPH_CHR1_UUID:
        console_printf("reading characteristic1 value\n");
        memcpy(buf, "char1", 5);
        ctxt->chr_access.len = 5;
        break;

    case PERIPH_CHR2_UUID:
        console_printf("reading characteristic2 value\n");
        memcpy(buf, "char2", 5);
        ctxt->chr_access.len = 5;
        break;

    case PERIPH_CHR3_UUID:
        console_printf("reading characteristic3 value\n");
        memcpy(buf, "char3", 5);
        ctxt->chr_access.len = 5;
        break;

    default:
        assert(0);
        break;
    }

    ctxt->chr_access.data = buf;

    return 0;
}

static void
periph_register_cb(uint8_t op, union ble_gatt_register_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    switch (op) {
    case BLE_GATT_REGISTER_OP_SVC:
        uuid16 = ble_uuid_128_to_16(ctxt->svc_reg.svc->uuid128);
        assert(uuid16 != 0);
        console_printf("registered service 0x%04x with handle=%d\n",
                       uuid16, ctxt->svc_reg.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        uuid16 = ble_uuid_128_to_16(ctxt->chr_reg.chr->uuid128);
        assert(uuid16 != 0);
        console_printf("registering characteristic 0x%04x with def_handle=%d "
                       "val_handle=%d\n",
                       uuid16, ctxt->chr_reg.def_handle,
                       ctxt->chr_reg.val_handle);
        break;

    default:
        assert(0);
        break;
    }
}

void
periph_init(void)
{
    int rc;

    rc = ble_gatts_register_svcs(periph_svcs, periph_register_cb, NULL);
    assert(rc == 0);
}
