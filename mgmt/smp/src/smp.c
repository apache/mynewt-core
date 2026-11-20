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
#include "os/mynewt.h"
#include "mem/mem.h"
#include "mgmt/mgmt.h"
#include "os_mgmt/os_mgmt.h"
#include "mynewt_smp/smp.h"
#include "tinycbor/cbor.h"
#include "tinycbor/cbor_mbuf_writer.h"
#include "tinycbor/cbor_mbuf_reader.h"

/* Shared queue that SMP uses for work items. */
struct os_eventq *g_smp_evq;

void
mgmt_evq_set(struct os_eventq *evq)
{
    g_smp_evq = evq;
}

struct os_eventq *
mgmt_evq_get(void)
{
    return g_smp_evq;
}

static struct os_mbuf *
smp_rsp_frag_alloc(uint16_t frag_size, void *arg)
{
    struct os_mbuf *src_rsp;
    struct os_mbuf *frag;

    /* We need to duplicate the user header from the source response, as that
     * is where transport-specific information is stored.
     */
    src_rsp = arg;

    frag = os_msys_get_pkthdr(frag_size, OS_MBUF_USRHDR_LEN(src_rsp));
    if (frag != NULL) {
        /* Copy the user header from the response into the fragmest mbuf. */
        memcpy(OS_MBUF_USRHDR(frag), OS_MBUF_USRHDR(src_rsp),
               OS_MBUF_USRHDR_LEN(src_rsp));
    }

    return frag;
}

int
smp_tx_rsp(struct smp_streamer *ns, struct os_mbuf *rsp, void *arg)
{
    struct smp_transport *st;
    struct os_mbuf *frag;
    struct os_mbuf *m;
    uint16_t mtu;
    int rc;

    st = arg;
    m  = rsp;

    mtu = st->st_get_mtu(rsp);
    if (mtu == 0U) {
        /* The transport cannot support a transmission right now. */
        return MGMT_ERR_EUNKNOWN;
    }

    while (m != NULL) {
        frag = mem_split_frag(&m, mtu, smp_rsp_frag_alloc, rsp);
        if (frag == NULL) {
            return MGMT_ERR_ENOMEM;
        }

        rc = st->st_output(frag);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
}

/**
 * Processes a single SMP packet and sends the corresponding response(s).
 */
static int
smp_process_packet(struct smp_transport *st)
{
    struct cbor_mbuf_reader reader;
    struct cbor_mbuf_writer writer;
    struct os_mbuf *m;
    int rc;

    if (!st) {
        return MGMT_ERR_EINVAL;
    }

    st->st_streamer = (struct smp_streamer){
        .reader = &reader,
        .writer = &writer,
        .cb_arg = st,
        .tx_rsp_cb = smp_tx_rsp,
    };

    while (1) {
        m = os_mqueue_get(&st->st_imq);
        if (!m) {
            break;
        }

        rc = smp_process_request_packet(&st->st_streamer, m);
        if (rc) {
            return rc;
        }
    }
    
    return 0;
}

int
smp_rx_req(struct smp_transport *st, struct os_mbuf *req)
{
    int rc;
    
    rc = os_mqueue_put(&st->st_imq, os_eventq_dflt_get(), req);
    if (rc) {
        goto err;
    }
     
    return 0;
err:
    os_mbuf_free_chain(req);
    return rc;
}

int
smp_reasm_req_len(struct os_mbuf *om)
{
    struct mgmt_hdr hdr;
    int rc;

    rc = os_mbuf_copydata(om, 0, sizeof(hdr), &hdr);
    if (rc != 0) {
        return 0;
    }

    mgmt_ntoh_hdr(&hdr);

    return MGMT_HDR_SIZE + hdr.nh_len;
}

/**
 * Checks if the buffer starts with a plausible request header.  Data from the
 * middle of a request (e.g., after a lost fragment) is rejected this way with
 * high probability, so that reassembly gets in sync with the next request.
 */
static bool
smp_reasm_hdr_valid(struct os_mbuf *om)
{
    struct mgmt_hdr hdr;
    int rc;

    rc = os_mbuf_copydata(om, 0, sizeof(hdr), &hdr);
    if (rc != 0) {
        return false;
    }

    if (hdr.nh_op != MGMT_OP_READ && hdr.nh_op != MGMT_OP_WRITE) {
        return false;
    }

    /* The two low reserved bits hold the protocol version in SMP version 2,
     * the others must be zero.
     */
    if (hdr._res1 & ~0x03) {
        return false;
    }

    return true;
}

void
smp_reasm_reset(struct smp_reasm *sr)
{
    os_mbuf_free_chain(sr->sr_om);
    sr->sr_om = NULL;
}

int
smp_reasm_rx(struct smp_reasm *sr, struct os_mbuf *frag, uint16_t usrhdr_len,
             struct os_mbuf **out_req)
{
    int req_len;
    int rc;

    *out_req = NULL;

    if (sr->sr_om == NULL) {
        sr->sr_om = os_msys_get_pkthdr(0, usrhdr_len);
        if (sr->sr_om == NULL) {
            return MGMT_ERR_ENOMEM;
        }
    }

    rc = os_mbuf_appendfrom(sr->sr_om, frag, 0, OS_MBUF_PKTLEN(frag));
    if (rc != 0) {
        smp_reasm_reset(sr);
        return MGMT_ERR_ENOMEM;
    }

    req_len = smp_reasm_req_len(sr->sr_om);
    if (req_len == 0) {
        /* Header is not complete yet. */
        return 0;
    }

    if (req_len > MGMT_MAX_MTU) {
        smp_reasm_reset(sr);
        return MGMT_ERR_EMSGSIZE;
    }

    if (!smp_reasm_hdr_valid(sr->sr_om)) {
        smp_reasm_reset(sr);
        return MGMT_ERR_EINVAL;
    }

    if (OS_MBUF_PKTLEN(sr->sr_om) < req_len) {
        return 0;
    }

    *out_req = sr->sr_om;
    sr->sr_om = NULL;

    return 0;
}

static void
smp_event_data_in(struct os_event *ev)
{
    smp_process_packet(ev->ev_arg);
}

int
smp_transport_init(struct smp_transport *st,
                   smp_transport_out_func_t output_func,
                   smp_transport_get_mtu_func_t get_mtu_func)
{
    int rc;

    st->st_output = output_func;
    st->st_get_mtu = get_mtu_func;

    rc = os_mqueue_init(&st->st_imq, smp_event_data_in, st);
    if (rc != 0) {
        goto err;
    }

    return 0;
err:
    return rc;
}

void
smp_pkg_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    mgmt_evq_set(os_eventq_dflt_get());
}
