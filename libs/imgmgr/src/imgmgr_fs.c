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
#ifdef FS_PRESENT
#include <os/os.h>
#include <os/endian.h>

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <newtmgr/newtmgr.h>
#include <bootutil/image.h>
#include <fs/fs.h>
#include <json/json.h>
#include <util/base64.h>
#include <bsp/bsp.h>

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

int
imgr_file_download(struct nmgr_jbuf *njb)
{
    long long unsigned int off = UINT_MAX;
    char tmp_str[IMGMGR_NMGR_MAX_NAME + 1];
    char img_data[BASE64_ENCODE_SIZE(IMGMGR_NMGR_MAX_MSG)];
    const struct json_attr_t dload_attr[3] = {
        [0] = {
            .attribute = "off",
            .type = t_uinteger,
            .addr.uinteger = &off
        },
        [1] = {
            .attribute = "name",
            .type = t_string,
            .addr.string = tmp_str,
            .len = sizeof(tmp_str)
        }
    };
    int rc;
    uint32_t out_len;
    struct fs_file *file;
    struct json_encoder *enc;
    struct json_value jv;

    rc = json_read_object(&njb->njb_buf, dload_attr);
    if (rc || off == UINT_MAX) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = fs_open(tmp_str, FS_ACCESS_READ, &file);
    if (rc || !file) {
        rc = NMGR_ERR_ENOMEM;
        goto err;
    }

    rc = fs_seek(file, off);
    if (rc) {
        rc = NMGR_ERR_EUNKNOWN;
        goto err_close;
    }
    rc = fs_read(file, 32, tmp_str, &out_len);
    if (rc) {
        rc = NMGR_ERR_EUNKNOWN;
        goto err_close;
    }

    out_len = base64_encode(tmp_str, out_len, img_data, 1);

    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    JSON_VALUE_UINT(&jv, off);
    json_encode_object_entry(enc, "off", &jv);
    JSON_VALUE_STRINGN(&jv, img_data, out_len);
    json_encode_object_entry(enc, "data", &jv);
    if (off == 0) {
        rc = fs_filelen(file, &out_len);
        JSON_VALUE_UINT(&jv, out_len);
        json_encode_object_entry(enc, "len", &jv);
    }
    fs_close(file);

    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);

    json_encode_object_finish(enc);

    return 0;

err_close:
    fs_close(file);
err:
    nmgr_jbuf_setoerr(njb, rc);
    return 0;
}

int
imgr_file_upload(struct nmgr_jbuf *njb)
{
    char img_data[BASE64_ENCODE_SIZE(IMGMGR_NMGR_MAX_MSG)];
    char file_name[IMGMGR_NMGR_MAX_NAME + 1];
    long long unsigned int off = UINT_MAX;
    long long unsigned int size = UINT_MAX;
    const struct json_attr_t off_attr[5] = {
        [0] = {
            .attribute = "off",
            .type = t_uinteger,
            .addr.uinteger = &off,
            .nodefault = true
        },
        [1] = {
            .attribute = "data",
            .type = t_string,
            .addr.string = img_data,
            .len = sizeof(img_data)
        },
        [2] = {
            .attribute = "len",
            .type = t_uinteger,
            .addr.uinteger = &size,
            .nodefault = true
        },
        [3] = {
            .attribute = "name",
            .type = t_string,
            .addr.string = file_name,
            .len = sizeof(file_name)
        }
    };
    struct json_encoder *enc;
    struct json_value jv;
    int rc;
    int len;

    rc = json_read_object(&njb->njb_buf, off_attr);
    if (rc || off == UINT_MAX) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }
    len = strlen(img_data);
    if (len) {
        len = base64_decode(img_data, img_data);
        if (len < 0) {
            rc = NMGR_ERR_EINVAL;
            goto err;
        }
    }

    if (off == 0) {
        /*
         * New upload.
         */
        imgr_state.upload.off = 0;
        imgr_state.upload.size = size;

        if (!strlen(file_name)) {
            rc = NMGR_ERR_EINVAL;
            goto err;
        }
        if (imgr_state.upload.file) {
            fs_close(imgr_state.upload.file);
            imgr_state.upload.file = NULL;
        }
        rc = fs_open(file_name, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE,
          &imgr_state.upload.file);
        if (rc) {
            rc = NMGR_ERR_EINVAL;
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
        rc = NMGR_ERR_EINVAL;
        goto err;
    }
    if (len) {
        rc = fs_write(imgr_state.upload.file, img_data, len);
        if (rc) {
            rc = NMGR_ERR_EINVAL;
            goto err_close;
        }
        imgr_state.upload.off += len;
        if (imgr_state.upload.size == imgr_state.upload.off) {
            /* Done */
            fs_close(imgr_state.upload.file);
            imgr_state.upload.file = NULL;
        }
    }
out:
    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);

    JSON_VALUE_UINT(&jv, imgr_state.upload.off);
    json_encode_object_entry(enc, "off", &jv);

    json_encode_object_finish(enc);

    return 0;

err_close:
    fs_close(imgr_state.upload.file);
    imgr_state.upload.file = NULL;
err:
    nmgr_jbuf_setoerr(njb, rc);
    return 0;
}
#endif
