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
#include <string.h>
#include <stdint.h>

#include "os/mynewt.h"

#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)

#include <stats/stats.h>
#include "oic/oc_gatt.h"
#include "oic/oc_log.h"
#include "oic/oc_ri.h"
#include "oic/messaging/coap/coap.h"
#include "oic/port/oc_connectivity.h"
#include "oic/port/mynewt/ble.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static uint8_t oc_ep_gatt_size(const struct oc_endpoint *oe);
static void oc_send_buffer_gatt(struct os_mbuf *m);
static char *oc_log_ep_gatt(char *ptr, int maxlen, const struct oc_endpoint *);
enum oc_resource_properties
oc_get_trans_security_gatt(const struct oc_endpoint *oe_ble);
static int oc_connectivity_init_gatt(void);
void oc_connectivity_shutdown_gatt(void);

static const struct oc_transport oc_gatt_transport = {
    .ot_flags = OC_TRANSPORT_USE_TCP,
    .ot_ep_size = oc_ep_gatt_size,
    .ot_tx_ucast = oc_send_buffer_gatt,
    .ot_tx_mcast = oc_send_buffer_gatt,
    .ot_get_trans_security = oc_get_trans_security_gatt,
    .ot_ep_str = oc_log_ep_gatt,
    .ot_init = oc_connectivity_init_gatt,
    .ot_shutdown = oc_connectivity_shutdown_gatt
};

static uint8_t oc_gatt_transport_id;

/* OIC Transport Profile GATT */

/* unsecure service UUID */
/* ADE3D529-C784-4F63-A987-EB69F70EE816 */
static const ble_uuid128_t oc_gatt_unsec_svc_uuid =
    BLE_UUID128_INIT(OC_GATT_UNSEC_SVC_UUID);

/* unsecure request characteristic UUID */
/* AD7B334F-4637-4B86-90B6-9D787F03D218 */
static const ble_uuid128_t oc_gatt_unsec_req_chr_uuid =
    BLE_UUID128_INIT(OC_GATT_UNSEC_REQ_CHR_UUID);

/* response characteristic UUID */
/* E9241982-4580-42C4-8831-95048216B256 */
static const ble_uuid128_t oc_gatt_unsec_rsp_chr_uuid =
    BLE_UUID128_INIT(OC_GATT_UNSEC_RSP_CHR_UUID);

/* secure service UUID. */
/* 0xfe18 */
static const ble_uuid16_t oc_gatt_sec_svc_uuid =
    BLE_UUID16_INIT(OC_GATT_SEC_SVC_UUID);

/* secure request characteristic UUID. */
/* 0x1000 */
static const ble_uuid16_t oc_gatt_sec_req_chr_uuid =
    BLE_UUID16_INIT(OC_GATT_SEC_REQ_CHR_UUID);

/* secure response characteristic UUID. */
/* 0x1001 */
static const ble_uuid16_t oc_gatt_sec_rsp_chr_uuid =
    BLE_UUID16_INIT(OC_GATT_SEC_RSP_CHR_UUID);

STATS_SECT_START(oc_ble_stats)
    STATS_SECT_ENTRY(iframe)
    STATS_SECT_ENTRY(iseg)
    STATS_SECT_ENTRY(ibytes)
    STATS_SECT_ENTRY(ierr)
    STATS_SECT_ENTRY(oframe)
    STATS_SECT_ENTRY(oseg)
    STATS_SECT_ENTRY(obytes)
    STATS_SECT_ENTRY(oerr)
STATS_SECT_END
STATS_SECT_DECL(oc_ble_stats) oc_ble_stats;
STATS_NAME_START(oc_ble_stats)
    STATS_NAME(oc_ble_stats, iframe)
    STATS_NAME(oc_ble_stats, iseg)
    STATS_NAME(oc_ble_stats, ibytes)
    STATS_NAME(oc_ble_stats, ierr)
    STATS_NAME(oc_ble_stats, oframe)
    STATS_NAME(oc_ble_stats, oseg)
    STATS_NAME(oc_ble_stats, obytes)
    STATS_NAME(oc_ble_stats, oerr)
STATS_NAME_END(oc_ble_stats)

static STAILQ_HEAD(, os_mbuf_pkthdr) oc_ble_reass_q;

#if (MYNEWT_VAL(OC_SERVER) == 1)
/*
 * BLE nmgr attribute handles for service
 */
#define OC_BLE_SRV_CNT		2
static struct {
    uint16_t req;
    uint16_t rsp;
} oc_ble_srv_handles[OC_BLE_SRV_CNT];

static int oc_gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                   struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def oc_gatt_svr_svcs[] = { {
        /* Service: iotivity */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &oc_gatt_unsec_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* Characteristic: Request */
                .uuid = &oc_gatt_unsec_req_chr_uuid.u,
                .access_cb = oc_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &oc_ble_srv_handles[0].req,
            },{
                /* Characteristic: Response */
                .uuid = &oc_gatt_unsec_rsp_chr_uuid.u,
                .access_cb = oc_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &oc_ble_srv_handles[0].rsp,
            },{
                0, /* No more characteristics in this service */
            }
        },
    },{
        /* Service: CoAP-over-BLE */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &oc_gatt_sec_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* Characteristic: Request */
                .uuid = &oc_gatt_sec_req_chr_uuid.u,
                .access_cb = oc_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &oc_ble_srv_handles[1].req,
            },{
                /* Characteristic: Response */
                .uuid = &oc_gatt_sec_rsp_chr_uuid.u,
                .access_cb = oc_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &oc_ble_srv_handles[1].rsp,
            },{
                0, /* No more characteristics in this service */
            }
        },
    },
    {
        0, /* No more services */
    },
};

/*
 * Look up service index based on characteristic handle from request.
 */
static int
oc_ble_req_attr_to_idx(uint16_t attr_handle)
{
    int i;

    for (i = 0; i < OC_BLE_SRV_CNT; i++) {
        if (oc_ble_srv_handles[i].req == attr_handle) {
            return i;
        }
    }
    return -1;
}

static uint8_t
oc_ep_gatt_size(const struct oc_endpoint *oe)
{
    return sizeof(struct oc_endpoint_ble);
}

static char *
oc_log_ep_gatt(char *ptr, int maxlen, const struct oc_endpoint *oe)
{
    struct oc_endpoint_ble *oe_ble = (struct oc_endpoint_ble *)oe;

    snprintf(ptr, maxlen, "ble %u", oe_ble->conn_handle);
    return ptr;
}

int
oc_ble_reass(struct os_mbuf *om1, uint16_t conn_handle, uint8_t srv_idx)
{
    struct os_mbuf_pkthdr *pkt1;
    struct oc_endpoint_ble *oe_ble;
    struct os_mbuf *om2;
    struct os_mbuf_pkthdr *pkt2;
    uint8_t hdr[6]; /* sizeof(coap_tcp_hdr32) */

    pkt1 = OS_MBUF_PKTHDR(om1);
    assert(pkt1);

    STATS_INC(oc_ble_stats, iseg);
    STATS_INCN(oc_ble_stats, ibytes, pkt1->omp_len);

    OC_LOG_DEBUG("oc_gatt rx seg %u-%x-%u\n", conn_handle,
                 (unsigned)pkt1, pkt1->omp_len);

    STAILQ_FOREACH(pkt2, &oc_ble_reass_q, omp_next) {
        om2 = OS_MBUF_PKTHDR_TO_MBUF(pkt2);
        oe_ble = (struct oc_endpoint_ble *)OC_MBUF_ENDPOINT(om2);
        if (conn_handle == oe_ble->conn_handle && srv_idx == oe_ble->srv_idx) {
            /*
             * Data from same connection. Append.
             */
            os_mbuf_concat(om2, om1);
            os_mbuf_copydata(om2, 0, sizeof(hdr), hdr);

            if (coap_tcp_msg_size(hdr, sizeof(hdr)) <= pkt2->omp_len) {
                STAILQ_REMOVE(&oc_ble_reass_q, pkt2, os_mbuf_pkthdr, omp_next);
                STATS_INC(oc_ble_stats, iframe);
                oc_recv_message(om2);
            }
            pkt1 = NULL;
            break;
        }
    }
    if (pkt1) {
        /*
         * New frame, need to add oc_endpoint_ble in the front.
         * Check if there is enough space available. If not, allocate a
         * new pkthdr.
         */
        if (OS_MBUF_USRHDR_LEN(om1) < sizeof(struct oc_endpoint_ble)) {
            om2 = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint_ble));
            if (!om2) {
                OC_LOG_ERROR("oc_gatt_rx: Could not allocate mbuf\n");
                STATS_INC(oc_ble_stats, ierr);
                return -1;
            }
            OS_MBUF_PKTHDR(om2)->omp_len = pkt1->omp_len;
            SLIST_NEXT(om2, om_next) = om1;
        } else {
            om2 = om1;
        }
        oe_ble = (struct oc_endpoint_ble *)OC_MBUF_ENDPOINT(om2);
        oe_ble->ep.oe_type = oc_gatt_transport_id;
        oe_ble->ep.oe_flags = 0;
        oe_ble->srv_idx = srv_idx;
        oe_ble->conn_handle = conn_handle;
        pkt2 = OS_MBUF_PKTHDR(om2);

        os_mbuf_copydata(om2, 0, sizeof(hdr), hdr);
        if (coap_tcp_msg_size(hdr, sizeof(hdr)) > pkt2->omp_len) {
            STAILQ_INSERT_TAIL(&oc_ble_reass_q, pkt2, omp_next);
        } else {
            STATS_INC(oc_ble_stats, iframe);
            oc_recv_message(om2);
        }
    }
    return 0;
}

static int
oc_gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    struct os_mbuf *m;
    int rc;
    int srv_idx;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        m = ctxt->om;

        srv_idx = oc_ble_req_attr_to_idx(attr_handle);
        assert(srv_idx >= 0);
        rc = oc_ble_reass(m, conn_handle, srv_idx);
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
#endif

int
oc_ble_coap_gatt_srv_init(void)
{
#if (MYNEWT_VAL(OC_SERVER) == 1)
    int rc;

    rc = ble_gatts_count_cfg(oc_gatt_svr_svcs);
    assert(rc == 0);

    rc = ble_gatts_add_svcs(oc_gatt_svr_svcs);
    assert(rc == 0);
#endif
    (void)stats_init_and_reg(STATS_HDR(oc_ble_stats),
      STATS_SIZE_INIT_PARMS(oc_ble_stats, STATS_SIZE_32),
      STATS_NAME_INIT_PARMS(oc_ble_stats), "oc_ble");
    return 0;
}

void
oc_ble_coap_conn_new(uint16_t conn_handle)
{
    OC_LOG_DEBUG("oc_gatt newconn %x\n", conn_handle);
}

void
oc_ble_coap_conn_del(uint16_t conn_handle)
{
    struct os_mbuf_pkthdr *pkt;
    struct os_mbuf *m;
    struct oc_endpoint_ble *oe_ble;

    OC_LOG_DEBUG("oc_gatt endconn %x\n", conn_handle);
    STAILQ_FOREACH(pkt, &oc_ble_reass_q, omp_next) {
        m = OS_MBUF_PKTHDR_TO_MBUF(pkt);
        oe_ble = (struct oc_endpoint_ble *)OC_MBUF_ENDPOINT(m);
        if (oe_ble->conn_handle == conn_handle) {
            STAILQ_REMOVE(&oc_ble_reass_q, pkt, os_mbuf_pkthdr, omp_next);
            os_mbuf_free_chain(m);
            break;
        }
    }
}

int
oc_connectivity_init_gatt(void)
{
    STAILQ_INIT(&oc_ble_reass_q);
    return 0;
}

void
oc_connectivity_shutdown_gatt(void)
{
    /* there is not unregister for BLE */
}

#if (MYNEWT_VAL(OC_SERVER) == 1)
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

    off = pkt->omp_len - (pkt->omp_len % mtu);
    while (off >= mtu) {
        n = os_msys_get_pkthdr(mtu, 0);
        if (!n) {
            goto err;
        }
        STAILQ_NEXT(OS_MBUF_PKTHDR(n), omp_next) = STAILQ_NEXT(pkt, omp_next);
        STAILQ_NEXT(pkt, omp_next) = OS_MBUF_PKTHDR(n);

        blk = pkt->omp_len - off;
        if (os_mbuf_appendfrom(n, m, off, blk)) {
            goto err;
        }
        off -= mtu;
        os_mbuf_adj(m, -blk);
    }
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
#endif

void
oc_send_buffer_gatt(struct os_mbuf *m)
{
#if (MYNEWT_VAL(OC_SERVER) == 1)
    struct oc_endpoint_ble *oe_ble;
    struct os_mbuf_pkthdr *pkt;
    uint16_t mtu;
    uint16_t conn_handle;
    uint16_t attr_handle;
#endif

#if (MYNEWT_VAL(OC_CLIENT) == 1)
    OC_LOG_ERROR("oc_gatt send not supported on client");
#endif

#if (MYNEWT_VAL(OC_SERVER) == 1)

    assert(OS_MBUF_USRHDR_LEN(m) >= sizeof(struct oc_endpoint_ble));
    oe_ble = (struct oc_endpoint_ble *)OC_MBUF_ENDPOINT(m);
    conn_handle = oe_ble->conn_handle;

    STATS_INC(oc_ble_stats, oframe);
    STATS_INCN(oc_ble_stats, obytes, OS_MBUF_PKTLEN(m));

    if (oe_ble->srv_idx >= OC_BLE_SRV_CNT) {
        goto err;
    }
    attr_handle = oc_ble_srv_handles[oe_ble->srv_idx].rsp;

    mtu = ble_att_mtu(conn_handle);
    if (mtu < 4) {
        oc_ble_coap_conn_del(conn_handle);
        goto err;
    }
    mtu -= 3; /* # of bytes for ATT notification base */

    if (oc_ble_frag(m, mtu)) {
        STATS_INC(oc_ble_stats, oerr);
        return;
    }
    while (1) {
        STATS_INC(oc_ble_stats, oseg);
        pkt = STAILQ_NEXT(OS_MBUF_PKTHDR(m), omp_next);

        ble_gattc_notify_custom(conn_handle, attr_handle, m);
        if (pkt) {
            m = OS_MBUF_PKTHDR_TO_MBUF(pkt);
        } else {
            break;
        }
    }
    return;

err:
    os_mbuf_free_chain(m);
    STATS_INC(oc_ble_stats, oerr);
#endif
}

/**
 * Retrieves the specified BLE endpoint's transport layer security properties.
 */
oc_resource_properties_t
oc_get_trans_security_gatt(const struct oc_endpoint *oe)
{
    const struct oc_endpoint_ble *oe_ble;
    oc_resource_properties_t props;
    struct ble_gap_conn_desc desc;
    int rc;

    oe_ble = (const struct oc_endpoint_ble *)oe;
    rc = ble_gap_conn_find(oe_ble->conn_handle, &desc);
    if (rc != 0) {
        return 0;
    }

    props = 0;
    if (desc.sec_state.encrypted) {
        props |= OC_TRANS_ENC;
    }
    if (desc.sec_state.authenticated) {
        props |= OC_TRANS_AUTH;
    }

    return props;
}

#endif

void
oc_register_gatt(void)
{
#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
    oc_gatt_transport_id = oc_transport_register(&oc_gatt_transport);
#endif
}
