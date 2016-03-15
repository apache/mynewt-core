/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include "bsp/bsp.h"
#include "console/console.h"
#include "host/ble_hs.h"
#include "bleprph.h"

#define GATT_SVR_SVC_ALERT_UUID               0x1811
#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID   0x2A47
#define GATT_SVR_CHR_NEW_ALERT                0x2A46
#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID   0x2A48
#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID      0x2A45
#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT        0x2A44

static int
gatt_svr_chr_access_gap(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                        union ble_gatt_access_ctxt *ctxt, void *arg);
static int
gatt_svr_chr_access_gatt(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                         union ble_gatt_access_ctxt *ctxt, void *arg);
static int
gatt_svr_chr_access_alert(uint16_t conn_handle, uint16_t attr_handle,
                          uint8_t op, union ble_gatt_access_ctxt *ctxt,
                          void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: GAP. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(BLE_GAP_SVC_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Device Name. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_DEVICE_NAME),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Appearance. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_APPEARANCE),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Peripheral Privacy Flag. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_PERIPH_PRIV_FLAG),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Reconnection Address. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_RECONNECT_ADDR),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_WRITE,
        }, {
            /*** Characteristic: Peripheral Preferred Connection Parameters. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        /*** Service: GATT */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(BLE_GATT_SVC_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid128 = BLE_UUID16(BLE_GATT_CHR_SERVICE_CHANGED_UUID16),
            .access_cb = gatt_svr_chr_access_gatt,
            .flags = BLE_GATT_CHR_F_INDICATE,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        /*** Alert Notification Service. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(GATT_SVR_SVC_ALERT_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID),
            .access_cb = gatt_svr_chr_access_alert,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_NEW_ALERT),
            .access_cb = gatt_svr_chr_access_alert,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID),
            .access_cb = gatt_svr_chr_access_alert,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_UNR_ALERT_STAT_UUID),
            .access_cb = gatt_svr_chr_access_alert,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_ALERT_NOT_CTRL_PT),
            .access_cb = gatt_svr_chr_access_alert,
            .flags = BLE_GATT_CHR_F_WRITE,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};

static int
gatt_svr_chr_write(uint8_t op, union ble_gatt_access_ctxt *ctxt,
                   uint16_t min_len, uint16_t max_len, void *dst,
                   uint16_t *len)
{
    assert(op == BLE_GATT_ACCESS_OP_WRITE_CHR);
    if (ctxt->chr_access.len < min_len ||
        ctxt->chr_access.len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    memcpy(dst, ctxt->chr_access.data, ctxt->chr_access.len);
    if (len != NULL) {
        *len = ctxt->chr_access.len;
    }

    return 0;
}

static int
gatt_svr_chr_access_gap(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                        union ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case BLE_GAP_CHR_UUID16_DEVICE_NAME:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)bleprph_device_name;
        ctxt->chr_access.len = strlen(bleprph_device_name);
        break;

    case BLE_GAP_CHR_UUID16_APPEARANCE:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&bleprph_appearance;
        ctxt->chr_access.len = sizeof bleprph_appearance;
        break;

    case BLE_GAP_CHR_UUID16_PERIPH_PRIV_FLAG:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&bleprph_privacy_flag;
        ctxt->chr_access.len = sizeof bleprph_privacy_flag;
        break;

    case BLE_GAP_CHR_UUID16_RECONNECT_ADDR:
        assert(op == BLE_GATT_ACCESS_OP_WRITE_CHR);
        if (ctxt->chr_access.len != sizeof bleprph_reconnect_addr) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        memcpy(bleprph_reconnect_addr, ctxt->chr_access.data,
               sizeof bleprph_reconnect_addr);
        break;

    case BLE_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&bleprph_pref_conn_params;
        ctxt->chr_access.len = sizeof bleprph_pref_conn_params;
        break;

    default:
        assert(0);
        break;
    }

    return 0;
}

static int
gatt_svr_chr_access_gatt(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                         union ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case BLE_GATT_CHR_SERVICE_CHANGED_UUID16:
        if (op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            if (ctxt->chr_access.len != sizeof bleprph_gatt_service_changed) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            memcpy(bleprph_gatt_service_changed, ctxt->chr_access.data,
                   sizeof bleprph_gatt_service_changed);
        } else if (op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ctxt->chr_access.data = (void *)&bleprph_gatt_service_changed;
            ctxt->chr_access.len = sizeof bleprph_gatt_service_changed;
        }
        break;

    default:
        assert(0);
        break;
    }

    return 0;
}

#define GATT_SVR_NEW_ALERT_VAL_MAX_LEN    64

static const uint8_t gatt_svr_new_alert_cat = 0x01; /* Simple alert. */
static uint8_t gatt_svr_new_alert_val[GATT_SVR_NEW_ALERT_VAL_MAX_LEN];
static uint16_t gatt_svr_new_alert_val_len;
static const uint8_t gatt_svr_unr_alert_cat = 0x01; /* Simple alert. */
static uint16_t gatt_svr_unr_alert_stat;
static uint16_t gatt_svr_alert_not_ctrl_pt;

static int
gatt_svr_chr_access_alert(uint16_t conn_handle, uint16_t attr_handle,
                          uint8_t op, union ble_gatt_access_ctxt *ctxt,
                          void *arg)
{
    uint16_t uuid16;
    int rc;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&gatt_svr_new_alert_cat;
        ctxt->chr_access.len = sizeof gatt_svr_new_alert_cat;
        return 0;

    case GATT_SVR_CHR_NEW_ALERT:
        if (op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(op, ctxt, 0, sizeof gatt_svr_new_alert_val,
                                  gatt_svr_new_alert_val,
                                  &gatt_svr_new_alert_val_len);
            return rc;
        } else if (op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ctxt->chr_access.data = (void *)&gatt_svr_new_alert_val;
            ctxt->chr_access.len = sizeof gatt_svr_new_alert_val;
            return 0;
        }

    case GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&gatt_svr_unr_alert_cat;
        ctxt->chr_access.len = sizeof gatt_svr_unr_alert_cat;
        return 0;

    case GATT_SVR_CHR_UNR_ALERT_STAT_UUID:
        if (op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(op, ctxt, 2, 2, &gatt_svr_unr_alert_stat,
                                    NULL);
        } else {
            ctxt->chr_access.data = (void *)&gatt_svr_unr_alert_stat;
            ctxt->chr_access.len = sizeof gatt_svr_unr_alert_stat;
            rc = 0;
        }
        return rc;

    case GATT_SVR_CHR_ALERT_NOT_CTRL_PT:
        if (op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(op, ctxt, 2, 2,
                                    &gatt_svr_alert_not_ctrl_pt, NULL);
        } else {
            rc = BLE_ATT_ERR_UNLIKELY;
        }
        return rc;

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static void
gatt_svr_register_cb(uint8_t op, union ble_gatt_register_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    switch (op) {
    case BLE_GATT_REGISTER_OP_SVC:
        uuid16 = ble_uuid_128_to_16(ctxt->svc_reg.svc->uuid128);
        assert(uuid16 != 0);
        BLEPRPH_LOG(DEBUG, "registered service 0x%04x with handle=%d\n",
                    uuid16, ctxt->svc_reg.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        uuid16 = ble_uuid_128_to_16(ctxt->chr_reg.chr->uuid128);
        assert(uuid16 != 0);
        BLEPRPH_LOG(DEBUG, "registering characteristic 0x%04x with "
                           "def_handle=%d val_handle=%d\n",
                    uuid16, ctxt->chr_reg.def_handle,
                    ctxt->chr_reg.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        uuid16 = ble_uuid_128_to_16(ctxt->dsc_reg.dsc->uuid128);
        assert(uuid16 != 0);
        BLEPRPH_LOG(DEBUG, "registering descriptor 0x%04x with handle=%d "
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
gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_register_svcs(gatt_svr_svcs, gatt_svr_register_cb, NULL);
    assert(rc == 0);
}
