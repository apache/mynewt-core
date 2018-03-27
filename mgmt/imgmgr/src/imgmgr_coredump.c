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

#include "os/mynewt.h"

#if MYNEWT_VAL(IMGMGR_COREDUMP)

#include <limits.h>

#include "flash_map/flash_map.h"
#include "mgmt/mgmt.h"
#include "coredump/coredump.h"
#include "cborattr/cborattr.h"

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

int
imgr_core_list(struct mgmt_cbuf *cb)
{
    const struct flash_area *fa;
    struct coredump_header hdr;
    int rc;

    rc = flash_area_open(MYNEWT_VAL(COREDUMP_FLASH_AREA), &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = flash_area_read(fa, 0, &hdr, sizeof(hdr));
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    if (hdr.ch_magic != COREDUMP_MAGIC) {
        return MGMT_ERR_ENOENT;
    }

    rc = mgmt_cbuf_setoerr(cb, 0);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
imgr_core_load(struct mgmt_cbuf *cb)
{
    unsigned long long off = UINT_MAX;
    const struct cbor_attr_t dload_attr[2] = {
        [0] = {
            .attribute = "off",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &off
        },
        [1] = { 0 },
    };
    int rc;
    int sz;
    const struct flash_area *fa;
    uint8_t data[MYNEWT_VAL(IMGMGR_MAX_CHUNK_SIZE)];
    struct coredump_header *hdr;
    CborError g_err = CborNoError;

    hdr = (struct coredump_header *)data;

    rc = cbor_read_object(&cb->it, dload_attr);
    if (rc || off == UINT_MAX) {
        return MGMT_ERR_EINVAL;
    }

    rc = flash_area_open(MYNEWT_VAL(COREDUMP_FLASH_AREA), &fa);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    rc = flash_area_read(fa, 0, hdr, sizeof(*hdr));
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err_close;
    }
    if (hdr->ch_magic != COREDUMP_MAGIC) {
        rc = MGMT_ERR_ENOENT;
        goto err_close;
    }
    if (off > hdr->ch_size) {
        off = hdr->ch_size;
    }
    sz = hdr->ch_size - off;
    if (sz > sizeof(data)) {
        sz = sizeof(data);
    }

    rc = flash_area_read(fa, off, data, sz);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err_close;
    }

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "off");
    g_err |= cbor_encode_int(&cb->encoder, off);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "data");
    g_err |= cbor_encode_byte_string(&cb->encoder, data, sz);

    /* Only include length in first response. */
    if (off == 0) {
        g_err |= cbor_encode_text_stringz(&cb->encoder, "len");
        g_err |= cbor_encode_uint(&cb->encoder, hdr->ch_size);
    }

    flash_area_close(fa);
    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;

err_close:
    flash_area_close(fa);
    return rc;
}

/*
 * Erase the area if it has a coredump, or the header is empty.
 */
int
imgr_core_erase(struct mgmt_cbuf *cb)
{
    struct coredump_header hdr;
    const struct flash_area *fa;
    int rc;

    rc = flash_area_open(MYNEWT_VAL(COREDUMP_FLASH_AREA), &fa);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    rc = flash_area_read(fa, 0, &hdr, sizeof(hdr));
    if (rc == 0 &&
      (hdr.ch_magic == COREDUMP_MAGIC || hdr.ch_magic == 0xffffffff)) {
        rc = flash_area_erase(fa, 0, fa->fa_size);
        if (rc != 0) {
            return MGMT_ERR_EINVAL;
        }
    }

    flash_area_close(fa);

    rc = mgmt_cbuf_setoerr(cb, rc);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

#endif
