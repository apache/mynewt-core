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
#include "os/mynewt.h"
#include "host/ble_hs.h"
#include "mgmt/mgmt.h"
#include "newtmgr/newtmgr.h"
#include "console/console.h"

/* nmgr ble mqueue */
struct os_mqueue nmgr_ble_mq;

/* ble nmgr transport */
struct nmgr_transport ble_nt;

/* ble nmgr attr handle */
uint16_t g_ble_nmgr_attr_handle;

/**
 * The vendor specific "newtmgr" service consists of one write no-rsp
 * characteristic for newtmgr requests: a single-byte characteristic that can
 * only accepts write-without-response commands.  The contents of each write
 * command contains an NMP request.  NMP responses are sent back in the form of
 * unsolicited notifications from the same characteristic.
 */

/* {8D53DC1D-1DB7-4CD3-868B-8A527460AA84} */
static const ble_uuid128_t gatt_svr_svc_newtmgr =
    BLE_UUID128_INIT(0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
                     0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d);

/* {DA2E7828-FBCE-4E01-AE9E-261174997C48} */
static const ble_uuid128_t gatt_svr_chr_newtmgr =
    BLE_UUID128_INIT(0x48, 0x7c, 0x99, 0x74, 0x11, 0x26, 0x9e, 0xae,
                     0x01, 0x4e, 0xce, 0xfb, 0x28, 0x78, 0x2e, 0xda);

static int
gatt_svr_chr_access_newtmgr(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* Service: newtmgr */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_newtmgr.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Characteristic: Write No Rsp */
            .uuid = &gatt_svr_chr_newtmgr.u,
            .access_cb = gatt_svr_chr_access_newtmgr,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &g_ble_nmgr_attr_handle,
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
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    struct os_mbuf *m_req;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            /* Try to reuse the BLE packet mbuf as the newtmgr request.  This
             * requires a two-byte usrhdr to hold the BLE connection handle so
             * that the newtmgr response can be sent to the correct peer.  If
             * it is not possible to reuse the mbuf, then allocate a new one
             * and copy the request contents.
             */
            if (OS_MBUF_USRHDR_LEN(ctxt->om) >= sizeof (conn_handle)) {
                /* Sufficient usrhdr space already present. */
                m_req = ctxt->om;
                ctxt->om = NULL;
            } else if (OS_MBUF_LEADINGSPACE(ctxt->om) >=
                       sizeof (conn_handle)) {

                /* Usrhdr isn't present, but there is enough leading space to
                 * add one.
                 */
                m_req = ctxt->om;
                ctxt->om = NULL;

                m_req->om_pkthdr_len += sizeof (conn_handle);
            } else {
                /* The mbuf can't be reused.  Allocate a new one and perform a
                 * copy.  Don't set ctxt->om to NULL; let the NimBLE host free
                 * it.
                 */
                m_req = os_msys_get_pkthdr(OS_MBUF_PKTLEN(ctxt->om),
                                           sizeof (conn_handle));
                if (!m_req) {
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }
                rc = os_mbuf_appendfrom(m_req, ctxt->om, 0,
                                        OS_MBUF_PKTLEN(ctxt->om));
                if (rc) {
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }
            }

            /* Write the connection handle to the newtmgr request usrhdr.  This
             * is necessary so that we later know who to send the newtmgr
             * response to.
             */
            memcpy(OS_MBUF_USRHDR(m_req), &conn_handle, sizeof(conn_handle));

            rc = nmgr_rx_req(&ble_nt, m_req);
            if (rc != 0) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            return 0;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
    }
}

uint16_t
nmgr_ble_get_mtu(struct os_mbuf *req) {

    uint16_t conn_handle;
    uint16_t mtu;

    memcpy(&conn_handle, OS_MBUF_USRHDR(req), sizeof (conn_handle));
    mtu = ble_att_mtu(conn_handle);
    if (!mtu) {
        /* No longer connected. */
        return 0;
    }

    /* 3 is the number of bytes for ATT notification base */
    mtu = mtu - 3;

    return (mtu);
}

/**
 * Nmgr ble process mqueue event
 * Gets an event from the nmgr mqueue and does a notify with the response
 *
 * @param eventq
 * @return 0 on success; non-zero on failure
 */

static void
nmgr_ble_event_data_in(struct os_event *ev)
{
    struct os_mbuf *m_resp;
    uint16_t conn_handle;

    while ((m_resp = os_mqueue_get(&nmgr_ble_mq)) != NULL) {
        assert(OS_MBUF_USRHDR_LEN(m_resp) >= sizeof (conn_handle));
        memcpy(&conn_handle, OS_MBUF_USRHDR(m_resp), sizeof (conn_handle));
        ble_gattc_notify_custom(conn_handle, g_ble_nmgr_attr_handle,
                                m_resp);
    }
}

static int
nmgr_ble_out(struct nmgr_transport *nt, struct os_mbuf *om)
{
    int rc;

    rc = os_mqueue_put(&nmgr_ble_mq, mgmt_evq_get(), om);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    os_mbuf_free_chain(om);
    return (rc);
}

/**
 * Nmgr ble GATT server initialization
 *
 * @param eventq
 * @return 0 on success; non-zero on failure
 */
int
nmgr_ble_gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        goto err;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    os_mqueue_init(&nmgr_ble_mq, &nmgr_ble_event_data_in, NULL);

    rc = nmgr_transport_init(&ble_nt, nmgr_ble_out, nmgr_ble_get_mtu);

err:
    return rc;
}

void
newtmgr_ble_pkg_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = nmgr_ble_gatt_svr_init();
    SYSINIT_PANIC_ASSERT(rc == 0);
}
