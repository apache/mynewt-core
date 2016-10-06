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
#include <stdio.h>
#include <string.h>
#include "bsp/bsp.h"
#include "console/console.h"
#include "host/ble_hs.h"
#include "bletiny.h"

/**
 * The vendor specific security test service consists of two characteristics:
 *     o random-number-generator: generates a random 32-bit number each time
 *       it is read.  This characteristic can only be read over an encrypted
 *       connection.
 *     o static-value: a single-byte characteristic that can always be read,
 *       but can only be written over an encrypted connection.
 */

/* 59462f12-9543-9999-12c8-58b459a2712d */
const uint8_t gatt_svr_svc_sec_test_uuid[16] = {
    0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
    0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59
};

/* 5c3a659e-897e-45e1-b016-007107c96df6 */
const uint8_t gatt_svr_chr_sec_test_rand_uuid[16] = {
    0xf6, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
    0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c
};

/* 5c3a659e-897e-45e1-b016-007107c96df7 */
const uint8_t gatt_svr_chr_sec_test_static_uuid[16] = {
    0xf7, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
    0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c
};

static uint8_t gatt_svr_sec_test_static_val;

static int
gatt_svr_chr_access_alert(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt,
                          void *arg);

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
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
        /*** Service: Security test. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = gatt_svr_svc_sec_test_uuid,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Random number generator. */
            .uuid128 = gatt_svr_chr_sec_test_rand_uuid,
            .access_cb = gatt_svr_chr_access_sec_test,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
        }, {
            /*** Characteristic: Static value. */
            .uuid128 = gatt_svr_chr_sec_test_static_uuid,
            .access_cb = gatt_svr_chr_access_sec_test,
            .flags = BLE_GATT_CHR_F_READ |
                     BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
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
                          struct ble_gatt_access_ctxt *ctxt,
                          void *arg)
{
    uint16_t uuid16;
    int rc;

    uuid16 = ble_uuid_128_to_16(ctxt->chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, &gatt_svr_new_alert_cat,
                            sizeof gatt_svr_new_alert_cat);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case GATT_SVR_CHR_NEW_ALERT:
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(ctxt->om, 0,
                                    sizeof gatt_svr_new_alert_val,
                                    gatt_svr_new_alert_val,
                                    &gatt_svr_new_alert_val_len);
            return rc;
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            rc = os_mbuf_append(ctxt->om, &gatt_svr_new_alert_val,
                                sizeof gatt_svr_new_alert_val);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

    case GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, &gatt_svr_unr_alert_cat,
                            sizeof gatt_svr_unr_alert_cat);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case GATT_SVR_CHR_UNR_ALERT_STAT_UUID:
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(ctxt->om, 2, 2, &gatt_svr_unr_alert_stat,
                                    NULL);
            return rc;
        } else {
            rc = os_mbuf_append(ctxt->om, &gatt_svr_unr_alert_stat,
                                sizeof gatt_svr_unr_alert_stat);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

    case GATT_SVR_CHR_ALERT_NOT_CTRL_PT:
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(ctxt->om, 2, 2,
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

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    const void *uuid128;
    int rand_num;
    int rc;

    uuid128 = ctxt->chr->uuid128;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (memcmp(uuid128, gatt_svr_chr_sec_test_rand_uuid, 16) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        /* Respond with a 32-bit random number. */
        rand_num = rand();
        rc = os_mbuf_append(ctxt->om, &rand_num, sizeof rand_num);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (memcmp(uuid128, gatt_svr_chr_sec_test_static_uuid, 16) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            rc = os_mbuf_append(ctxt->om, &gatt_svr_sec_test_static_val,
                                sizeof gatt_svr_sec_test_static_val);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof gatt_svr_sec_test_static_val,
                                    sizeof gatt_svr_sec_test_static_val,
                                    &gatt_svr_sec_test_static_val, NULL);
            return rc;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static char *
gatt_svr_uuid_to_s(const void *uuid128, char *dst)
{
    const uint8_t *u8p;
    uint16_t uuid16;

    uuid16 = ble_uuid_128_to_16(uuid128);
    if (uuid16 != 0) {
        sprintf(dst, "0x%04x", uuid16);
        return dst;
    }

    u8p = uuid128;

    sprintf(dst,      "%02x%02x%02x%02x-", u8p[15], u8p[14], u8p[13], u8p[12]);
    sprintf(dst + 9,  "%02x%02x-%02x%02x-", u8p[11], u8p[10], u8p[9], u8p[8]);
    sprintf(dst + 19, "%02x%02x%02x%02x%02x%02x%02x%02x",
            u8p[7], u8p[6], u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);

    return dst;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[40];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        BLETINY_LOG(DEBUG, "registered service %s with handle=%d\n",
                    gatt_svr_uuid_to_s(ctxt->svc.svc_def->uuid128, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        BLETINY_LOG(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    gatt_svr_uuid_to_s(ctxt->chr.chr_def->uuid128, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        BLETINY_LOG(DEBUG, "registering descriptor %s with handle=%d\n",
                    gatt_svr_uuid_to_s(ctxt->dsc.dsc_def->uuid128, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_register(void)
{
    int rc;

    rc = ble_gatts_register_svcs(gatt_svr_svcs, gatt_svr_register_cb, NULL);
    return rc;
}

int
gatt_svr_init(struct ble_hs_cfg *cfg)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs, cfg);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
