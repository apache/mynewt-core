#include <assert.h>
#include <string.h>
#include "console/console.h"
#include "host/ble_hs.h"
#include "bleshell_priv.h"

#define PERIPH_SVC1_UUID      0x1234
#define PERIPH_SVC2_UUID      0x5678
#define PERIPH_CHR1_UUID      0x1111
#define PERIPH_CHR2_UUID      0x1112
#define PERIPH_CHR3_UUID      0x5555

#define CHR_F_FULL_ACCESS (BLE_GATT_CHR_F_READ              |   \
                           BLE_GATT_CHR_F_WRITE_NO_RSP      |   \
                           BLE_GATT_CHR_F_WRITE             |   \
                           BLE_GATT_CHR_F_NOTIFY            |   \
                           BLE_GATT_CHR_F_INDICATE)

#define PERIPH_CHR_MAX_LEN  16


static uint8_t periph_chr_data[3][PERIPH_CHR_MAX_LEN];
static uint16_t periph_chr_lens[3];

static int
periph_chr_access_gap(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                      union ble_gatt_access_ctxt *ctxt, void *arg);
static int
periph_chr_access_gatt(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                       union ble_gatt_access_ctxt *ctxt, void *arg);

static int
periph_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
               union ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def periph_svcs[] = {
    [0] = {
        /*** Service: GAP. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(BLE_GAP_SVC_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Device Name. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_DEVICE_NAME),
            .access_cb = periph_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Appearance. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_APPEARANCE),
            .access_cb = periph_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Peripheral Privacy Flag. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_PERIPH_PRIV_FLAG),
            .access_cb = periph_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Reconnection Address. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_RECONNECT_ADDR),
            .access_cb = periph_chr_access_gap,
            .flags = BLE_GATT_CHR_F_WRITE,
        }, {
            /*** Characteristic: Peripheral Preferred Connection Parameters. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS),
            .access_cb = periph_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    [1] = {
        /*** Service: GATT */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(BLE_GATT_SVC_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid128 = BLE_UUID16(BLE_GATT_CHR_SERVICE_CHANGED_UUID16),
            .access_cb = periph_chr_access_gatt,
            .flags = BLE_GATT_CHR_F_INDICATE,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    [2] = {
        /*** Service 0x1234. */
        .type = BLE_GATT_SVC_TYPE_SECONDARY,
        .uuid128 = BLE_UUID16(PERIPH_SVC1_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic 0x1111. */
            .uuid128 = BLE_UUID16(PERIPH_CHR1_UUID),
            .access_cb = periph_gatt_cb,
            .flags = CHR_F_FULL_ACCESS,
        }, {
            /*** Characteristic 0x1112. */
            .uuid128 = BLE_UUID16(PERIPH_CHR2_UUID),
            .access_cb = periph_gatt_cb,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    [3] = {
        /*** Service 0x5678. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(PERIPH_SVC2_UUID),
        .includes = (const struct ble_gatt_svc_def *[]) {
            &periph_svcs[2],
            NULL,
        },
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic 0x5555. */
            .uuid128 = BLE_UUID16(PERIPH_CHR3_UUID),
            .access_cb = periph_gatt_cb,
            .flags = CHR_F_FULL_ACCESS,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};

static int
periph_gatt_read(uint16_t attr_handle, union ble_gatt_access_ctxt *ctxt,
                 void *arg)
{
    uint16_t uuid16;
    int idx;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    switch (uuid16) {
    case PERIPH_CHR1_UUID:
        idx = 0;
        break;

    case PERIPH_CHR2_UUID:
        idx = 1;
        break;

    case PERIPH_CHR3_UUID:
        idx = 2;
        break;

    default:
        assert(0);
        break;
    }

    ctxt->chr_access.data = periph_chr_data[idx];
    ctxt->chr_access.len = periph_chr_lens[idx];

    return 0;
}

static int
periph_gatt_write(uint16_t attr_handle, union ble_gatt_access_ctxt *ctxt,
                  void *arg)
{
    uint16_t uuid16;
    int idx;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    switch (uuid16) {
    case PERIPH_CHR1_UUID:
        idx = 0;
        break;

    case PERIPH_CHR2_UUID:
        idx = 1;
        break;

    case PERIPH_CHR3_UUID:
        idx = 2;
        break;

    default:
        assert(0);
        break;
    }

    if (ctxt->chr_access.len > sizeof periph_chr_data[idx]) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    memcpy(periph_chr_data[idx], ctxt->chr_access.data, ctxt->chr_access.len);
    periph_chr_lens[idx] = ctxt->chr_access.len;

    return 0;
}

static int
periph_chr_access_gap(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                      union ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case BLE_GAP_CHR_UUID16_DEVICE_NAME:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)bleshell_device_name;
        ctxt->chr_access.len = strlen(bleshell_device_name);
        break;

    case BLE_GAP_CHR_UUID16_APPEARANCE:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&bleshell_appearance;
        ctxt->chr_access.len = sizeof bleshell_appearance;
        break;

    case BLE_GAP_CHR_UUID16_PERIPH_PRIV_FLAG:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&bleshell_privacy_flag;
        ctxt->chr_access.len = sizeof bleshell_privacy_flag;
        break;

    case BLE_GAP_CHR_UUID16_RECONNECT_ADDR:
        assert(op == BLE_GATT_ACCESS_OP_WRITE_CHR);
        if (ctxt->chr_access.len != sizeof bleshell_reconnect_addr) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        memcpy(bleshell_reconnect_addr, ctxt->chr_access.data,
               sizeof bleshell_reconnect_addr);
        break;

    case BLE_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&bleshell_pref_conn_params;
        ctxt->chr_access.len = sizeof bleshell_pref_conn_params;
        break;

    default:
        assert(0);
        break;
    }

    return 0;
}

static int
periph_chr_access_gatt(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                       union ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case BLE_GAP_CHR_UUID16_DEVICE_NAME:
        assert(op == BLE_GATT_ACCESS_OP_WRITE_CHR);
        if (ctxt->chr_access.len != sizeof bleshell_gatt_service_changed) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        memcpy(bleshell_gatt_service_changed, ctxt->chr_access.data,
               sizeof bleshell_gatt_service_changed);
        break;

    default:
        assert(0);
        break;
    }

    return 0;
}

static int
periph_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
               union ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        return periph_gatt_read(attr_handle, ctxt, arg);

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        return periph_gatt_write(attr_handle, ctxt, arg);

    default:
        assert(0);
        return -1;
    }
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

    case BLE_GATT_REGISTER_OP_DSC:
        uuid16 = ble_uuid_128_to_16(ctxt->dsc_reg.dsc->uuid128);
        assert(uuid16 != 0);
        console_printf("registering descriptor 0x%04x with handle=%d "
                       "chr_handle=%d\n",
                       uuid16, ctxt->dsc_reg.dsc_handle,
                       ctxt->dsc_reg.chr_def_handle);
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

    strcpy((char *)periph_chr_data[0], "hello0");
    periph_chr_lens[0] = strlen((char *)periph_chr_data[0]);
    strcpy((char *)periph_chr_data[1], "hello1");
    periph_chr_lens[1] = strlen((char *)periph_chr_data[1]);
    strcpy((char *)periph_chr_data[2], "hello2");
    periph_chr_lens[2] = strlen((char *)periph_chr_data[2]);

    rc = ble_gatts_register_svcs(periph_svcs, periph_register_cb, NULL);
    assert(rc == 0);
}
