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

#ifdef NEWTMGR_PRESENT

#include <string.h>

#include <newtmgr/newtmgr.h>
#include <json/json.h>

#include "config/config.h"
#include "config_priv.h"

static int conf_nmgr_read(struct nmgr_jbuf *);
static int conf_nmgr_write(struct nmgr_jbuf *);

static const struct nmgr_handler conf_nmgr_handlers[] = {
    [CONF_NMGR_OP] = { conf_nmgr_read, conf_nmgr_write}
};

static struct nmgr_group conf_nmgr_group = {
    .ng_handlers = (struct nmgr_handler *)conf_nmgr_handlers,
    .ng_handlers_count = 1,
    .ng_group_id = NMGR_GROUP_ID_CONFIG
};

static int
conf_nmgr_read(struct nmgr_jbuf *njb)
{
    char tmp_str[max(CONF_MAX_DIR_DEPTH * 8, 16)];
    char *val_str;
    const struct json_attr_t attr[2] = {
        [0] = {
            .attribute = "name",
            .type = t_string,
            .addr.string = tmp_str,
            .len = sizeof(tmp_str)
        },
        [1] = {
            .attribute = NULL
        }
    };
    struct json_value jv;
    int rc;

    rc = json_read_object(&njb->njb_buf, attr);
    if (rc) {
        return OS_EINVAL;
    }

    val_str = conf_get_value(tmp_str, tmp_str, sizeof(tmp_str));
    if (!val_str) {
        return OS_EINVAL;
    }

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_STRING(&jv, val_str);
    json_encode_object_entry(&njb->njb_enc, "val", &jv);
    json_encode_object_finish(&njb->njb_enc);

    return 0;
}

static int
conf_nmgr_write(struct nmgr_jbuf *njb)
{
    char name_str[CONF_MAX_DIR_DEPTH * 8];
    char val_str[256];
    struct json_attr_t attr[3] = {
        [0] = {
            .attribute = "name",
            .type = t_string,
            .addr.string = name_str,
            .len = sizeof(name_str)
        },
        [1] = {
            .attribute = "val",
            .type = t_string,
            .addr.string = val_str,
            .len = sizeof(val_str)
        },
        [2] = {
            .attribute = NULL
        }
    };
    int rc;

    rc = json_read_object(&njb->njb_buf, attr);
    if (rc) {
        return OS_EINVAL;
    }

    rc = conf_set_value(name_str, val_str);
    if (rc) {
        return OS_EINVAL;
    }
    return 0;
}

int
conf_nmgr_register(void)
{
    return nmgr_group_register(&conf_nmgr_group);
}
#endif
