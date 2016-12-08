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

#include <syscfg/syscfg.h>
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
#include <assert.h>
#include <os/os.h>
#include <string.h>

#include <stats/stats.h>
#include "oic/oc_gatt.h"
#include "oic/oc_log.h"
#include "messaging/coap/coap.h"
#include "api/oc_buffer.h"
#include "port/oc_connectivity.h"
#include "adaptor.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* OIC Transport Profile GATT */

/* service UUID */
/* ADE3D529-C784-4F63-A987-EB69F70EE816 */
const uint8_t oc_gatt_svc_uuid[16] = {
    0x16, 0xe8, 0x0e, 0xf7, 0x69, 0xeb, 0x87, 0xa9,
    0x63, 0x4f, 0x84, 0xc7, 0x29, 0xd5, 0xe3, 0xad
};

/* request characteristic UUID */
/* AD7B334F-4637-4B86-90B6-9D787F03D218 */
static const uint8_t oc_gatt_req_chr_uuid[16] = {
    0x18, 0xd2, 0x03, 0x7f, 0x78, 0x9d, 0xb6, 0x90,
    0x86, 0x4b, 0x37, 0x46, 0x4f, 0x33, 0x7b, 0xad
};

/* response characteristic UUID */
/* E9241982-4580-42C4-8831-95048216B256 */
static const uint8_t oc_gatt_rsp_chr_uuid[16] = {
    0x56, 0xb2, 0x16, 0x82, 0x04, 0x95, 0x31, 0x88,
    0xc4, 0x42, 0x80, 0x45, 0x82, 0x19, 0x24, 0xe9
};

STATS_SECT_START(oc_ble_stats)
    STATS_SECT_ENTRY(iseg)
    STATS_SECT_ENTRY(ibytes)
    STATS_SECT_ENTRY(ierr)
    STATS_SECT_ENTRY(oseg)
    STATS_SECT_ENTRY(obytes)
    STATS_SECT_ENTRY(oerr)
STATS_SECT_END
static STATS_SECT_DECL(oc_ble_stats) oc_ble_stats;
STATS_NAME_START(oc_ble_stats)
    STATS_NAME(oc_ble_stats, iseg)
    STATS_NAME(oc_ble_stats, ibytes)
    STATS_NAME(oc_ble_stats, ierr)
    STATS_NAME(oc_ble_stats, oseg)
    STATS_NAME(oc_ble_stats, obytes)
    STATS_NAME(oc_ble_stats, oerr)
STATS_NAME_END(oc_ble_stats)

/* queue to hold mbufs until we get called from oic */
static struct os_mqueue oc_ble_coap_mq;
static STAILQ_HEAD(, os_mbuf_pkthdr) oc_ble_reass_q;

#if (MYNEWT_VAL(OC_SERVER) == 1)
/* ble nmgr attr handle */
static uint16_t oc_ble_coap_req_handle;
static uint16_t oc_ble_coap_rsp_handle;

static int oc_gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                   struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = { {
        /* Service: newtmgr */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = (void *)oc_gatt_svc_uuid,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* Characteristic: Request */
                .uuid128 = (void *)oc_gatt_req_chr_uuid,
                .access_cb = oc_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP |
                         BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &oc_ble_coap_req_handle,
            },{
                /* Characteristic: Response */
                .uuid128 = (void *)oc_gatt_rsp_chr_uuid,
                .access_cb = oc_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &oc_ble_coap_rsp_handle,
            },{
                0, /* No more characteristics in this service */
            }
        },
    },
    {
        0, /* No more services */
    },
};

static int
oc_gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    struct os_mbuf *m;
    int rc;
    (void) attr_handle; /* xxx req should only come in via req handle */

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        m = ctxt->om;

        /* stick the conn handle at the end of the frame -- we will
         * pull it out later */
        rc = os_mbuf_append(m, &conn_handle, sizeof(conn_handle));
        if (rc) {
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        rc = os_mqueue_put(&oc_ble_coap_mq, oc_evq_get(), m);
        if (rc) {
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        /* tell nimble we are keeping the mbuf */
        ctxt->om = NULL;

        break;
    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
    return 0;
}

static struct os_mbuf *
oc_attempt_rx_gatt(void)
{
    int rc;
    struct os_mbuf *m;
    struct os_mbuf *n;
    struct os_mbuf_pkthdr *pkt;
    struct oc_endpoint *oe;
    uint16_t conn_handle;

    LOG("oc_gatt attempt rx\n");

    /* get an mbuf from the queue */
    n = os_mqueue_get(&oc_ble_coap_mq);
    if (NULL == n) {
        ERROR("oc_gatt Woke for receive but found no mbufs\n");
        return NULL;
    }

    pkt = OS_MBUF_PKTHDR(n);

    STATS_INC(oc_ble_stats, iseg);
    STATS_INCN(oc_ble_stats, ibytes, pkt->omp_len);

    /* get the conn handle from the end of the message */
    rc = os_mbuf_copydata(n, pkt->omp_len - sizeof(conn_handle),
                          sizeof(conn_handle), &conn_handle);
    if (rc != 0) {
        ERROR("oc_gatt Failed to retrieve conn_handle from mbuf\n");
        goto rx_attempt_err;
    }

    /* trim conn_handle from the end */
    os_mbuf_adj(n, - sizeof(conn_handle));

    m = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint));
    if (!m) {
        ERROR("oc_gatt Could not allocate OC message buffer\n");
        goto rx_attempt_err;
    }
    OS_MBUF_PKTHDR(m)->omp_len = pkt->omp_len;
    SLIST_NEXT(m, om_next) = n;

    oe = OC_MBUF_ENDPOINT(m);

    oe->flags = GATT;
    oe->bt_addr.conn_handle = conn_handle;

    LOG("oc_gatt rx %x-%u\n", (unsigned)pkt, pkt->omp_len);

    return m;

    /* add the addr info to the message */
rx_attempt_err:
    STATS_INC(oc_ble_stats, ierr);
    os_mbuf_free_chain(n);
    return NULL;
}
#endif

int
oc_ble_coap_gatt_srv_init(void)
{
#if (MYNEWT_VAL(OC_SERVER) == 1)
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }
#endif
    stats_init_and_reg(STATS_HDR(oc_ble_stats),
      STATS_SIZE_INIT_PARMS(oc_ble_stats, STATS_SIZE_32),
      STATS_NAME_INIT_PARMS(oc_ble_stats), "oc_ble");
    return 0;
}

void
oc_ble_reass(struct os_mbuf *om1)
{
    struct os_mbuf_pkthdr *pkt1;
    struct oc_endpoint *oe1;
    struct os_mbuf *om2;
    struct os_mbuf_pkthdr *pkt2;
    struct oc_endpoint *oe2;
    int sr;
    uint8_t hdr[6]; /* sizeof(coap_tcp_hdr32) */

    pkt1 = OS_MBUF_PKTHDR(om1);
    assert(pkt1);
    oe1 = OC_MBUF_ENDPOINT(om1);

    LOG("oc_gatt rx seg %d-%x-%u\n", oe1->bt_addr.conn_handle,
      (unsigned)pkt1, pkt1->omp_len);

    OS_ENTER_CRITICAL(sr);
    STAILQ_FOREACH(pkt2, &oc_ble_reass_q, omp_next) {
        om2 = OS_MBUF_PKTHDR_TO_MBUF(pkt2);
        oe2 = OC_MBUF_ENDPOINT(om2);
        if (oe1->bt_addr.conn_handle == oe2->bt_addr.conn_handle) {
            /*
             * Data from same connection. Append.
             */
            os_mbuf_concat(om2, om1);
            if (os_mbuf_copydata(om2, 0, sizeof(hdr), hdr)) {
                pkt1 = NULL;
                break;
            }
            if (coap_tcp_msg_size(hdr, sizeof(hdr)) <= pkt2->omp_len) {
                STAILQ_REMOVE(&oc_ble_reass_q, pkt2, os_mbuf_pkthdr,
                  omp_next);
                oc_recv_message(om2);
            }
            pkt1 = NULL;
            break;
        }
    }
    if (pkt1) {
        /*
         *
         */
        if (os_mbuf_copydata(om1, 0, sizeof(hdr), hdr) ||
          coap_tcp_msg_size(hdr, sizeof(hdr)) > pkt1->omp_len) {
            STAILQ_INSERT_TAIL(&oc_ble_reass_q, pkt1, omp_next);
        } else {
            oc_recv_message(om1);
        }
    }
    OS_EXIT_CRITICAL(sr);
}

void
oc_ble_coap_conn_new(uint16_t conn_handle)
{
    LOG("oc_gatt newconn %x\n", conn_handle);
}

void
oc_ble_coap_conn_del(uint16_t conn_handle)
{
    struct os_mbuf_pkthdr *pkt;
    struct os_mbuf *m;
    struct oc_endpoint *oe;
    int sr;

    LOG("oc_gatt endconn %x\n", conn_handle);
    OS_ENTER_CRITICAL(sr);
    STAILQ_FOREACH(pkt, &oc_ble_reass_q, omp_next) {
        m = OS_MBUF_PKTHDR_TO_MBUF(pkt);
        oe = OC_MBUF_ENDPOINT(m);
        if (oe->bt_addr.conn_handle == conn_handle) {
            STAILQ_REMOVE(&oc_ble_reass_q, pkt, os_mbuf_pkthdr, omp_next);
            os_mbuf_free_chain(m);
            break;
        }
    }
    OS_EXIT_CRITICAL(sr);
}

static void
oc_event_gatt(struct os_event *ev)
{
    struct os_mbuf *m;

    while ((m = oc_attempt_rx_gatt()) != NULL) {
        oc_ble_reass(m);
    }
}

int
oc_connectivity_init_gatt(void)
{
    os_mqueue_init(&oc_ble_coap_mq, oc_event_gatt, NULL);
    STAILQ_INIT(&oc_ble_reass_q);
    return 0;
}

void
oc_connectivity_shutdown_gatt(void)
{
    /* there is not unregister for BLE */
}

static int
oc_ble_frag(struct os_mbuf *m, uint16_t mtu)
{
    struct os_mbuf_pkthdr *pkt;
    struct os_mbuf *n;
    uint16_t off, blk;

    pkt = OS_MBUF_PKTHDR(m);
    if (pkt->omp_len <= mtu) {
        STAILQ_NEXT(pkt, omp_next) = NULL;
        return 0;
    }
    off = mtu;

    while (off < OS_MBUF_PKTLEN(m)) {
        n = os_msys_get_pkthdr(mtu, 0);
        if (!n) {
            goto err;
        }

        STAILQ_NEXT(pkt, omp_next) = OS_MBUF_PKTHDR(n);
        pkt = STAILQ_NEXT(pkt, omp_next);

        blk = OS_MBUF_PKTLEN(m) - off;
        if (blk > mtu) {
            blk = mtu;
        }
        if (os_mbuf_appendfrom(n, m, off, blk)) {
            goto err;
        }
        off += blk;
    }
    os_mbuf_adj(m, mtu - OS_MBUF_PKTLEN(m));
    return 0;
err:
    pkt = OS_MBUF_PKTHDR(m);
    while (1) {
        pkt = STAILQ_NEXT(pkt, omp_next);
        os_mbuf_free_chain(m);
        if (!pkt) {
            break;
        }
        m = OS_MBUF_PKTHDR_TO_MBUF(pkt);
    };
    return -1;
}

void
oc_send_buffer_gatt(struct os_mbuf *m)
{
    struct oc_endpoint *oe;
    struct os_mbuf_pkthdr *pkt;
    uint16_t mtu;
    uint16_t conn_handle;

    oe = OC_MBUF_ENDPOINT(m);
    conn_handle = oe->bt_addr.conn_handle;

#if (MYNEWT_VAL(OC_CLIENT) == 1)
    ERROR("oc_gatt send not supported on client");
#endif

#if (MYNEWT_VAL(OC_SERVER) == 1)

    STATS_INC(oc_ble_stats, oseg);
    STATS_INCN(oc_ble_stats, obytes, OS_MBUF_PKTLEN(m));

    mtu = ble_att_mtu(oe->bt_addr.conn_handle);
    mtu -= 3; /* # of bytes for ATT notification base */

    if (oc_ble_frag(m, mtu)) {
        STATS_INC(oc_ble_stats, oerr);
        return;
    }
    while (1) {
        pkt = STAILQ_NEXT(OS_MBUF_PKTHDR(m), omp_next);

        ble_gattc_notify_custom(conn_handle, oc_ble_coap_rsp_handle, m);
        if (pkt) {
            m = OS_MBUF_PKTHDR_TO_MBUF(pkt);
        } else {
            break;
        }
    }
#endif
}

void
oc_send_buffer_gatt_mcast(oc_message_t *message)
{
#if (MYNEWT_VAL(OC_CLIENT) == 1)
    ERROR("oc_gatt send not supported on client");
#elif (MYNEWT_VAL(OC_SERVER) == 1)
    oc_message_unref(message);
    ERROR("oc_gatt no multicast support for server only system \n");
#endif
}

#endif
