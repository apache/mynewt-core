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
#include <fs/fs.h>
#include <fs/fsutil.h>
#include <json/json.h>
#include <util/base64.h>
#include <bsp/bsp.h>

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

int
imgr_boot_read(struct nmgr_jbuf *njb)
{
    int rc;
    struct json_encoder *enc;
    struct image_version ver;

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

    rc = imgr_read_ver(bsp_imgr_current_slot(), &ver);
    if (!rc) {
        imgr_ver_jsonstr(enc, "active", &ver);
    }

    json_encode_object_finish(enc);

    return 0;
}

int
imgr_boot_write(struct nmgr_jbuf *njb)
{
    char test_ver_str[28];
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
    int rc;
    struct image_version ver;

    rc = json_read_object(&njb->njb_buf, boot_write_attr);
    if (rc) {
        return OS_EINVAL;
    }

    rc = imgr_ver_parse(boot_write_attr[0].addr.string, &ver);
    if (rc) {
        return OS_EINVAL;
    }

    rc = boot_vect_write_test(&ver);
    if (rc) {
        return OS_EINVAL;
    }
    return rc;
}
