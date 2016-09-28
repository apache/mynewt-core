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

#include <json/json.h>
#include <newtmgr/newtmgr.h>
#include <bootutil/bootutil_misc.h>
#include <bootutil/image.h>
#include <split/split.h>
#include <split/split_priv.h>


static int imgr_splitapp_read(struct nmgr_jbuf *njb);
static int imgr_splitapp_write(struct nmgr_jbuf *njb);

static const struct nmgr_handler split_nmgr_handlers[] = {
    [SPLIT_NMGR_OP_SPLIT] = {
        .nh_read = imgr_splitapp_read,
        .nh_write = imgr_splitapp_write
    },
};

static struct nmgr_group split_nmgr_group = {
    .ng_handlers = (struct nmgr_handler *)split_nmgr_handlers,
    .ng_handlers_count =
        sizeof(split_nmgr_handlers) / sizeof(split_nmgr_handlers[0]),
    .ng_group_id = NMGR_GROUP_ID_SPLIT,
};

int
split_nmgr_register(void)
{
    int rc;
    rc = nmgr_group_register(&split_nmgr_group);
    return (rc);
}

int
imgr_splitapp_read(struct nmgr_jbuf *njb)
{
    int rc;
    int x;
    split_mode_t split;
    struct json_encoder *enc;
    struct json_value jv;

    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    rc = split_read_split(&split);
    if (!rc) {
        x = split;
    } else {
        x = SPLIT_NONE;
    }
    JSON_VALUE_INT(&jv, x)
    json_encode_object_entry(enc, "splitMode", &jv);

    x = split_check_status();
    JSON_VALUE_INT(&jv, x)
    json_encode_object_entry(enc, "splitStatus", &jv);

    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(enc, "rc", &jv);

    json_encode_object_finish(enc);

    return 0;
}

int
imgr_splitapp_write(struct nmgr_jbuf *njb)
{
    long long int split_mode;
    long long int send_split_status;  /* ignored */
    long long int sent_rc; /* ignored */
    const struct json_attr_t split_write_attr[4] = {
        [0] =
        {
            .attribute = "splitMode",
            .type = t_integer,
            .addr.integer = &split_mode,
            .nodefault = true,
        },
        [1] =
        {
            .attribute = "splitStatus",
            .type = t_integer,
            .addr.integer = &send_split_status,
            .nodefault = true,
        },
        [2] =
        {
            .attribute = "rc",
            .type = t_integer,
            .addr.integer = &sent_rc,
            .nodefault = true,
        },
        [3] =
        {
            .attribute = NULL
        }
    };
    struct json_encoder *enc;
    struct json_value jv;
    int rc;

    rc = json_read_object(&njb->njb_buf, split_write_attr);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = split_write_split((split_mode_t) split_mode);
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
