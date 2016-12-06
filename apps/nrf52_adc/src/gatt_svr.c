/*
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
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "bleadc.h"
#include "console/console.h"

static int
gatt_svr_chr_access_alert(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt,
                          void *arg);

static int
gatt_svr_sns_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt,
                          void *arg);

static uint16_t gatt_adc_val_len;
static uint16_t gatt_spi_val_len;
                          
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
        /*** Water Level Notification Service. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = gatt_svr_svc_sns_uuid,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid128 = BLE_UUID16(ADC_SNS_TYPE),
            .access_cb = gatt_svr_sns_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid128 = BLE_UUID16(ADC_SNS_VAL),
            .access_cb = gatt_svr_sns_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            .uuid128 = BLE_UUID16(SPI_SNS_TYPE),
            .access_cb = gatt_svr_sns_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid128 = BLE_UUID16(SPI_SNS_VAL),
            .access_cb = gatt_svr_sns_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
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
gatt_svr_sns_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt,
                          void *arg)
{
    uint16_t uuid16;
    int rc;

    uuid16 = ble_uuid_128_to_16(ctxt->chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case ADC_SNS_TYPE:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, ADC_SNS_STRING, sizeof ADC_SNS_STRING);
        BLEADC_LOG(INFO, "ADC SENSOR TYPE READ: %s\n", ADC_SNS_STRING);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        
    case SPI_SNS_TYPE:
        BLEADC_LOG(INFO, "SPI SENSOR TYPE READ: %s\n", SPI_SNS_STRING);
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, SPI_SNS_STRING, sizeof SPI_SNS_STRING);
        
        
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case ADC_SNS_VAL:
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(ctxt->om, 0,
                                    sizeof gatt_adc_val,
                                    &gatt_adc_val,
                                    &gatt_adc_val_len);
            return rc;
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            rc = os_mbuf_append(ctxt->om, &gatt_adc_val,
                                sizeof gatt_adc_val);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    case SPI_SNS_VAL:
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(ctxt->om, 0,
                                sizeof gatt_spi_val,
                                        &gatt_spi_val,
                                        &gatt_spi_val_len);
                return rc;
            } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
                rc = os_mbuf_append(ctxt->om, &gatt_spi_val,
                                    sizeof gatt_spi_val);
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }                              
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
        BLEADC_LOG(DEBUG, "registered service %s with handle=%d\n",
                    gatt_svr_uuid_to_s(ctxt->svc.svc_def->uuid128, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        BLEADC_LOG(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    gatt_svr_uuid_to_s(ctxt->chr.chr_def->uuid128, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        BLEADC_LOG(DEBUG, "registering descriptor %s with handle=%d\n",
                    gatt_svr_uuid_to_s(ctxt->dsc.dsc_def->uuid128, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
