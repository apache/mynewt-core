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
#if MYNEWT_VAL(OC_TRANSPORT_LORA)
#include <assert.h>
#include <string.h>

#include <os/os.h>
#include <os/endian.h>

#include <log/log.h>
#include <stats/stats.h>
#include <crc/crc16.h>

#include <node/lora.h>

#include "port/oc_connectivity.h"
#include "oic/oc_log.h"
#include "api/oc_buffer.h"
#include "adaptor.h"

#ifdef OC_SECURITY
#error This implementation does not yet support security
#endif

STATS_SECT_START(oc_lora_stats)
    STATS_SECT_ENTRY(iframe)
    STATS_SECT_ENTRY(ibytes)
    STATS_SECT_ENTRY(ierr)
    STATS_SECT_ENTRY(icsum)
    STATS_SECT_ENTRY(ishort)
    STATS_SECT_ENTRY(ioof)
    STATS_SECT_ENTRY(oframe)
    STATS_SECT_ENTRY(obytes)
    STATS_SECT_ENTRY(oerr)
    STATS_SECT_ENTRY(oom)
STATS_SECT_END
static STATS_SECT_DECL(oc_lora_stats) oc_lora_stats;
STATS_NAME_START(oc_lora_stats)
    STATS_NAME(oc_lora_stats, iframe)
    STATS_NAME(oc_lora_stats, ibytes)
    STATS_NAME(oc_lora_stats, ierr)
    STATS_NAME(oc_lora_stats, icsum)
    STATS_NAME(oc_lora_stats, ishort)
    STATS_NAME(oc_lora_stats, ioof)
    STATS_NAME(oc_lora_stats, oframe)
    STATS_NAME(oc_lora_stats, obytes)
    STATS_NAME(oc_lora_stats, oerr)
    STATS_NAME(oc_lora_stats, oom)
STATS_NAME_END(oc_lora_stats)
static uint8_t oc_lora_stat_reg;

static struct oc_lora_state {
    STAILQ_HEAD(, os_mbuf_pkthdr) reass_q;
    STAILQ_HEAD(, os_mbuf_pkthdr) tx_q;
    int tx_frag_num;
} oc_lora_state = {
    .reass_q = STAILQ_HEAD_INITIALIZER(oc_lora_state.reass_q),
    .tx_q = STAILQ_HEAD_INITIALIZER(oc_lora_state.tx_q)
};

/*
 * Fragmentation/reassembly.
 */
#define COAP_LORA_LAST_FRAG	0x80	/* Indicates last frag in a frame. */
#define COAP_LORA_FRAG_NUM(a)   ((a) & ~COAP_LORA_LAST_FRAG)

struct coap_lora_frag_start {
    uint8_t frag_num;	/* 0 */
    uint16_t crc;	/* crc over reassembled packet */
} __attribute__((packed));

struct coap_lora_frag {
    uint8_t frag_num;	/* which fragment within packet */
};

void
oc_send_frag_lora(struct oc_lora_state *os)
{
    union {
        struct coap_lora_frag_start s;
        struct coap_lora_frag f;
    } hdr;
    struct oc_endpoint_lora *oe;
    struct os_mbuf_pkthdr *pkt;
    struct os_mbuf *m;
    struct os_mbuf *n;
    struct os_mbuf *o;
    int mtu;
    int blk_len;
    uint16_t crc;

    pkt = STAILQ_FIRST(&os->tx_q);
    m = OS_MBUF_PKTHDR_TO_MBUF(pkt);
    oe = (struct oc_endpoint_lora *)OC_MBUF_ENDPOINT(m);

    mtu = lora_app_mtu();

    n = os_msys_get_pkthdr(mtu, sizeof(struct lora_pkt_info));
    if (!n) {
        goto oom;
    }
    if (os->tx_frag_num == 0) {
        crc = CRC16_INITIAL_CRC;
        for (o = m; o; o = SLIST_NEXT(o, om_next)) {
            crc = crc16_ccitt(crc, o->om_data, o->om_len);
        }
        hdr.s.frag_num = 0;
        hdr.s.crc = crc;
        os->tx_frag_num = 1;
        blk_len = mtu - sizeof(hdr.s);
        if (blk_len >= pkt->omp_len) {
            blk_len = pkt->omp_len;
            hdr.s.frag_num |= COAP_LORA_LAST_FRAG;
        }
        if (os_mbuf_copyinto(n, 0, &hdr.s, sizeof(hdr.s))) {
            goto oom;
        }
    } else {
        hdr.f.frag_num = os->tx_frag_num++;
        blk_len = mtu - sizeof(hdr.f);
        if (blk_len >= pkt->omp_len) {
            blk_len = pkt->omp_len;
            hdr.f.frag_num |= COAP_LORA_LAST_FRAG;
        }
        if (os_mbuf_copyinto(n, 0, &hdr.f, sizeof(hdr.f))) {
            goto oom;
        }
    }
    if (os_mbuf_appendfrom(n, m, 0, blk_len)) {
        goto oom;
    }
    os_mbuf_adj(m, blk_len);
    STATS_INC(oc_lora_stats, oframe);
    STATS_INCN(oc_lora_stats, obytes, OS_MBUF_PKTLEN(n));
    if (lora_app_port_send(oe->port, MCPS_CONFIRMED, n)) {
        STATS_INC(oc_lora_stats, oerr);
        goto err;
    }
    return;
oom:
    STATS_INC(oc_lora_stats, oom);
err:
    os_mbuf_free_chain(n);
    os_mbuf_free_chain(m);
    STAILQ_REMOVE_HEAD(&os->tx_q, omp_next);
    /* XXX unlikely that there's something else queued, but if there is,
     * we should start tx again */
}

static void
oc_lora_tx_cb(uint8_t port, LoRaMacEventInfoStatus_t status, Mcps_t pkt_type,
              struct os_mbuf *m)
{
    struct oc_lora_state *os = &oc_lora_state;
    struct os_mbuf_pkthdr *pkt;

    os_mbuf_free_chain(m);
    if (status != LORAMAC_EVENT_INFO_STATUS_OK) {
        STATS_INC(oc_lora_stats, oerr);
    }

    pkt = STAILQ_FIRST(&os->tx_q);
    assert(pkt);
    if (pkt->omp_len == 0) {
        STAILQ_REMOVE_HEAD(&os->tx_q, omp_next);
        os->tx_frag_num = 0;
        os_mbuf_free_chain(OS_MBUF_PKTHDR_TO_MBUF(pkt));
    }
    if (!STAILQ_EMPTY(&os->tx_q)) {
        oc_send_frag_lora(os);
    }
}

void
oc_send_buffer_lora(struct os_mbuf *m)
{
    struct oc_lora_state *os = &oc_lora_state;
    int in_progress = 0;

    if (!STAILQ_EMPTY(&os->tx_q)) {
        in_progress = 1;
    }
    STAILQ_INSERT_TAIL(&os->tx_q, OS_MBUF_PKTHDR(m), omp_next);
    if (!in_progress) {
        oc_send_frag_lora(os);
    }
}

static struct os_mbuf *
oc_lora_rx_compact(struct os_mbuf *m)
{
    struct os_mbuf *n;

    n = SLIST_NEXT(m, om_next);
    if (OS_MBUF_TRAILINGSPACE(m) >= n->om_len) {
        memcpy(m->om_data + m->om_len, n->om_data, n->om_len);
        m->om_len += n->om_len;
        SLIST_NEXT(m, om_next) = SLIST_NEXT(n, om_next);
        os_mbuf_free(n);
        return m;
    } else {
        return n;
    }
}

static void
oc_lora_deliver(struct os_mbuf_pkthdr *mp, uint16_t port)
{
    struct oc_endpoint_lora *oe;
    struct os_mbuf *m;
    struct os_mbuf *n;
    struct os_mbuf *o;
    struct os_mbuf_pkthdr *np;
    uint16_t crc;
    uint16_t pktcrc;

    m = OS_MBUF_PKTHDR_TO_MBUF(mp);

    os_mbuf_copydata(m, (int)(&((struct coap_lora_frag_start *)0)->crc),
                     sizeof(crc), &pktcrc);

    /*
     * Trim frag header out.
     */
    os_mbuf_adj(m, sizeof(struct coap_lora_frag_start));

    /*
     * add oc_endpoint_lora in the front.
     */
    if (OS_MBUF_USRHDR_LEN(m) < sizeof(struct oc_endpoint_lora)) {
        n = os_msys_get_pkthdr(0, sizeof(struct oc_endpoint_lora));
        if (!n) {
            OC_LOG_ERROR("oc_lora_rx_cb: Could not allocate mbuf\n");
            STATS_INC(oc_lora_stats, ierr);
            os_mbuf_free_chain(m);
            return;
        }
        OS_MBUF_PKTHDR(n)->omp_len = OS_MBUF_PKTLEN(m);
        SLIST_NEXT(n, om_next) = m;
        m = n;
    }
    oe = (struct oc_endpoint_lora *)OC_MBUF_ENDPOINT(m);
    oe->flags = LORA;
    oe->port = port;

    /*
     * CRC first fragment.
     */
    crc = crc16_ccitt(CRC16_INITIAL_CRC, m->om_data, m->om_len);
    n = m;
    while ((o = SLIST_NEXT(n, om_next))) {
        crc = crc16_ccitt(crc, o->om_data, o->om_len);
        n = oc_lora_rx_compact(n);
    }

    while ((np = STAILQ_NEXT(mp, omp_next))) {
        /*
         * XXX coalesce short fragments
         */
        STAILQ_NEXT(mp, omp_next) = STAILQ_NEXT(np, omp_next);
        os_mbuf_adj(OS_MBUF_PKTHDR_TO_MBUF(np), sizeof(struct coap_lora_frag));
        SLIST_NEXT(n, om_next) = OS_MBUF_PKTHDR_TO_MBUF(np);
        mp->omp_len += np->omp_len;
        while ((o = SLIST_NEXT(n, om_next))) {
            crc = crc16_ccitt(crc, o->om_data, o->om_len);
            n = oc_lora_rx_compact(n);
        }
    }

    if (crc != pktcrc) {
        STATS_INC(oc_lora_stats, icsum);
        os_mbuf_free_chain(m);
    } else {
        oc_recv_message(m);
    }
}

static uint8_t
oc_lora_frag_num(struct os_mbuf *m)
{
    struct coap_lora_frag *cf;

    cf = (struct coap_lora_frag *)m->om_data;
    return COAP_LORA_FRAG_NUM(cf->frag_num);
}

static void
oc_lora_rx_reass(struct os_mbuf *m, uint16_t port)
{
    struct oc_lora_state *os = &oc_lora_state;
    struct coap_lora_frag_start *cfs;
    struct coap_lora_frag cf;
    uint8_t frag_num;
    struct os_mbuf_pkthdr *pkt;
    struct os_mbuf_pkthdr *pkt2;

    if (os_mbuf_copydata(m, 0, sizeof(cf), &cf)) {
        STATS_INC(oc_lora_stats, ishort);
        goto err;
    }
    frag_num = COAP_LORA_FRAG_NUM(cf.frag_num);
    if (frag_num == 0) {
        m = os_mbuf_pullup(m, sizeof(*cfs));
    } else {
        m = os_mbuf_pullup(m, sizeof(cf));
    }
    if (!m) {
        STATS_INC(oc_lora_stats, ishort);
        goto err;
    }

    pkt = OS_MBUF_PKTHDR(m);

    if (STAILQ_EMPTY(&os->reass_q)) {
        if (frag_num != 0) {
            STATS_INC(oc_lora_stats, ioof);
            goto err;
        }
        if (cf.frag_num & COAP_LORA_LAST_FRAG) {
            oc_lora_deliver(pkt, port);
        } else {
            STAILQ_INSERT_TAIL(&os->reass_q, pkt, omp_next);
        }
    } else {
        pkt2 = STAILQ_LAST(&os->reass_q, os_mbuf_pkthdr, omp_next);
        if (oc_lora_frag_num(OS_MBUF_PKTHDR_TO_MBUF(pkt2)) + 1 != frag_num) {
            while ((pkt2 = STAILQ_FIRST(&os->reass_q))) {
                STAILQ_REMOVE_HEAD(&os->reass_q, omp_next);
                STATS_INC(oc_lora_stats, ioof);
                os_mbuf_free_chain(OS_MBUF_PKTHDR_TO_MBUF(pkt2));
            }
            STAILQ_INSERT_TAIL(&os->reass_q, pkt, omp_next);
        } else {
            STAILQ_INSERT_TAIL(&os->reass_q, pkt, omp_next);
            pkt2 = STAILQ_FIRST(&os->reass_q);
            if (cf.frag_num & COAP_LORA_LAST_FRAG) {
                STAILQ_INIT(&os->reass_q);
                oc_lora_deliver(pkt2, port);
            }
        }
    }
    return;
err:
    os_mbuf_free_chain(m);
}

static void
oc_lora_rx_cb(uint8_t port, LoRaMacEventInfoStatus_t status, Mcps_t pkt_type,
              struct os_mbuf *m)
{
    assert(port == MYNEWT_VAL(OC_LORA_PORT));
    STATS_INC(oc_lora_stats, iframe);
    if (status != LORAMAC_EVENT_INFO_STATUS_OK) {
        STATS_INC(oc_lora_stats, ierr);
        os_mbuf_free_chain(m);
    } else {
        STATS_INCN(oc_lora_stats, ibytes, OS_MBUF_PKTLEN(m));
        oc_lora_rx_reass(m, port);
    }
}

void
oc_connectivity_shutdown_lora(void)
{
    lora_app_port_close(MYNEWT_VAL(OC_LORA_PORT));
}

int
oc_connectivity_init_lora(void)
{
    int rc;

    if (!oc_lora_stat_reg) {
        (void)stats_init_and_reg(STATS_HDR(oc_lora_stats),
          STATS_SIZE_INIT_PARMS(oc_lora_stats, STATS_SIZE_32),
          STATS_NAME_INIT_PARMS(oc_lora_stats), "oc_lora");
        oc_lora_stat_reg = 1;
    }

    rc = lora_app_port_open(MYNEWT_VAL(OC_LORA_PORT), oc_lora_tx_cb,
                            oc_lora_rx_cb);
    if (rc) {
        return rc;
    }
    return 0;
}

#endif
