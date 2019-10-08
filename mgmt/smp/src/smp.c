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

static mgmt_alloc_rsp_fn smp_alloc_rsp;
static mgmt_trim_front_fn smp_trim_front;
static mgmt_reset_buf_fn smp_reset_buf;
static mgmt_write_at_fn smp_write_at;
static mgmt_init_reader_fn smp_init_reader;
static mgmt_init_writer_fn smp_init_writer;
static mgmt_free_buf_fn smp_free_buf;

const struct mgmt_streamer_cfg g_smp_cbor_cfg = {
    .alloc_rsp = smp_alloc_rsp,
    .trim_front = smp_trim_front,
    .reset_buf = smp_reset_buf,
    .write_at = smp_write_at,
    .init_reader = smp_init_reader,
    .init_writer = smp_init_writer,
    .free_buf = smp_free_buf,
};

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

static void *
smp_alloc_rsp(const void *req, void *arg)
{
   struct os_mbuf *m;
   struct os_mbuf *rsp;
 
   if (!req) {
       return NULL;
   }

   m = (struct os_mbuf *)req;

   rsp = os_msys_get_pkthdr(0, OS_MBUF_USRHDR_LEN(m));
   if (!rsp) {
       return NULL;
   }

   memcpy(OS_MBUF_USRHDR(rsp), OS_MBUF_USRHDR(m), OS_MBUF_USRHDR_LEN(m));

   return rsp;
}

static void
smp_trim_front(void *m, size_t len, void *arg)
{
    os_mbuf_adj(m, len);
}

static void
smp_reset_buf(void *m, void *arg)
{
    if (!m) {
        return;
    }

    /* We need to trim from the back because the head
     * contains useful information which we do not want
     * to get rid of
     */
    os_mbuf_adj(m, -1 * OS_MBUF_PKTLEN((struct os_mbuf *)m));
}

static int
smp_write_at(struct cbor_encoder_writer *writer, size_t offset,
              const void *data, size_t len, void *arg)
{
    struct cbor_mbuf_writer *cmw;
    struct os_mbuf *m;
    int rc;

    if (!writer) {
        return MGMT_ERR_EINVAL;
    }

    cmw = (struct cbor_mbuf_writer *)writer;
    m = cmw->m;

    if (offset > OS_MBUF_PKTLEN(m)) {
        return MGMT_ERR_EINVAL;
    }
    
    rc = os_mbuf_copyinto(m, offset, data, len);
    if (rc) {
        return MGMT_ERR_ENOMEM;
    }

    writer->bytes_written = OS_MBUF_PKTLEN(m);

    return 0;
}

static void
smp_free_buf(void *m, void *arg)
{
    if (!m) {
        return;
    }

    os_mbuf_free_chain(m);
}

static int
smp_init_reader(struct cbor_decoder_reader *reader, void *m,
		void *arg)
{
    struct cbor_mbuf_reader *cmr;
    
    if (!reader) {
        return MGMT_ERR_EINVAL;
    }

    cmr = (struct cbor_mbuf_reader *)reader;
    cbor_mbuf_reader_init(cmr, m, 0);

    return 0;
}

static int
smp_init_writer(struct cbor_encoder_writer *writer, void *m,
		void *arg)
{
    struct cbor_mbuf_writer *cmw;
     
    if (!writer) {
        return MGMT_ERR_EINVAL;
    }

    cmw = (struct cbor_mbuf_writer *)writer;
    cbor_mbuf_writer_init(cmw, m);

    return 0;
}

/**
 * Allocates an mbuf to costain an outgoing response fragment.
 */
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
smp_tx_rsp(struct smp_streamer *ns, void *rsp, void *arg)
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

    st->st_streamer = (struct smp_streamer) {
        .mgmt_stmr = {
            .cfg = &g_smp_cbor_cfg,
            .reader = &reader.r,
            .writer = &writer.enc,
            .cb_arg = st,
        },
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
