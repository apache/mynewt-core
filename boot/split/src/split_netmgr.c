/**
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

#include <tinycbor/cbor.h>
#include <cborattr/cborattr.h>
#include <mgmt/mgmt.h>
#include <bootutil/bootutil_misc.h>
#include <bootutil/image.h>
#include <split/split.h>
#include <split_priv.h>


static int imgr_splitapp_read(struct mgmt_cbuf *cb);
static int imgr_splitapp_write(struct mgmt_cbuf *cb);

static const struct mgmt_handler split_nmgr_handlers[] = {
    [SPLIT_NMGR_OP_SPLIT] = {
        .mh_read = imgr_splitapp_read,
        .mh_write = imgr_splitapp_write
    },
};

static struct mgmt_group split_nmgr_group = {
    .mg_handlers = (struct mgmt_handler *)split_nmgr_handlers,
    .mg_handlers_count =
        sizeof(split_nmgr_handlers) / sizeof(split_nmgr_handlers[0]),
    .mg_group_id = MGMT_GROUP_ID_SPLIT,
};

int
split_nmgr_register(void)
{
    int rc;
    rc = mgmt_group_register(&split_nmgr_group);
    return (rc);
}

int
imgr_splitapp_read(struct mgmt_cbuf *cb)
{
    int x;
    CborError g_err = CborNoError;
    CborEncoder *penc = &cb->encoder;
    CborEncoder rsp;

    g_err |= cbor_encoder_create_map(penc, &rsp, CborIndefiniteLength);

    x = split_mode_get();
    g_err |= cbor_encode_text_stringz(&rsp, "splitMode");
    g_err |= cbor_encode_int(&rsp, x);

    x = split_check_status();
    g_err |= cbor_encode_text_stringz(&rsp, "splitStatus");
    g_err |= cbor_encode_int(&rsp, x);

    g_err |= cbor_encode_text_stringz(&rsp, "rc");
    g_err |= cbor_encode_int(&rsp, MGMT_ERR_EOK);

    g_err |= cbor_encoder_close_container(penc, &rsp);

    return 0;
}

int
imgr_splitapp_write(struct mgmt_cbuf *cb)
{
    long long int split_mode;
    long long int send_split_status;  /* ignored */
    long long int sent_rc; /* ignored */
    const struct cbor_attr_t split_write_attr[4] = {
        [0] =
        {
            .attribute = "splitMode",
            .type = CborAttrIntegerType,
            .addr.integer = &split_mode,
            .nodefault = true,
        },
        [1] =
        {
            .attribute = "splitStatus",
            .type = CborAttrIntegerType,
            .addr.integer = &send_split_status,
            .nodefault = true,
        },
        [2] =
        {
            .attribute = "rc",
            .type = CborAttrIntegerType,
            .addr.integer = &sent_rc,
            .nodefault = true,
        },
        [3] =
        {
            .attribute = NULL
        }
    };
    int rc;

    rc = cbor_read_object(&cb->it, split_write_attr);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    rc = split_write_split(split_mode);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    rc = MGMT_ERR_EOK;
err:
    mgmt_cbuf_setoerr(cb, rc);
    return 0;
}
