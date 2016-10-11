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
#include <os/os.h>
#include <os/endian.h>

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <mgmt/mgmt.h>
#include <bootutil/image.h>
#include <bootutil/bootutil_misc.h>
#include <cborattr/cborattr.h>
#include <hal/hal_bsp.h>

#include "split/split.h"
#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"


int
imgr_boot2_read(struct mgmt_cbuf *cb)
{
    int rc;
    struct image_version ver;
    uint8_t hash[IMGMGR_HASH_LEN];
    int test_slot;
    int main_slot;
    int active_slot;
    CborEncoder *penc = &cb->encoder;
    CborError g_err = CborNoError;
    CborEncoder rsp;

    g_err |= cbor_encoder_create_map(penc, &rsp, CborIndefiniteLength);

    test_slot = -1;
    main_slot = -1;
    active_slot = -1;

    /* Temporary hack to preserve old behavior. */
    if (boot_split_app_active_get()) {
        if (split_mode_get() == SPLIT_MODE_TEST_APP) {
            test_slot = 0;
        }
        main_slot = 0;
        active_slot = 1;
    } else {
        boot_vect_read_test(&test_slot);
        boot_vect_read_main(&main_slot);
        active_slot = boot_current_slot;
    }

    if (test_slot != -1) {
        rc = imgr_read_info(test_slot, &ver, hash, NULL);
        if (rc >= 0) {
            g_err |= cbor_encode_text_stringz(&rsp, "test");
            g_err |= cbor_encode_byte_string(&rsp, hash, IMGMGR_HASH_LEN);
        }
    }

    if (main_slot != -1) {
        rc = imgr_read_info(main_slot, &ver, hash, NULL);
        if (rc >= 0) {
            g_err |= cbor_encode_text_stringz(&rsp, "main");
            g_err |= cbor_encode_byte_string(&rsp, hash, IMGMGR_HASH_LEN);
        }
    }

    if (active_slot != -1) {
        rc = imgr_read_info(active_slot, &ver, hash, NULL);
        if (rc >= 0) {
            g_err |= cbor_encode_text_stringz(&rsp, "active");
            g_err |= cbor_encode_byte_string(&rsp, hash, IMGMGR_HASH_LEN);
        }
    }

    g_err |= cbor_encode_text_stringz(&rsp, "rc");
    g_err |= cbor_encode_int(&rsp, MGMT_ERR_EOK);
    g_err |= cbor_encoder_close_container(penc, &rsp);

    return 0;
}

int
imgr_boot2_write(struct mgmt_cbuf *cb)
{
    int rc;
    uint8_t hash[IMGMGR_HASH_LEN];
    const struct cbor_attr_t boot_write_attr[2] = {
        [0] = {
            .attribute = "test",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = hash,
            .len = sizeof(hash),
        },
        [1] = {
            .attribute = NULL
        }
    };
    struct image_version ver;

    rc = cbor_read_object(&cb->it, boot_write_attr);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    rc = imgr_find_by_hash(hash, &ver);
    if (rc >= 0) {
        rc = boot_vect_write_test(rc);
        if (rc) {
            rc = MGMT_ERR_EUNKNOWN;
            goto err;
        }
        rc = 0;
    } else {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    rc =  MGMT_ERR_EOK;

err:
    mgmt_cbuf_setoerr(cb, rc);

    return 0;
}
