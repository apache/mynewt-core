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
#include "omp/omp.h"

#include "tinycbor/cbor.h"
#include "tinycbor/cbor_mbuf_writer.h"
#include "tinycbor/cbor_mbuf_reader.h"
#include "cborattr/cborattr.h"
#include "omp/omp.h"

#include "oic/oc_api.h"

static struct omp_state omgr_state;

static void
oic_tx_rsp(struct omp_streamer *stmr, int retval, void *unused)
{
    oc_send_response((oc_request_t *) omgr_state.request, retval);
}

/**
 * Processes a single OMP request and sends the corresponding response(s).
 */
void
omgr_process_request(oc_request_t *req, oc_interface_mask_t mask)
{
    struct cbor_mbuf_reader reader;
    struct os_mbuf *m_req;
    uint16_t req_data_off;
    int rc = 0;

    if (!req) {
        rc = MGMT_ERR_EINVAL;
        goto done;
    }

    coap_get_payload(req->packet, &m_req, &req_data_off);
    omgr_state = (struct omp_state) {
        .omp_stmr = {
            .reader = &reader,
            .req_off = req_data_off,
            .rsp_encoder = &g_encoder,
            .tx_rsp_cb = oic_tx_rsp,
        },
        .request = req,
    };

    switch (mask) {
    case OC_IF_BASELINE:
        oc_process_baseline_interface(req->resource);
        /* Fallthrough */

    case OC_IF_RW:
        rc = omp_process_request_packet(&omgr_state, m_req);
        break;

    default:
        rc = OC_STATUS_BAD_REQUEST;
    }

done:
    switch (rc) {
        case 0:
            break;

        case MGMT_ERR_ENOMEM:
            rc = OC_STATUS_INTERNAL_SERVER_ERROR;
            break;

        default:
            rc = OC_STATUS_BAD_REQUEST;
            break;
    }
    if (rc != 0) {
        oc_send_response(req, rc);
    }

}

int
omgr_pkg_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    uint8_t mode;
    oc_resource_t *res = NULL;

    /*
     * net/oic must be initialized before now.
     */
    res = oc_new_resource("/omgr", 1, 0);
    oc_resource_bind_resource_type(res, MYNEWT_VAL(OICMGR_OIC_RESOURCE_NAME));
    mode = OC_IF_RW;
    oc_resource_bind_resource_interface(res, mode);
    oc_resource_set_default_interface(res, mode);
    oc_resource_set_discoverable(res);
    oc_resource_set_request_handler(res, OC_PUT, omgr_process_request);
    res->properties |= MYNEWT_VAL(OICMGR_TRANS_SECURITY);
    oc_add_resource(res);

    return 0;
}

int
omgr_extract_req_hdr(oc_request_t *req, struct nmgr_hdr *out_hdr)
{
    uint16_t data_off;
    struct os_mbuf *m;
    int rc;
    struct cbor_mbuf_reader reader;
    CborParser parser;
    CborValue it;

    coap_get_payload(req->packet, &m, &data_off);

    cbor_mbuf_reader_init(&reader, m, data_off);
    cbor_parser_init(&reader.r, 0, &parser,
                     &it);

    rc = omp_read_hdr(&it, out_hdr);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    return 0;
}
