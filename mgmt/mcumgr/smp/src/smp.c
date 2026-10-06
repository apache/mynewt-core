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

/** SMP - Simple Management Protocol. */

#include <assert.h>
#include <string.h>

#include "tinycbor/cbor.h"
#include "mgmt/endian.h"
#include "mgmt/mgmt.h"
#include "smp/smp.h"

static int
smp_align4(int x)
{
    int rem;

    rem = x % 4;
    if (rem == 0) {
        return x;
    } else {
        return x - rem + 4;
    }
}

/**
 * Converts a request opcode to its corresponding response opcode.
 */
static uint8_t
smp_rsp_op(uint8_t req_op)
{
    if (req_op == MGMT_OP_READ) {
        return MGMT_OP_READ_RSP;
    } else {
        return MGMT_OP_WRITE_RSP;
    }
}

static void
smp_init_rsp_hdr(const struct mgmt_hdr *req_hdr, struct mgmt_hdr *rsp_hdr)
{
    *rsp_hdr = (struct mgmt_hdr) {
        .nh_len = 0,
        .nh_flags = 0,
        .nh_op = smp_rsp_op(req_hdr->nh_op),
        .nh_group = req_hdr->nh_group,
        .nh_seq = req_hdr->nh_seq,
        .nh_id = req_hdr->nh_id,
    };
}

static struct os_mbuf *
smp_alloc_rsp(struct os_mbuf *req)
{
    struct os_mbuf *rsp;

    if (!req) {
        return NULL;
    }

    rsp = os_msys_get_pkthdr(0, OS_MBUF_USRHDR_LEN(req));
    if (!rsp) {
        return NULL;
    }

    memcpy(OS_MBUF_USRHDR(rsp), OS_MBUF_USRHDR(req), OS_MBUF_USRHDR_LEN(req));

    return rsp;
}

static void
smp_reset_buf(struct os_mbuf *m)
{
    if (!m) {
        return;
    }

    /* We need to trim from the back because the head
     * contains useful information which we do not want
     * to get rid of
     */
    os_mbuf_adj(m, -1 * OS_MBUF_PKTLEN(m));
}

static void
smp_free_buf(struct os_mbuf *m)
{
    if (!m) {
        return;
    }

    os_mbuf_free_chain(m);
}

static int
smp_write_at(struct cbor_mbuf_writer *cmw, size_t offset, const void *data,
             size_t len)
{
    struct os_mbuf *m;
    int rc;

    m = cmw->m;

    if (offset > OS_MBUF_PKTLEN(m)) {
        return MGMT_ERR_EINVAL;
    }

    rc = os_mbuf_copyinto(m, offset, data, len);
    if (rc) {
        return MGMT_ERR_ENOMEM;
    }

    cmw->enc.bytes_written = OS_MBUF_PKTLEN(m);

    return 0;
}

static int
smp_read_hdr(struct smp_streamer *streamer, struct mgmt_hdr *dst_hdr)
{
    struct cbor_decoder_reader *reader;

    reader = &streamer->reader->r;

    if (reader->message_size < sizeof *dst_hdr) {
        return MGMT_ERR_EINVAL;
    }

    reader->cpy(reader, (char *)dst_hdr, 0, sizeof *dst_hdr);
    return 0;
}

static int
smp_write_hdr(struct smp_streamer *streamer, const struct mgmt_hdr *src_hdr)
{
    int rc;

    rc = smp_write_at(streamer->writer, 0, src_hdr, sizeof *src_hdr);
    return mgmt_err_from_cbor(rc);
}

static int
smp_build_err_rsp(struct smp_streamer *streamer,
                  const struct mgmt_hdr *req_hdr,
                  int status)
{
    struct CborEncoder map;
    struct mgmt_ctxt cbuf;
    struct mgmt_hdr rsp_hdr;
    int rc;

    rc = mgmt_ctxt_init(&cbuf, &streamer->reader->r, &streamer->writer->enc);
    if (rc != 0) {
        return rc;
    }

    smp_init_rsp_hdr(req_hdr, &rsp_hdr);
    rc = smp_write_hdr(streamer, &rsp_hdr);
    if (rc != 0) {
        return rc;
    }

    rc = cbor_encoder_create_map(&cbuf.encoder, &map, CborIndefiniteLength);
    if (rc != 0) {
        return rc;
    }

    rc = mgmt_write_rsp_status(&cbuf, status);
    if (rc != 0) {
        return rc;
    }

    rc = cbor_encoder_close_container(&cbuf.encoder, &map);
    if (rc != 0) {
        return rc;
    }

    rsp_hdr.nh_len = cbor_encode_bytes_written(&cbuf.encoder) - MGMT_HDR_SIZE;
    mgmt_hton_hdr(&rsp_hdr);
    rc = smp_write_hdr(streamer, &rsp_hdr);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Processes a single SMP request and generates a response payload (i.e.,
 * everything after the management header).  On success, the response payload
 * is written to the supplied cbuf but not transmitted.  On failure, no error
 * response gets written; the caller is expected to build an error response
 * from the return code.
 *
 * @param cbuf                  A cbuf containing the request and response
 *                                  buffer.
 * @param req_hdr               The management header belonging to the incoming
 *                                  request (host-byte order).
 *
 * @return                      A MGMT_ERR_[...] error code.
 */
static int
smp_handle_single_payload(struct mgmt_ctxt *cbuf,
                          const struct mgmt_hdr *req_hdr, bool *handler_found)
{
    const struct mgmt_handler *handler;
    mgmt_handler_fn *handler_fn;
    struct CborEncoder payload_encoder;
    int rc;

    handler = mgmt_find_handler(req_hdr->nh_group, req_hdr->nh_id);
    if (handler == NULL) {
        return MGMT_ERR_ENOTSUP;
    }

    /* Begin response payload.  Response fields are inserted into the root
     * map as key value pairs.
     */
    rc = cbor_encoder_create_map(&cbuf->encoder, &payload_encoder,
                                 CborIndefiniteLength);
    rc = mgmt_err_from_cbor(rc);
    if (rc != 0) {
        return rc;
    }

    switch (req_hdr->nh_op) {
    case MGMT_OP_READ:
        handler_fn = handler->mh_read;
        break;

    case MGMT_OP_WRITE:
        handler_fn = handler->mh_write;
        break;

    default:
        return MGMT_ERR_EINVAL;
    }

    if (handler_fn) {
        *handler_found = true;
        mgmt_evt(MGMT_EVT_OP_CMD_RECV, req_hdr->nh_group, req_hdr->nh_id, NULL);

        rc = handler_fn(cbuf);
    } else {
        rc = MGMT_ERR_ENOTSUP;
    }

    if (rc != 0) {
        return rc;
    }

    /* End response payload. */
    rc = cbor_encoder_close_container(&cbuf->encoder, &payload_encoder);
    return mgmt_err_from_cbor(rc);
}

/**
 * Processes a single SMP request and generates a complete response (i.e.,
 * header and payload).  On success, the response is written using the supplied
 * streamer but not transmitted.  On failure, no error response gets written;
 * the caller is expected to build an error response from the return code.
 *
 * @param streamer              The SMP streamer to use for reading the request
 *                                  and writing the response.
 * @param req_hdr               The management header belonging to the incoming
 *                                  request (host-byte order).
 *
 * @return                      A MGMT_ERR_[...] error code.
 */
static int
smp_handle_single_req(struct smp_streamer *streamer,
                      const struct mgmt_hdr *req_hdr, bool *handler_found)
{
    struct mgmt_ctxt cbuf;
    struct mgmt_hdr rsp_hdr;
    int rc;

    rc = mgmt_ctxt_init(&cbuf, &streamer->reader->r, &streamer->writer->enc);
    if (rc != 0) {
        return rc;
    }

    /* Write a dummy header to the beginning of the response buffer.  Some
     * fields will need to be fixed up later.
     */
    smp_init_rsp_hdr(req_hdr, &rsp_hdr);
    rc = smp_write_hdr(streamer, &rsp_hdr);
    if (rc != 0) {
        return rc;
    }

    /* Process the request and write the response payload. */
    rc = smp_handle_single_payload(&cbuf, req_hdr, handler_found);
    if (rc != 0) {
        return rc;
    }

    /* Fix up the response header with the correct length. */
    rsp_hdr.nh_len = cbor_encode_bytes_written(&cbuf.encoder) - MGMT_HDR_SIZE;
    mgmt_hton_hdr(&rsp_hdr);
    rc = smp_write_hdr(streamer, &rsp_hdr);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Attempts to transmit an SMP error response.  This function consumes both
 * supplied buffers.
 *
 * @param streamer              The SMP streamer for building and transmitting
 *                                  the response.
 * @param req_hdr               The header of the request which elicited the
 *                                  error.
 * @param req                   The buffer holding the request.
 * @param rsp                   The buffer holding the response, or NULL if
 *                                  none was allocated.
 * @param status                The status to indicate in the error response.
 */
static void
smp_on_err(struct smp_streamer *streamer, const struct mgmt_hdr *req_hdr,
           struct os_mbuf *req, struct os_mbuf *rsp, int status)
{
    int rc;

    /* Prefer the response buffer for holding the error response.  If no
     * response buffer was allocated, use the request buffer instead.
     */
    if (rsp == NULL) {
        rsp = req;
        req = NULL;
    }

    /* Clear the partial response from the buffer, if any. */
    smp_reset_buf(rsp);
    cbor_mbuf_writer_init(streamer->writer, rsp);

    /* Build and transmit the error response. */
    rc = smp_build_err_rsp(streamer, req_hdr, status);
    if (rc == 0) {
        streamer->tx_rsp_cb(streamer, rsp, streamer->cb_arg);
        rsp = NULL;
    }

    /* Free any extra buffers. */
    smp_free_buf(req);
    smp_free_buf(rsp);
}

/**
 * Processes all SMP requests in an incoming packet.  Requests are processed
 * sequentially from the start of the packet to the end.  Each response is sent
 * individually in its own packet.  If a request elicits an error response,
 * processing of the packet is aborted.  This function consumes the supplied
 * request buffer regardless of the outcome.
 *
 * @param streamer              The streamer to use for reading, writing, and
 *                                  transmitting.
 * @param req                   A buffer containing the request packet.
 *
 * @return                      0 on success, MGMT_ERR_[...] code on failure.
 */
int
smp_process_request_packet(struct smp_streamer *streamer, struct os_mbuf *req)
{
    struct mgmt_hdr req_hdr;
    struct mgmt_evt_op_cmd_done_arg cmd_done_arg;
    struct os_mbuf *rsp;
    bool valid_hdr, handler_found;
    int rc;

    rsp = NULL;
    valid_hdr = true;

    while (1) {
        handler_found = false;

        cbor_mbuf_reader_init(streamer->reader, req, 0);

        /* Read the management header and strip it from the request. */
        rc = smp_read_hdr(streamer, &req_hdr);
        if (rc != 0) {
            valid_hdr = false;
            break;
        }
        mgmt_ntoh_hdr(&req_hdr);
        os_mbuf_adj(req, MGMT_HDR_SIZE);

        rsp = smp_alloc_rsp(req);
        if (rsp == NULL) {
            rc = MGMT_ERR_ENOMEM;
            break;
        }

        cbor_mbuf_writer_init(streamer->writer, rsp);

        /* Process the request payload and build the response. */
        rc = smp_handle_single_req(streamer, &req_hdr, &handler_found);
        if (rc != 0) {
            break;
        }

        /* Send the response. */
        rc = streamer->tx_rsp_cb(streamer, rsp, streamer->cb_arg);
        rsp = NULL;
        if (rc != 0) {
            break;
        }

        /* Trim processed request to free up space for subsequent responses. */
        os_mbuf_adj(req, smp_align4(req_hdr.nh_len));

        cmd_done_arg.err = MGMT_ERR_EOK;
        mgmt_evt(MGMT_EVT_OP_CMD_DONE, req_hdr.nh_group, req_hdr.nh_id,
                 &cmd_done_arg);
    }

    if (rc != 0 && valid_hdr) {
        smp_on_err(streamer, &req_hdr, req, rsp, rc);

        if (handler_found) {
            cmd_done_arg.err = rc;
            mgmt_evt(MGMT_EVT_OP_CMD_DONE, req_hdr.nh_group, req_hdr.nh_id,
                     &cmd_done_arg);
        }

        return rc;
    }

    smp_free_buf(req);
    smp_free_buf(rsp);
    return 0;
}
