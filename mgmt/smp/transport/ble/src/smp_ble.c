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
#include "smp/smp.h"
#include "console/console.h"
#include "mynewt_smp/smp.h"

/**
 * \addtogroup SMP
 * @{
 */

/* smp ble mqueue */
struct os_mqueue g_smp_ble_mq;

/* ble smp transport */
struct smp_transport g_smp_ble_transport;

/* ble smp attr handle */
uint16_t g_ble_smp_attr_handle;

/* Requests split over several writes, one per connection.  An entry is in
 * use while its reassembly buffer holds data.
 */
struct smp_ble_reasm {
    uint16_t conn_handle;
    struct smp_reasm reasm;

    /* Drops the partial request if no fragment arrives in time. */
    struct os_callout tmo;
};

static struct smp_ble_reasm smp_ble_reasm[MYNEWT_VAL(BLE_MAX_CONNECTIONS)];

#define SMP_BLE_REASM_TMO_TICKS                                               \
    os_time_ms_to_ticks32(MYNEWT_VAL(SMP_BLE_REASSEMBLY_TIMEOUT))

static struct ble_gap_event_listener smp_ble_gap_listener;

/**
 * The vendor specific "smp" service consists of one write no-rsp
 * characteristic for smp requests: a single-byte characteristic that can
 * only accepts write-without-response commands.  Each write command contains
 * an SMP request or a part of one; a request longer than the ATT MTU can be
 * split over several consecutive writes.  SMP responses are sent back in the
 * form of unsolicited notifications from the same characteristic.
 */

/* {8D53DC1D-1DB7-4CD3-868B-8A527460AA84} */
static const ble_uuid128_t gatt_svr_svc_smp =
    BLE_UUID128_INIT(0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
                     0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d);

/* {DA2E7828-FBCE-4E01-AE9E-261174997C48} */
static const ble_uuid128_t gatt_svr_chr_smp =
    BLE_UUID128_INIT(0x48, 0x7c, 0x99, 0x74, 0x11, 0x26, 0x9e, 0xae,
                     0x01, 0x4e, 0xce, 0xfb, 0x28, 0x78, 0x2e, 0xda);

static int
gatt_svr_chr_access_smp(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* Service: smp */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_smp.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Characteristic: Write No Rsp */
            .uuid = &gatt_svr_chr_smp.u,
            .access_cb = gatt_svr_chr_access_smp,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &g_ble_smp_attr_handle,
        }, {
            0, /* No more characteristics in this service */
        } },
    },

    {
        0, /* No more services */
    },
};

/**
 * Returns reassembly entry holding a partial request from the specified
 * connection, or NULL if there is none.
 */
static struct smp_ble_reasm *
smp_ble_reasm_find(uint16_t conn_handle)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(smp_ble_reasm); i++) {
        if (smp_ble_reasm[i].reasm.sr_om != NULL &&
            smp_ble_reasm[i].conn_handle == conn_handle) {
            return &smp_ble_reasm[i];
        }
    }

    return NULL;
}

static struct smp_ble_reasm *
smp_ble_reasm_alloc(uint16_t conn_handle)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(smp_ble_reasm); i++) {
        if (smp_ble_reasm[i].reasm.sr_om == NULL) {
            smp_ble_reasm[i].conn_handle = conn_handle;
            return &smp_ble_reasm[i];
        }
    }

    return NULL;
}

static void
smp_ble_reasm_drop(struct smp_ble_reasm *entry)
{
    os_callout_stop(&entry->tmo);
    smp_reasm_reset(&entry->reasm);
}

/**
 * A client which stopped sending in the middle of a request would otherwise
 * keep its fragments buffered until disconnect.  The buffered mbufs may also
 * be needed by the controller to receive the remaining fragments, so the link
 * could not make progress anymore.
 */
static void
smp_ble_reasm_tmo(struct os_event *ev)
{
    smp_ble_reasm_drop(ev->ev_arg);
}

static int
smp_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct smp_ble_reasm *entry;

    if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        entry = smp_ble_reasm_find(event->disconnect.conn.conn_handle);
        if (entry != NULL) {
            smp_ble_reasm_drop(entry);
        }
    }

    return 0;
}

/**
 * Adds a written fragment to the partial request of the connection.
 *
 * @param conn_handle           Connection the fragment was received on.
 * @param frag                  The written data; not consumed.
 * @param out_req               On success, the complete request or NULL if
 *                                  more fragments are needed.
 *
 * @return                      0 on success; nonzero if the request was
 *                                  dropped.
 */
static int
smp_ble_reasm_rx(uint16_t conn_handle, struct os_mbuf *frag, struct os_mbuf **out_req)
{
    struct smp_ble_reasm *entry;

    int rc;

    entry = smp_ble_reasm_find(conn_handle);
    if (entry == NULL) {
        entry = smp_ble_reasm_alloc(conn_handle);
        if (entry == NULL) {
            return MGMT_ERR_ENOMEM;
        }
    }

    rc = smp_reasm_rx(&entry->reasm, frag, sizeof(conn_handle), out_req);

    if (entry->reasm.sr_om == NULL) {
        os_callout_stop(&entry->tmo);
    } else if (MYNEWT_VAL(SMP_BLE_REASSEMBLY_TIMEOUT) > 0) {
        os_callout_reset(&entry->tmo, SMP_BLE_REASM_TMO_TICKS);
    }

    return rc;
}

/**
 * Turns a write holding a complete request into an smp request mbuf.
 *
 * @return                      The request, or NULL on allocation failure.
 */
static struct os_mbuf *
smp_ble_req_from_write(struct ble_gatt_access_ctxt *ctxt)
{
    struct os_mbuf *m_req;
    uint16_t conn_handle;
    int rc;

    /* Try to reuse the BLE packet mbuf as the smp request.  This
     * requires a two-byte usrhdr to hold the BLE connection handle so
     * that the smp response can be sent to the correct peer.  If
     * it is not possible to reuse the mbuf, then allocate a new one
     * and copy the request contents.
     */
    if (OS_MBUF_USRHDR_LEN(ctxt->om) >= sizeof(conn_handle)) {
        /* Sufficient usrhdr space already present. */
        m_req = ctxt->om;
        ctxt->om = NULL;
    } else if (OS_MBUF_LEADINGSPACE(ctxt->om) >= sizeof(conn_handle)) {

        /* Usrhdr isn't present, but there is enough leading space to
         * add one.
         */
        m_req = ctxt->om;
        ctxt->om = NULL;

        m_req->om_pkthdr_len += sizeof(conn_handle);
    } else {
        /* The mbuf can't be reused.  Allocate a new one and perform a
         * copy.  Don't set ctxt->om to NULL; let the NimBLE host free
         * it.
         */
        m_req = os_msys_get_pkthdr(OS_MBUF_PKTLEN(ctxt->om), sizeof(conn_handle));
        if (!m_req) {
            return NULL;
        }
        rc = os_mbuf_appendfrom(m_req, ctxt->om, 0, OS_MBUF_PKTLEN(ctxt->om));
        if (rc) {
            os_mbuf_free_chain(m_req);
            return NULL;
        }
    }

    return m_req;
}

static int
gatt_svr_chr_access_smp(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    int req_len;
    struct os_mbuf *m_req;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            req_len = smp_reasm_req_len(ctxt->om);
            if (smp_ble_reasm_find(conn_handle) == NULL && req_len != 0 &&
                req_len <= OS_MBUF_PKTLEN(ctxt->om)) {

                /* Complete request in a single write. */
                m_req = smp_ble_req_from_write(ctxt);
                if (!m_req) {
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }
            } else {
                /* Part of a request split over several writes. */
                rc = smp_ble_reasm_rx(conn_handle, ctxt->om, &m_req);
                if (rc) {
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }
                if (!m_req) {
                    return 0;
                }
            }

            /* Write the connection handle to the smp request usrhdr.  This
             * is necessary so that we later know who to send the smp
             * response to.
             */
            memcpy(OS_MBUF_USRHDR(m_req), &conn_handle, sizeof(conn_handle));

            rc = smp_rx_req(&g_smp_ble_transport, m_req);
            if (rc) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            
            return 0;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
    }
}

uint16_t
smp_ble_get_mtu(struct os_mbuf *req) {

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
 * SMP ble process mqueue event
 * Gets an event from the smp mqueue and does a notify with the response
 *
 * @param eventq
 * @return 0 on success; non-zero on failure
 */

static void
smp_ble_event_data_in(struct os_event *ev)
{
    struct os_mbuf *m_resp;
    uint16_t conn_handle;

    while ((m_resp = os_mqueue_get(&g_smp_ble_mq)) != NULL) {
        assert(OS_MBUF_USRHDR_LEN(m_resp) >= sizeof (conn_handle));
        memcpy(&conn_handle, OS_MBUF_USRHDR(m_resp), sizeof (conn_handle));
        ble_gatts_notify_custom(conn_handle, g_ble_smp_attr_handle, m_resp);
    }
}

static int
smp_ble_out(struct os_mbuf *om)
{
    int rc;

    rc = os_mqueue_put(&g_smp_ble_mq, os_eventq_dflt_get(), om);
    if (rc != 0) {
        goto err;
    }

    return 0;
err:
    os_mbuf_free_chain(om);
    return rc;
}

/**
 * SMP ble GATT server initialization
 *
 * @param eventq
 * @return 0 on success; non-zero on failure
 */
int
smp_ble_gatt_svr_init(void)
{
    int rc;
    int i;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        goto err;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gap_event_listener_register(&smp_ble_gap_listener,
                                         smp_ble_gap_event, NULL);
    if (rc != 0) {
        return rc;
    }

    /* Timeouts run on the default event queue, like the NimBLE host which
     * also accesses the reassembly state.
     */
    for (i = 0; i < ARRAY_SIZE(smp_ble_reasm); i++) {
        os_callout_init(&smp_ble_reasm[i].tmo, os_eventq_dflt_get(),
                        smp_ble_reasm_tmo, &smp_ble_reasm[i]);
    }

    os_mqueue_init(&g_smp_ble_mq, &smp_ble_event_data_in, NULL);

    rc = smp_transport_init(&g_smp_ble_transport, smp_ble_out, smp_ble_get_mtu);

err:
    return rc;
}

void
smp_ble_pkg_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = smp_ble_gatt_svr_init();
    SYSINIT_PANIC_ASSERT(rc == 0);
}

/**
 * @} SMP
 */
