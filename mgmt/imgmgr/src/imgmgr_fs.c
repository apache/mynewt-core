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

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(IMGMGR_FS)

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "os/os.h"
#include "os/endian.h"
#include "newtmgr/newtmgr.h"
#include "bootutil/image.h"
#include "fs/fs.h"
#include "cborattr/cborattr.h"
#include "bsp/bsp.h"
#include "mgmt/mgmt.h"

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

int
imgr_file_download(struct mgmt_cbuf *cb)
{
    long long unsigned int off = UINT_MAX;
    char tmp_str[IMGMGR_NMGR_MAX_NAME + 1];
    uint8_t img_data[IMGMGR_NMGR_MAX_MSG];
    const struct cbor_attr_t dload_attr[3] = {
        [0] = {
            .attribute = "off",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &off
        },
        [1] = {
            .attribute = "name",
            .type = CborAttrTextStringType,
            .addr.string = tmp_str,
            .len = sizeof(tmp_str)
        },
        [2] = { 0 },
    };
    int rc;
    uint32_t out_len;
    struct fs_file *file;
    CborError g_err = CborNoError;
    CborEncoder *penc = &cb->encoder;
    CborEncoder rsp;

    rc = cbor_read_object(&cb->it, dload_attr);
    if (rc || off == UINT_MAX) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    rc = fs_open(tmp_str, FS_ACCESS_READ, &file);
    if (rc || !file) {
        rc = MGMT_ERR_ENOMEM;
        goto err;
    }

    rc = fs_seek(file, off);
    if (rc) {
        rc = MGMT_ERR_EUNKNOWN;
        goto err_close;
    }
    rc = fs_read(file, 32, img_data, &out_len);
    if (rc) {
        rc = MGMT_ERR_EUNKNOWN;
        goto err_close;
    }

    g_err |= cbor_encoder_create_map(penc, &rsp, CborIndefiniteLength);

    g_err |= cbor_encode_text_stringz(&rsp, "off");
    g_err |= cbor_encode_uint(&rsp, off);

    g_err |= cbor_encode_text_stringz(&rsp, "data");
    g_err |= cbor_encode_byte_string(&rsp, img_data, out_len);

    g_err |= cbor_encode_text_stringz(&rsp, "rc");
    g_err |= cbor_encode_int(&rsp, MGMT_ERR_EOK);
    if (off == 0) {
        rc = fs_filelen(file, &out_len);
        g_err |= cbor_encode_text_stringz(&rsp, "len");
        g_err |= cbor_encode_uint(&rsp, out_len);
    }
    g_err |= cbor_encoder_close_container(penc, &rsp);

    fs_close(file);
    return 0;

err_close:
    fs_close(file);
err:
    mgmt_cbuf_setoerr(cb, rc);
    return 0;
}

int
imgr_file_upload(struct mgmt_cbuf *cb)
{
    uint8_t img_data[IMGMGR_NMGR_MAX_MSG];
    char file_name[IMGMGR_NMGR_MAX_NAME + 1];
    size_t img_len;
    long long unsigned int off = UINT_MAX;
    long long unsigned int size = UINT_MAX;
    const struct cbor_attr_t off_attr[5] = {
        [0] = {
            .attribute = "off",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &off,
            .nodefault = true
        },
        [1] = {
            .attribute = "data",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = img_data,
            .addr.bytestring.len = &img_len,
            .len = sizeof(img_data)
        },
        [2] = {
            .attribute = "len",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &size,
            .nodefault = true
        },
        [3] = {
            .attribute = "name",
            .type = CborAttrTextStringType,
            .addr.string = file_name,
            .len = sizeof(file_name)
        },
        [4] = { 0 },
    };
    CborError g_err = CborNoError;
    CborEncoder *penc = &cb->encoder;
    CborEncoder rsp;
    int rc;

    rc = cbor_read_object(&cb->it, off_attr);
    if (rc || off == UINT_MAX) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    if (off == 0) {
        /*
         * New upload.
         */
        imgr_state.upload.off = 0;
        imgr_state.upload.size = size;

        if (!strlen(file_name)) {
            rc = MGMT_ERR_EINVAL;
            goto err;
        }
        if (imgr_state.upload.file) {
            fs_close(imgr_state.upload.file);
            imgr_state.upload.file = NULL;
        }
        rc = fs_open(file_name, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE,
          &imgr_state.upload.file);
        if (rc) {
            rc = MGMT_ERR_EINVAL;
            goto err;
        }
    } else if (off != imgr_state.upload.off) {
        /*
         * Invalid offset. Drop the data, and respond with the offset we're
         * expecting data for.
         */
        goto out;
    }

    if (!imgr_state.upload.file) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }
    if (img_len) {
        rc = fs_write(imgr_state.upload.file, img_data, img_len);
        if (rc) {
            rc = MGMT_ERR_EINVAL;
            goto err_close;
        }
        imgr_state.upload.off += img_len;
        if (imgr_state.upload.size == imgr_state.upload.off) {
            /* Done */
            fs_close(imgr_state.upload.file);
            imgr_state.upload.file = NULL;
        }
    }
out:
    g_err |= cbor_encoder_create_map(penc, &rsp, CborIndefiniteLength);
    g_err |= cbor_encode_text_stringz(&rsp, "rc");
    g_err |= cbor_encode_int(&rsp, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&rsp, "off");
    g_err |= cbor_encode_uint(&rsp, imgr_state.upload.off);
    g_err |= cbor_encoder_close_container(penc, &rsp);
    return 0;

err_close:
    fs_close(imgr_state.upload.file);
    imgr_state.upload.file = NULL;
err:
    mgmt_cbuf_setoerr(cb, rc);
    return 0;
}
#endif
