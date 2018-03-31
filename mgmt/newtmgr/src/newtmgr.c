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

#include "newtmgr/newtmgr.h"
#include "nmgr_os/nmgr_os.h"

#include "tinycbor/cbor.h"
#include "tinycbor/cbor_mbuf_writer.h"
#include "tinycbor/cbor_mbuf_reader.h"

/* Shared queue that newtmgr uses for work items. */
struct os_eventq *nmgr_evq;

/*
 * cbor buffer for newtmgr
 */
static struct nmgr_cbuf {
    struct mgmt_cbuf n_b;
    struct cbor_mbuf_writer writer;
    struct cbor_mbuf_reader reader;
    struct os_mbuf *n_out_m;
} nmgr_task_cbuf;

struct os_eventq *
mgmt_evq_get(void)
{
    return nmgr_evq;
}

void
mgmt_evq_set(struct os_eventq *evq)
{
    nmgr_evq = evq;
}

static int
nmgr_cbuf_init(struct nmgr_cbuf *njb)
{
    memset(njb, 0, sizeof(*njb));
    return (0);
}

static struct nmgr_hdr *
nmgr_init_rsp(struct os_mbuf *m, struct nmgr_hdr *src)
{
    struct nmgr_hdr *hdr;

    hdr = (struct nmgr_hdr *) os_mbuf_extend(m, sizeof(struct nmgr_hdr));
    if (!hdr) {
        return NULL;
    }
    memcpy(hdr, src, sizeof(*hdr));
    hdr->nh_len = 0;
    hdr->nh_flags = 0;
    hdr->nh_op = (src->nh_op == NMGR_OP_READ) ? NMGR_OP_READ_RSP :
      NMGR_OP_WRITE_RSP;
    hdr->nh_group = src->nh_group;
    hdr->nh_seq = src->nh_seq;
    hdr->nh_id = src->nh_id;

    /* setup state for cbor encoding */
    cbor_mbuf_writer_init(&nmgr_task_cbuf.writer, m);
    cbor_encoder_init(&nmgr_task_cbuf.n_b.encoder, &nmgr_task_cbuf.writer.enc, 0);
    nmgr_task_cbuf.n_out_m = m;
    return hdr;
}

static void
nmgr_send_err_rsp(struct nmgr_transport *nt, struct os_mbuf *m,
                  struct nmgr_hdr *hdr, int status)
{
    struct CborEncoder map;
    int rc;

    hdr = nmgr_init_rsp(m, hdr);
    if (!hdr) {
        os_mbuf_free_chain(m);
        return;
    }

    rc = cbor_encoder_create_map(&nmgr_task_cbuf.n_b.encoder, &map,
                                 CborIndefiniteLength);
    if (rc != 0) {
        return;
    }

    rc = mgmt_cbuf_setoerr(&nmgr_task_cbuf.n_b, status);
    if (rc != 0) {
        return;
    }

    rc = cbor_encoder_close_container(&nmgr_task_cbuf.n_b.encoder, &map);
    if (rc != 0) {
        return;
    }

    hdr->nh_len =
        htons(cbor_encode_bytes_written(&nmgr_task_cbuf.n_b.encoder));

    nt->nt_output(nt, nmgr_task_cbuf.n_out_m);
}

/**
 * Allocates an mbuf to contain an outgoing response fragment.
 */
static struct os_mbuf *
nmgr_rsp_frag_alloc(uint16_t frag_size, void *arg)
{
    struct os_mbuf *src_rsp;
    struct os_mbuf *frag;

    /* We need to duplicate the user header from the source response, as that
     * is where transport-specific information is stored.
     */
    src_rsp = arg;

    frag = os_msys_get_pkthdr(frag_size, OS_MBUF_USRHDR_LEN(src_rsp));
    if (frag != NULL) {
        /* Copy the user header from the response into the fragment mbuf. */
        memcpy(OS_MBUF_USRHDR(frag), OS_MBUF_USRHDR(src_rsp),
               OS_MBUF_USRHDR_LEN(src_rsp));
    }

    return frag;
}

/**
 * Sends a newtmgr response, fragmenting it as needed.  The supplied response
 * mbuf is consumed on success and in some failure cases.  If the mbuf is
 * consumed, the supplied pointer is set to NULL.
 *
 * This function prefers not to consume the supplied mbuf on failure.  The
 * reason for this is to allow the caller to reuse the mbuf for an error
 * response.
 */
static int
nmgr_rsp_tx(struct nmgr_transport *nt, struct os_mbuf **rsp, uint16_t mtu)
{
    struct os_mbuf *frag;
    int rc;

    while (*rsp != NULL) {
        frag = mem_split_frag(rsp, mtu, nmgr_rsp_frag_alloc, *rsp);
        if (frag == NULL) {
            return MGMT_ERR_ENOMEM;
        }

        rc = nt->nt_output(nt, frag);
        if (rc != 0) {
            /* Output function already freed mbuf. */
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return MGMT_ERR_EOK;
}

static void
nmgr_handle_req(struct nmgr_transport *nt, struct os_mbuf *req)
{
    struct os_mbuf *rsp;
    const struct mgmt_handler *handler;
    CborEncoder payload_enc;
    struct nmgr_hdr *rsp_hdr;
    struct nmgr_hdr hdr;
    int off;
    uint16_t len;
    uint16_t mtu;
    int rc;

    rsp_hdr = NULL;

    rsp = os_msys_get_pkthdr(512, OS_MBUF_USRHDR_LEN(req));
    if (!rsp) {
        rc = os_mbuf_copydata(req, 0, sizeof(hdr), &hdr);
        if (rc < 0) {
            goto err_norsp;
        }
        rsp = req;
        req = NULL;
        goto err;
    }

    mtu = nt->nt_get_mtu(req);
    if (mtu == 0) {
        /* The transport cannot support a transmission right now. */
        goto err_norsp;
    }

    /* Copy the request user header into the response. */
    memcpy(OS_MBUF_USRHDR(rsp), OS_MBUF_USRHDR(req), OS_MBUF_USRHDR_LEN(req));

    off = 0;
    len = OS_MBUF_PKTHDR(req)->omp_len;

    while (off < len) {
        rc = os_mbuf_copydata(req, off, sizeof(hdr), &hdr);
        if (rc < 0) {
            rc = MGMT_ERR_EINVAL;
            goto err_norsp;
        }

        hdr.nh_len = ntohs(hdr.nh_len);

        handler = mgmt_find_handler(ntohs(hdr.nh_group), hdr.nh_id);
        if (!handler) {
            rc = MGMT_ERR_ENOENT;
            goto err;
        }

        /* Build response header apriori.  Then pass to the handlers
         * to fill out the response data, and adjust length & flags.
         */
        rsp_hdr = nmgr_init_rsp(rsp, &hdr);
        if (!rsp_hdr) {
            rc = MGMT_ERR_ENOMEM;
            goto err_norsp;
        }

        cbor_mbuf_reader_init(&nmgr_task_cbuf.reader, req, sizeof(hdr));
        cbor_parser_init(&nmgr_task_cbuf.reader.r, 0,
                         &nmgr_task_cbuf.n_b.parser, &nmgr_task_cbuf.n_b.it);

        /* Begin response payload.  Response fields are inserted into the root
         * map as key value pairs.
         */
        rc = cbor_encoder_create_map(&nmgr_task_cbuf.n_b.encoder, &payload_enc,
                                     CborIndefiniteLength);
        if (rc != 0) {
            rc = MGMT_ERR_ENOMEM;
            goto err;
        }

        if (hdr.nh_op == NMGR_OP_READ) {
            if (handler->mh_read) {
                rc = handler->mh_read(&nmgr_task_cbuf.n_b);
            } else {
                rc = MGMT_ERR_ENOENT;
            }
        } else if (hdr.nh_op == NMGR_OP_WRITE) {
            if (handler->mh_write) {
                rc = handler->mh_write(&nmgr_task_cbuf.n_b);
            } else {
                rc = MGMT_ERR_ENOENT;
            }
        } else {
            rc = MGMT_ERR_EINVAL;
        }
        if (rc != 0) {
            goto err;
        }

        /* End response payload. */
        rc = cbor_encoder_close_container(&nmgr_task_cbuf.n_b.encoder,
                                          &payload_enc);
        if (rc != 0) {
            rc = MGMT_ERR_ENOMEM;
            goto err;
        }

        rsp_hdr->nh_len +=
            cbor_encode_bytes_written(&nmgr_task_cbuf.n_b.encoder);
        rsp_hdr->nh_len = htons(rsp_hdr->nh_len);

        rc = nmgr_rsp_tx(nt, &rsp, mtu);
        if (rc) {
            /* If the entire mbuf was consumed by the transport, don't attempt
             * to send an error response.
             */
            if (rsp == NULL) {
                goto err_norsp;
            } else {
                goto err;
            }
        }

        off += sizeof(hdr) + OS_ALIGN(hdr.nh_len, 4);
    }

    os_mbuf_free_chain(rsp);
    os_mbuf_free_chain(req);
    return;

err:
    /* Clear partially written response. */
    os_mbuf_adj(rsp, OS_MBUF_PKTLEN(rsp));

    nmgr_send_err_rsp(nt, rsp, &hdr, rc);
    os_mbuf_free_chain(req);
    return;

err_norsp:
    os_mbuf_free_chain(rsp);
    os_mbuf_free_chain(req);
    return;
}


static void
nmgr_process(struct nmgr_transport *nt)
{
    struct os_mbuf *m;

    while (1) {
        m = os_mqueue_get(&nt->nt_imq);
        if (!m) {
            break;
        }

        nmgr_handle_req(nt, m);
    }
}

static void
nmgr_event_data_in(struct os_event *ev)
{
    nmgr_process(ev->ev_arg);
}

int
nmgr_transport_init(struct nmgr_transport *nt,
        nmgr_transport_out_func_t output_func,
        nmgr_transport_get_mtu_func_t get_mtu_func)
{
    int rc;

    nt->nt_output = output_func;
    nt->nt_get_mtu = get_mtu_func;

    rc = os_mqueue_init(&nt->nt_imq, nmgr_event_data_in, nt);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Transfers an incoming request to the newtmgr task.  The caller relinquishes
 * ownership of the supplied mbuf upon calling this function, whether this
 * function succeeds or fails.
 *
 * @param nt                    The transport that the request was received
 *                                  over.
 * @param req                   An mbuf containing the newtmgr request.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
nmgr_rx_req(struct nmgr_transport *nt, struct os_mbuf *req)
{
    int rc;

    rc = os_mqueue_put(&nt->nt_imq, mgmt_evq_get(), req);
    if (rc != 0) {
        os_mbuf_free_chain(req);
    }

    return rc;
}

void
nmgr_pkg_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = nmgr_os_groups_register();
    SYSINIT_PANIC_ASSERT(rc == 0);

    nmgr_cbuf_init(&nmgr_task_cbuf);

    mgmt_evq_set(os_eventq_dflt_get());
}
