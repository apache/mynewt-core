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
#ifdef COREDUMP_PRESENT

#include <limits.h>

#include <hal/flash_map.h>
#include <newtmgr/newtmgr.h>
#include <coredump/coredump.h>
#include <util/base64.h>

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

int
imgr_core_list(struct nmgr_jbuf *njb)
{
    const struct flash_area *fa;
    struct coredump_header hdr;
    struct json_encoder *enc;
    struct json_value jv;
    int rc;

    rc = flash_area_open(FLASH_AREA_CORE, &fa);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
    } else {
        rc = flash_area_read(fa, 0, &hdr, sizeof(hdr));
        if (rc != 0) {
            rc = NMGR_ERR_EINVAL;
        } else if (hdr.ch_magic != COREDUMP_MAGIC) {
            rc = NMGR_ERR_ENOENT;
        } else {
            rc = 0;
        }
    }

    enc = &njb->njb_enc;

    json_encode_object_start(enc);
    JSON_VALUE_INT(&jv, rc);
    json_encode_object_entry(enc, "rc", &jv);
    json_encode_object_finish(enc);

    return 0;
}

int
imgr_core_load(struct nmgr_jbuf *njb)
{
    unsigned long long off = UINT_MAX;
    const struct json_attr_t dload_attr[2] = {
        [0] = {
            .attribute = "off",
            .type = t_uinteger,
            .addr.uinteger = &off
        }
    };
    int rc;
    int sz;
    const struct flash_area *fa;
    char data[IMGMGR_NMGR_MAX_MSG];
    char encoded[BASE64_ENCODE_SIZE(IMGMGR_NMGR_MAX_MSG)];
    struct coredump_header *hdr;
    struct json_encoder *enc;
    struct json_value jv;

    hdr = (struct coredump_header *)data;

    rc = json_read_object(&njb->njb_buf, dload_attr);
    if (rc || off == UINT_MAX) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = flash_area_open(FLASH_AREA_CORE, &fa);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = flash_area_read(fa, 0, hdr, sizeof(*hdr));
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto err_close;
    }
    if (hdr->ch_magic != COREDUMP_MAGIC) {
        rc = NMGR_ERR_ENOENT;
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
        rc = NMGR_ERR_EINVAL;
        goto err_close;
    }

    sz = base64_encode(data, sz, encoded, 1);

    enc = &njb->njb_enc;

    json_encode_object_start(enc);
    JSON_VALUE_INT(&jv, 0);
    json_encode_object_entry(enc, "rc", &jv);

    JSON_VALUE_INT(&jv, off);
    json_encode_object_entry(enc, "off", &jv);

    JSON_VALUE_STRINGN(&jv, encoded, sz);
    json_encode_object_entry(enc, "data", &jv);
    json_encode_object_finish(enc);

    flash_area_close(fa);
    return 0;

err_close:
    flash_area_close(fa);
err:
    nmgr_jbuf_setoerr(njb, rc);
    return 0;
}

/*
 * Erase the area if it has a coredump, or the header is empty.
 */
int
imgr_core_erase(struct nmgr_jbuf *njb)
{
    struct coredump_header hdr;
    const struct flash_area *fa;
    int rc;

    rc = flash_area_open(FLASH_AREA_CORE, &fa);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = flash_area_read(fa, 0, &hdr, sizeof(hdr));
    if (rc == 0 &&
      (hdr.ch_magic == COREDUMP_MAGIC || hdr.ch_magic == 0xffffffff)) {
        rc = flash_area_erase(fa, 0, fa->fa_size);
        if (rc) {
            rc = NMGR_ERR_EINVAL;
        }
    }
    rc = 0;

    flash_area_close(fa);
err:
    nmgr_jbuf_setoerr(njb, rc);
    return 0;
}

#endif
