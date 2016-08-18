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
    int rc;
    char name_str[CONF_MAX_NAME_LEN];
    char val_str[CONF_MAX_VAL_LEN];
    char *val;

    const struct json_attr_t attr[2] = {
        [0] = {
            .attribute = "name",
            .type = t_string,
            .addr.string = name_str,
            .len = sizeof(name_str)
        },
        [1] = {
            .attribute = NULL
        }
    };
    struct json_value jv;

    rc = json_read_object(&njb->njb_buf, attr);
    if (rc) {
        return OS_EINVAL;
    }

    val = conf_get_value(name_str, val_str, sizeof(val_str));
    if (!val) {
        return OS_EINVAL;
    }

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_STRING(&jv, val);
    json_encode_object_entry(&njb->njb_enc, "val", &jv);
    json_encode_object_finish(&njb->njb_enc);

    return 0;
}

static int
conf_nmgr_write(struct nmgr_jbuf *njb)
{
    int rc;
    char name_str[CONF_MAX_NAME_LEN];
    char val_str[CONF_MAX_VAL_LEN];

    rc = conf_json_line(&njb->njb_buf, name_str, sizeof(name_str), val_str,
      sizeof(val_str));
    if (rc) {
        return OS_EINVAL;
    }

    rc = conf_set_value(name_str, val_str);
    if (rc) {
        return OS_EINVAL;
    }

    rc = conf_commit(NULL);
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
