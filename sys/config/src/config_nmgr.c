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

#if MYNEWT_VAL(CONFIG_NEWTMGR)

#include <string.h>

#include "mgmt/mgmt.h"
#include "json/json.h"

#include "config/config.h"
#include "config_priv.h"

static int conf_nmgr_read(struct mgmt_jbuf *);
static int conf_nmgr_write(struct mgmt_jbuf *);

static const struct mgmt_handler conf_nmgr_handlers[] = {
    [CONF_NMGR_OP] = { conf_nmgr_read, conf_nmgr_write}
};

static struct mgmt_group conf_nmgr_group = {
    .mg_handlers = (struct mgmt_handler *)conf_nmgr_handlers,
    .mg_handlers_count = 1,
    .mg_group_id = MGMT_GROUP_ID_CONFIG
};

static int
conf_nmgr_read(struct mgmt_jbuf *njb)
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

    rc = json_read_object(&njb->mjb_buf, attr);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    val = conf_get_value(name_str, val_str, sizeof(val_str));
    if (!val) {
        return MGMT_ERR_EINVAL;
    }

    json_encode_object_start(&njb->mjb_enc);
    JSON_VALUE_STRING(&jv, val);
    json_encode_object_entry(&njb->mjb_enc, "val", &jv);
    json_encode_object_finish(&njb->mjb_enc);

    return 0;
}

static int
conf_nmgr_write(struct mgmt_jbuf *njb)
{
    int rc;
    char name_str[CONF_MAX_NAME_LEN];
    char val_str[CONF_MAX_VAL_LEN];

    rc = conf_json_line(&njb->mjb_buf, name_str, sizeof(name_str), val_str,
      sizeof(val_str));
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    rc = conf_set_value(name_str, val_str);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    rc = conf_commit(NULL);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }
    return 0;
}

int
conf_nmgr_register(void)
{
    return mgmt_group_register(&conf_nmgr_group);
}
#endif
