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

#include <newtmgr/newtmgr.h>
#include <bootutil/image.h>
#include <bootutil/bootutil_misc.h>
#include <json/json.h>
#include <util/base64.h>
#include <hal/hal_bsp.h>

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

static void
imgr_ver_jsonstr(struct json_encoder *enc, char *key,
  struct image_version *ver)
{
    struct json_value jv;
    char ver_str[IMGMGR_NMGR_MAX_VER];
    int ver_len;

    ver_len = imgr_ver_str(ver, ver_str);
    JSON_VALUE_STRINGN(&jv, ver_str, ver_len);
    json_encode_object_entry(enc, key, &jv);
}

static void
imgr_hash_jsonstr(struct json_encoder *enc, char *key, uint8_t *hash)
{
    struct json_value jv;
    char hash_str[IMGMGR_HASH_STR + 1];

    base64_encode(hash, IMGMGR_HASH_LEN, hash_str, 1);
    JSON_VALUE_STRING(&jv, hash_str);
    json_encode_object_entry(enc, key, &jv);
}

int
imgr_boot_read(struct nmgr_jbuf *njb)
{
    int rc;
    struct json_encoder *enc;
    struct image_version ver;
    struct json_value jv;
    uint8_t hash[IMGMGR_HASH_LEN];

    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    rc = boot_vect_read_test(&ver);
    if (!rc) {
        imgr_ver_jsonstr(enc, "test", &ver);
    }

    rc = boot_vect_read_main(&ver);
    if (!rc) {
        imgr_ver_jsonstr(enc, "main", &ver);
    }

    rc = imgr_read_info(bsp_imgr_current_slot(), &ver, hash);
    if (!rc) {
        imgr_ver_jsonstr(enc, "active", &ver);
    }

    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(enc, "rc", &jv);

    json_encode_object_finish(enc);

    return 0;
}

int
imgr_boot_write(struct nmgr_jbuf *njb)
{
    char test_ver_str[28];
    uint8_t hash[IMGMGR_HASH_LEN];
    const struct json_attr_t boot_write_attr[2] = {
        [0] = {
            .attribute = "test",
            .type = t_string,
            .addr.string = test_ver_str,
            .len = sizeof(test_ver_str),
        },
        [1] = {
            .attribute = NULL
        }
    };
    struct json_encoder *enc;
    struct json_value jv;
    int rc;
    struct image_version ver;

    rc = json_read_object(&njb->njb_buf, boot_write_attr);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = imgr_ver_parse(boot_write_attr[0].addr.string, &ver);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = imgr_find_by_ver(&ver, hash);
    if (rc < 0) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }
    rc = boot_vect_write_test(&ver);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(enc, "rc", &jv);

    json_encode_object_finish(enc);

    return 0;

err:
    nmgr_jbuf_setoerr(njb, rc);
    return 0;
}

int
imgr_boot2_read(struct nmgr_jbuf *njb)
{
    int rc;
    struct json_encoder *enc;
    struct image_version ver;
    struct json_value jv;
    uint8_t hash[IMGMGR_HASH_LEN];

    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    rc = boot_vect_read_test(&ver);
    if (!rc) {
        rc = imgr_find_by_ver(&ver, hash);
        if (rc >= 0) {
            imgr_hash_jsonstr(enc, "test", hash);
        }
    }

    rc = boot_vect_read_main(&ver);
    if (!rc) {
        rc = imgr_find_by_ver(&ver, hash);
        if (rc >= 0) {
            imgr_hash_jsonstr(enc, "main", hash);
        }
    }

    rc = imgr_read_info(bsp_imgr_current_slot(), &ver, hash);
    if (!rc) {
        imgr_hash_jsonstr(enc, "active", hash);
    }

    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(enc, "rc", &jv);

    json_encode_object_finish(enc);

    return 0;
}

int
imgr_boot2_write(struct nmgr_jbuf *njb)
{
    char hash_str[IMGMGR_HASH_STR + 1];
    uint8_t hash[IMGMGR_HASH_LEN];
    const struct json_attr_t boot_write_attr[2] = {
        [0] = {
            .attribute = "test",
            .type = t_string,
            .addr.string = hash_str,
            .len = sizeof(hash_str),
        },
        [1] = {
            .attribute = NULL
        }
    };
    struct json_encoder *enc;
    struct json_value jv;
    int rc;
    struct image_version ver;

    rc = json_read_object(&njb->njb_buf, boot_write_attr);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    base64_decode(hash_str, hash);
    rc = imgr_find_by_hash(hash, &ver);
    if (rc >= 0) {
        rc = boot_vect_write_test(&ver);
        if (rc) {
            rc = NMGR_ERR_EUNKNOWN;
            goto err;
        }
    } else {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);

    json_encode_object_finish(enc);

    return 0;

err:
    nmgr_jbuf_setoerr(njb, rc);

    return 0;
}
