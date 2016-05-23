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

#include <string.h>

#include <stdio.h>

#ifdef NEWTMGR_PRESENT

#include "newtmgr/newtmgr.h"
#include "json/json.h"
#include "stats/stats.h"

/* Source code is only included if the newtmgr library is enabled.  Otherwise
 * this file is compiled out for code size.
 */
static int stats_nmgr_read(struct nmgr_jbuf *njb);
static int stats_nmgr_list(struct nmgr_jbuf *njb);

static struct nmgr_group shell_nmgr_group;

#define STATS_NMGR_ID_READ  (0)
#define STATS_NMGR_ID_LIST  (1)

/* ORDER MATTERS HERE.
 * Each element represents the command ID, referenced from newtmgr.
 */
static struct nmgr_handler shell_nmgr_group_handlers[] = {
    [STATS_NMGR_ID_READ] = {stats_nmgr_read, stats_nmgr_read},
    [STATS_NMGR_ID_LIST] = {stats_nmgr_list, stats_nmgr_list}
};

static int
stats_nmgr_walk_func(struct stats_hdr *hdr, void *arg, char *sname,
        uint16_t stat_off)
{
    struct json_encoder *encoder;
    struct json_value jv;
    void *stat_val;
    int rc;

    stat_val = (uint8_t *)hdr + stat_off;

    encoder = (struct json_encoder *) arg;

    switch (hdr->s_size) {
        case sizeof(uint16_t):
            JSON_VALUE_UINT(&jv, *(uint16_t *) stat_val);
            break;
        case sizeof(uint32_t):
            JSON_VALUE_UINT(&jv, *(uint32_t *) stat_val);
            break;
        case sizeof(uint64_t):
            JSON_VALUE_UINT(&jv, *(uint64_t *) stat_val);
            break;
    }

    rc = json_encode_object_entry(encoder, sname, &jv);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int
stats_nmgr_encode_name(struct stats_hdr *hdr, void *arg)
{
    struct json_encoder *encoder;
    struct json_value jv;

    encoder = (struct json_encoder *)arg;
    JSON_VALUE_STRING(&jv, hdr->s_name);
    json_encode_array_value(encoder, &jv);

    return (0);
}

static int
stats_nmgr_read(struct nmgr_jbuf *njb)
{
    struct stats_hdr *hdr;
#define STATS_NMGR_NAME_LEN (32)
    char stats_name[STATS_NMGR_NAME_LEN];
    struct json_attr_t attrs[] = {
        { "name", t_string, .addr.string = &stats_name[0],
            .len = sizeof(stats_name) },
        { NULL },
    };
    struct json_value jv;
    int rc;

    rc = json_read_object((struct json_buffer *) njb, attrs);
    if (rc != 0) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    hdr = stats_group_find(stats_name);
    if (!hdr) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&nmgr_task_jbuf.njb_enc, "rc", &jv);
    JSON_VALUE_STRINGN(&jv, stats_name, strlen(stats_name));
    json_encode_object_entry(&nmgr_task_jbuf.njb_enc, "name", &jv);
    JSON_VALUE_STRINGN(&jv, "sys", sizeof("sys")-1);
    json_encode_object_entry(&nmgr_task_jbuf.njb_enc, "group", &jv);
    json_encode_object_key(&nmgr_task_jbuf.njb_enc, "fields");
    json_encode_object_start(&nmgr_task_jbuf.njb_enc);
    stats_walk(hdr, stats_nmgr_walk_func, &nmgr_task_jbuf.njb_enc);
    json_encode_object_finish(&njb->njb_enc);
    json_encode_object_finish(&njb->njb_enc);

    return (0);
err:
    nmgr_jbuf_setoerr(njb, rc);

    return (0);
}

static int
stats_nmgr_list(struct nmgr_jbuf *njb)
{
    struct json_value jv;

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&nmgr_task_jbuf.njb_enc, "rc", &jv);
    json_encode_array_name(&nmgr_task_jbuf.njb_enc, "stat_list");
    json_encode_array_start(&nmgr_task_jbuf.njb_enc);
    stats_group_walk(stats_nmgr_encode_name, &nmgr_task_jbuf.njb_enc);
    json_encode_array_finish(&njb->njb_enc);
    json_encode_object_finish(&njb->njb_enc);

    return (0);
}

/**
 * Register nmgr group handlers
 */
int
stats_nmgr_register_group(void)
{
    int rc;

    NMGR_GROUP_SET_HANDLERS(&shell_nmgr_group, shell_nmgr_group_handlers);
    shell_nmgr_group.ng_group_id = NMGR_GROUP_ID_STATS;

    rc = nmgr_group_register(&shell_nmgr_group);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

#endif /* NEWTMGR_PRESENT */
