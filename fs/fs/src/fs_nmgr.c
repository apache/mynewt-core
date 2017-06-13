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

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(FS_NMGR)

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "os/os.h"
#include "os/endian.h"
#include "bootutil/image.h"
#include "fs/fs.h"
#include "cborattr/cborattr.h"
#include "bsp/bsp.h"
#include "mgmt/mgmt.h"

#include "fs/fs.h"
#include "fs_priv.h"

static struct {
    struct {
        uint32_t off;
        uint32_t size;
        const struct flash_area *fa;
        struct fs_file *file;
    } upload;
} fs_nmgr_state;

static int fs_nmgr_file_download(struct mgmt_cbuf *cb);
static int fs_nmgr_file_upload(struct mgmt_cbuf *cb);

static const struct mgmt_handler fs_nmgr_handlers[] = {
    [FS_NMGR_ID_FILE] = {
        .mh_read = fs_nmgr_file_download,
        .mh_write = fs_nmgr_file_upload
    },
};

#define FS_NMGR_HANDLER_CNT                                                \
    sizeof(fs_nmgr_handlers) / sizeof(fs_nmgr_handlers[0])

static struct mgmt_group fs_nmgr_group = {
    .mg_handlers = fs_nmgr_handlers,
    .mg_handlers_count = FS_NMGR_HANDLER_CNT,
    .mg_group_id = MGMT_GROUP_ID_FS,
};

static int
fs_nmgr_file_download(struct mgmt_cbuf *cb)
{
    long long unsigned int off = UINT_MAX;
    char tmp_str[FS_NMGR_MAX_NAME + 1];
    uint8_t img_data[MYNEWT_VAL(FS_UPLOAD_MAX_CHUNK_SIZE)];
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

    rc = cbor_read_object(&cb->it, dload_attr);
    if (rc || off == UINT_MAX) {
        return MGMT_ERR_EINVAL;
    }

    rc = fs_open(tmp_str, FS_ACCESS_READ, &file);
    if (rc || !file) {
        return MGMT_ERR_ENOMEM;
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

    g_err |= cbor_encode_text_stringz(&cb->encoder, "off");
    g_err |= cbor_encode_uint(&cb->encoder, off);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "data");
    g_err |= cbor_encode_byte_string(&cb->encoder, img_data, out_len);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    if (off == 0) {
        rc = fs_filelen(file, &out_len);
        g_err |= cbor_encode_text_stringz(&cb->encoder, "len");
        g_err |= cbor_encode_uint(&cb->encoder, out_len);
    }

    fs_close(file);
    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;

err_close:
    fs_close(file);
    return rc;
}

static int
fs_nmgr_file_upload(struct mgmt_cbuf *cb)
{
    uint8_t img_data[MYNEWT_VAL(FS_UPLOAD_MAX_CHUNK_SIZE)];
    char file_name[FS_NMGR_MAX_NAME + 1];
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
    int rc;

    rc = cbor_read_object(&cb->it, off_attr);
    if (rc || off == UINT_MAX) {
        return MGMT_ERR_EINVAL;
    }

    if (off == 0) {
        /*
         * New upload.
         */
        fs_nmgr_state.upload.off = 0;
        fs_nmgr_state.upload.size = size;

        if (!strlen(file_name)) {
            return MGMT_ERR_EINVAL;
        }
        if (fs_nmgr_state.upload.file) {
            fs_close(fs_nmgr_state.upload.file);
            fs_nmgr_state.upload.file = NULL;
        }
        rc = fs_open(file_name, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE,
          &fs_nmgr_state.upload.file);
        if (rc) {
            return MGMT_ERR_EINVAL;
        }
    } else if (off != fs_nmgr_state.upload.off) {
        /*
         * Invalid offset. Drop the data, and respond with the offset we're
         * expecting data for.
         */
        goto out;
    }

    if (!fs_nmgr_state.upload.file) {
        return MGMT_ERR_EINVAL;
    }
    if (img_len) {
        rc = fs_write(fs_nmgr_state.upload.file, img_data, img_len);
        if (rc) {
            rc = MGMT_ERR_EINVAL;
            goto err_close;
        }
        fs_nmgr_state.upload.off += img_len;
        if (fs_nmgr_state.upload.size == fs_nmgr_state.upload.off) {
            /* Done */
            fs_close(fs_nmgr_state.upload.file);
            fs_nmgr_state.upload.file = NULL;
        }
    }

out:
    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "off");
    g_err |= cbor_encode_uint(&cb->encoder, fs_nmgr_state.upload.off);
    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;

err_close:
    fs_close(fs_nmgr_state.upload.file);
    fs_nmgr_state.upload.file = NULL;
    return rc;
}

int
fs_nmgr_init(void)
{
    int rc;

    rc = mgmt_group_register(&fs_nmgr_group);
    return rc;
}
#endif
