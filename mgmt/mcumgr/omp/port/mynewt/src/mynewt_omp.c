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

#include <mgmt/mgmt.h>
#include <cborattr/cborattr.h>
#include <tinycbor/cbor.h>

#include "omp/omp.h"
#include "omp/omp_priv.h"

int
omp_impl_process_request_packet(struct omp_state *omgr_st, void *req_buf)
{
    struct mgmt_ctxt ctxt;
    struct omp_streamer *streamer;
    struct mgmt_hdr req_hdr, rsp_hdr;
    struct os_mbuf *req_m;
    int rc = 0;

    assert(omgr_st);
    assert(req_buf);

    streamer = &omgr_st->omp_stmr;
    omgr_st->m_ctxt = &ctxt;

    req_m = (struct os_mbuf *) req_buf;

    rc = mgmt_streamer_init_reader(&streamer->mgmt_stmr, req_m);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        return rc;

    }

    rc = omp_read_hdr(&ctxt.it, &req_hdr);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        return rc;

    }

    memcpy(&rsp_hdr, &req_hdr, sizeof(struct mgmt_hdr));

    rc = mgmt_streamer_init_reader(&streamer->mgmt_stmr, req_m);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        return rc;

    }

    rc = cbor_encoder_create_map(streamer->rsp_encoder,
                                 &ctxt.encoder,
                                 CborIndefiniteLength);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        return rc;
    }

    rc = omp_process_mgmt_hdr(&req_hdr, &rsp_hdr, &ctxt);

    cbor_encoder_close_container(streamer->rsp_encoder, &ctxt.encoder);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        return rc;

    }

    streamer->tx_rsp_cb(streamer, rc, NULL);
    return 0;
}
