/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
static int stats_nmgr_read(struct nmgr_hdr *, struct os_mbuf *, uint16_t,
        struct nmgr_hdr *, struct os_mbuf *);

static struct nmgr_group shell_nmgr_group;

#define STATS_NMGR_ID_READ (0) 

/* ORDER MATTERS HERE.
 * Each element represents the command ID, referenced from newtmgr.
 */
static struct nmgr_handler shell_nmgr_group_handlers[] = {
    [STATS_NMGR_ID_READ] = {stats_nmgr_read, stats_nmgr_read},
};

static int 
stats_nmgr_walk_func(struct stats_hdr *hdr, void *arg, char *sname, 
        uint8_t *stat)
{
    struct json_encoder *encoder;
    struct json_value jv;
    int rc;

    encoder = (struct json_encoder *) arg;

    switch (hdr->s_size) {
        case sizeof(uint16_t):
            JSON_VALUE_UINT(&jv, *(uint16_t *) stat);
            break;
        case sizeof(uint32_t): 
            JSON_VALUE_UINT(&jv, *(uint32_t *) stat);
            break;
        case sizeof(uint64_t):
            JSON_VALUE_UINT(&jv, *(uint64_t *) stat);
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
stats_nmgr_read(struct nmgr_hdr *nmr, struct os_mbuf *req, uint16_t srcoff, 
        struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    struct stats_hdr *hdr;
#define STATS_NMGR_NAME_LEN (32)
    char stats_name[STATS_NMGR_NAME_LEN];
    struct json_attr_t attrs[] = {
        { "n", t_string, .addr.string = &stats_name[0], 
            .len = sizeof(stats_name) },
        { NULL },
    };
    struct json_value jv;
    int rc;

    rc = nmgr_jbuf_setibuf(&nmgr_task_jbuf, req, srcoff + sizeof(*nmr), 
            nmr->nh_len);
    if (rc != 0) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = json_read_object((struct json_buffer *) &nmgr_task_jbuf, attrs);
    if (rc != 0) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    hdr = stats_group_find(stats_name);
    if (!hdr) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }

    rc = nmgr_jbuf_setobuf(&nmgr_task_jbuf, rsp_hdr, rsp);
    if (rc != 0) {
        rc = NMGR_ERR_EUNKNOWN;
        goto err;
    }

    json_encode_object_start(&nmgr_task_jbuf.njb_enc);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&nmgr_task_jbuf.njb_enc, "r", &jv);
    JSON_VALUE_STRINGN(&jv, stats_name, strlen(stats_name));
    json_encode_object_entry(&nmgr_task_jbuf.njb_enc, "n", &jv);
    json_encode_object_key(&nmgr_task_jbuf.njb_enc, "f");
    json_encode_object_start(&nmgr_task_jbuf.njb_enc);
    stats_walk(hdr, stats_nmgr_walk_func, &nmgr_task_jbuf.njb_enc);
    json_encode_object_finish(&nmgr_task_jbuf.njb_enc);
    json_encode_object_finish(&nmgr_task_jbuf.njb_enc);

    return (0);
err:
    nmgr_jbuf_setoerr(&nmgr_task_jbuf, rsp_hdr, rsp, rc);

    return (0);
}


/**
 * Register nmgr group handlers.   
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
