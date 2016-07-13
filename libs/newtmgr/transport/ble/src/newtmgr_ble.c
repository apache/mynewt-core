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
#include "host/ble_hs.h"
#include <newtmgr/newtmgr.h>
#include <os/endian.h>

#define BLE_ATT_MTU_MAX 240

/* nmgr ble mqueue */
struct os_mqueue ble_nmgr_mq;

/* ble nmgr transport */
struct nmgr_transport ble_nt;

/* ble nmgr attr handle */
uint16_t g_ble_nmgr_attr_handle;

/* ble nmgr response buffer */
char ble_nmgr_resp_buf[BLE_ATT_MTU_MAX];

struct os_eventq *app_evq;
/**
 * The vendor specific "newtmgr" service consists of one write no-rsp characteristic
 * for newtmgr requests
 *     o "write no-rsp": a single-byte characteristic that can always be read, but
 *       can only be written over an encrypted connection.
 */

/* {8D53DC1D-1DB7-4CD3-868B-8A527460AA84} */
const uint8_t gatt_svr_svc_newtmgr[16] = {
    0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
    0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d
};

/* {DA2E7828-FBCE-4E01-AE9E-261174997C48} */
const uint8_t gatt_svr_chr_newtmgr[16] = {
    0x48, 0x7c, 0x99, 0x74, 0x11, 0x26, 0x9e, 0xae,
    0x01, 0x4e, 0xce, 0xfb, 0x28, 0x78, 0x2e, 0xda
};

static int
gatt_svr_chr_access_newtmgr(uint16_t conn_handle, uint16_t attr_handle,
                            uint8_t op, union ble_gatt_access_ctxt *ctxt,
                            void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* Service: newtmgr */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = (void *)gatt_svr_svc_newtmgr,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Characteristic: Write No Rsp */
            .uuid128 = (void *)gatt_svr_chr_newtmgr,
            .access_cb = gatt_svr_chr_access_newtmgr,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
            0, /* No more characteristics in this service */
        } },
    },

    {
        0, /* No more services */
    },
};

static int
gatt_svr_chr_access_newtmgr(uint16_t conn_handle, uint16_t attr_handle,
                            uint8_t op, union ble_gatt_access_ctxt *ctxt,
                            void *arg)
{
    int rc;
    struct os_mbuf *m_req;

    switch (op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            m_req = os_msys_get_pkthdr(ctxt->chr_access.len, sizeof(conn_handle));
            if (!m_req) {
                goto err;
            }
            rc = os_mbuf_copyinto(m_req, OS_MBUF_PKTHDR(m_req)->omp_len,
                                  ctxt->chr_access.data, ctxt->chr_access.len);
            if (rc) {
                goto err;
            }
            memcpy(OS_MBUF_USRHDR(m_req), &conn_handle, sizeof(conn_handle));

            rc = nmgr_rx_req(&ble_nt, m_req);
            return rc;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
    assert(0);
err:
    return BLE_ATT_ERR_UNLIKELY;
}

static void
gatt_svr_register_cb(uint8_t op, union ble_gatt_register_ctxt *ctxt, void *arg)
{

    switch (op) {
    case BLE_GATT_REGISTER_OP_DSC:
    case BLE_GATT_REGISTER_OP_SVC:
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        /* Searching for the newtmgr characteristic */
        if (!memcmp(ctxt->chr_reg.chr->uuid128, gatt_svr_chr_newtmgr, 16)) {
            g_ble_nmgr_attr_handle = ctxt->chr_reg.val_handle;
        }
        break;

    default:
        assert(0);
        break;
    }
}

int
nmgr_ble_proc_mq_evt(struct os_event *ev)
{
    struct os_mbuf *m_resp;
    uint16_t conn_handle;
    uint16_t max_pktlen;
    int rc;

    rc = 0;
    switch (ev->ev_type) {

        case OS_EVENT_T_MQUEUE_DATA:
            if (ev->ev_arg != &ble_nmgr_mq) {
                rc = -1;
                goto done;
            }

            while (1) {
                m_resp = os_mqueue_get(&ble_nmgr_mq);
                if (!m_resp) {
                    break;
                }
                memcpy(&conn_handle, OS_MBUF_USRHDR(m_resp),
                       sizeof(conn_handle));
                max_pktlen = min(OS_MBUF_PKTLEN(m_resp), sizeof(ble_nmgr_resp_buf));
                rc = os_mbuf_copydata(m_resp, 0, max_pktlen, ble_nmgr_resp_buf);
                assert(rc == 0);
                rc = os_mbuf_free_chain(m_resp);
                assert(rc == 0);
                ble_gattc_notify_custom(conn_handle, g_ble_nmgr_attr_handle,
                                        ble_nmgr_resp_buf, max_pktlen);
            }
            break;
        default:
            rc = -1;
            goto done;
    }

done:
    return rc;
}

static int
nmgr_ble_out(struct nmgr_transport *nt, struct os_mbuf *m)
{
    int rc;

    rc = os_mqueue_put(&ble_nmgr_mq, app_evq, m);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

void
nmgr_ble_gatt_svr_init(struct os_eventq *evq)
{
    int rc;

    assert(evq != NULL);

    rc = ble_gatts_register_svcs(gatt_svr_svcs, gatt_svr_register_cb, NULL);
    assert(rc == 0);

    app_evq = evq;

    os_mqueue_init(&ble_nmgr_mq, &ble_nmgr_mq);

    nmgr_transport_init(&ble_nt, &nmgr_ble_out);

}

