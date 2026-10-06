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

#include "omp/omp_priv.h"

int
omp_encode_mgmt_hdr(struct CborEncoder *enc, struct mgmt_hdr hdr)
{
    int rc;

    rc = cbor_encode_text_string(enc, "_h", 2);
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    hdr.nh_len = htons(hdr.nh_len);
    hdr.nh_group = htons(hdr.nh_group);

    /* Encode the MGMT header in the response. */
    rc = cbor_encode_byte_string(enc, (void *)&hdr, sizeof hdr);
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

int
omp_send_err_rsp(struct CborEncoder *enc,
                 const struct mgmt_hdr *hdr,
                 int mgmt_status)
{
    int rc;

    rc = omp_encode_mgmt_hdr(enc, *hdr);
    if (rc != 0) {
        return rc;
    }

    rc = cbor_encode_text_stringz(enc, "rc");
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    rc = cbor_encode_int(enc, mgmt_status);
    if (rc != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

int
omp_read_hdr(struct CborValue *cv, struct mgmt_hdr *out_hdr)
{
    size_t hlen = 0;
    int rc;

    struct cbor_attr_t attrs[] = {
        [0] = {
            .attribute = "_h",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = (void *)out_hdr,
            .addr.bytestring.len = &hlen,
            .nodefault = 1,
            .len = sizeof *out_hdr,
        },
        [1] = { 0 }
    };

    memset(out_hdr, 0, sizeof(*out_hdr));

    rc = cbor_read_object(cv, attrs);
    if (rc != 0 || hlen != sizeof *out_hdr) {
        return MGMT_ERR_EINVAL;
    }

    out_hdr->nh_len = ntohs(out_hdr->nh_len);
    out_hdr->nh_group = ntohs(out_hdr->nh_group);

    return 0;
}

int
omp_process_mgmt_hdr(struct mgmt_hdr *req_hdr,
                     struct mgmt_hdr *rsp_hdr,
                     struct mgmt_ctxt *ctxt)
{
    int rc = 0;
    const struct mgmt_handler *handler;
    mgmt_handler_fn *handler_fn = NULL;

    handler = mgmt_find_handler(req_hdr->nh_group, req_hdr->nh_id);
    if (handler == NULL) {
        rc = MGMT_ERR_ENOENT;
        return rc;
    }

    switch (req_hdr->nh_op) {
    case MGMT_OP_READ:
        rsp_hdr->nh_op = MGMT_OP_READ_RSP;
        handler_fn = handler->mh_read;
        break;

    case MGMT_OP_WRITE:
        rsp_hdr->nh_op = MGMT_OP_WRITE_RSP;
        handler_fn = handler->mh_write;
        break;

    default:
        rc = MGMT_ERR_EINVAL;
    }

    if (handler_fn) {
        rc = handler_fn(ctxt);
    } else {
        rc = MGMT_ERR_ENOTSUP;
    }

    /* Encode the MGMT header in the response. */

    if (rc != 0) {
        if (handler_fn) {
            rc = omp_send_err_rsp(&ctxt->encoder, rsp_hdr, rc);
        }
    } else {
        rc = omp_encode_mgmt_hdr(&ctxt->encoder, *rsp_hdr);
        if (rc != 0) {
            rc = MGMT_ERR_ENOMEM;
        }
    }

    return mgmt_err_from_cbor(rc);
}
