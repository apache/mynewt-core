/*
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

#include <string.h>
#include <stdio.h>

#include "os/mynewt.h"

#if MYNEWT_VAL(STATS_NEWTMGR)

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "stats/stats.h"

/* Source code is only included if the newtmgr library is enabled.  Otherwise
 * this file is compiled out for code size.
 */
static int stats_nmgr_read(struct mgmt_cbuf *cb);
static int stats_nmgr_list(struct mgmt_cbuf *cb);

static struct mgmt_group shell_nmgr_group;

#define STATS_NMGR_ID_READ  (0)
#define STATS_NMGR_ID_LIST  (1)

/* ORDER MATTERS HERE.
 * Each element represents the command ID, referenced from newtmgr.
 */
static struct mgmt_handler shell_nmgr_group_handlers[] = {
    [STATS_NMGR_ID_READ] = {stats_nmgr_read, stats_nmgr_read},
    [STATS_NMGR_ID_LIST] = {stats_nmgr_list, stats_nmgr_list}
};

static int
stats_nmgr_walk_func(struct stats_hdr *hdr, void *arg, char *sname,
        uint16_t stat_off)
{
    void *stat_val;
    CborEncoder *penc = (CborEncoder *) arg;
    CborError g_err = CborNoError;

    stat_val = (uint8_t *)hdr + stat_off;

    g_err |= cbor_encode_text_stringz(penc, sname);

    switch (hdr->s_size) {
        case sizeof(uint16_t):
            g_err |= cbor_encode_uint(penc, *(uint16_t *) stat_val);
            break;
        case sizeof(uint32_t):
            g_err |= cbor_encode_uint(penc, *(uint32_t *) stat_val);
            break;
        case sizeof(uint64_t):
            g_err |= cbor_encode_uint(penc, *(uint64_t *) stat_val);
            break;
    }

    return (g_err);
}

static int
stats_nmgr_encode_name(struct stats_hdr *hdr, void *arg)
{
    CborEncoder *penc = (CborEncoder *) arg;

    return cbor_encode_text_stringz(penc, hdr->s_name);
}

static int
stats_nmgr_read(struct mgmt_cbuf *cb)
{
    struct stats_hdr *hdr;
#define STATS_NMGR_NAME_LEN (32)
    char stats_name[STATS_NMGR_NAME_LEN];
    struct cbor_attr_t attrs[] = {
        { "name", CborAttrTextStringType, .addr.string = &stats_name[0],
            .len = sizeof(stats_name) },
        { NULL },
    };
    CborError g_err = CborNoError;
    CborEncoder stats;

    g_err = cbor_read_object(&cb->it, attrs);
    if (g_err != 0) {
        return MGMT_ERR_EINVAL;
    }

    hdr = stats_group_find(stats_name);
    if (!hdr) {
        return MGMT_ERR_EINVAL;
    }

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "name");
    g_err |= cbor_encode_text_stringz(&cb->encoder, stats_name);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "group");
    g_err |= cbor_encode_text_string(&cb->encoder, "sys", sizeof("sys")-1);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "fields");

    g_err |= cbor_encoder_create_map(&cb->encoder, &stats,
                                     CborIndefiniteLength);

    stats_walk(hdr, stats_nmgr_walk_func, &stats);

    g_err |= cbor_encoder_close_container(&cb->encoder, &stats);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }

    return (0);
}

static int
stats_nmgr_list(struct mgmt_cbuf *cb)
{
    CborError g_err = CborNoError;
    CborEncoder stats;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "stat_list");
    g_err |= cbor_encoder_create_array(&cb->encoder, &stats,
                                       CborIndefiniteLength);
    stats_group_walk(stats_nmgr_encode_name, &stats);
    g_err |= cbor_encoder_close_container(&cb->encoder, &stats);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

/**
 * Register nmgr group handlers
 */
int
stats_nmgr_register_group(void)
{
    int rc;

    MGMT_GROUP_SET_HANDLERS(&shell_nmgr_group, shell_nmgr_group_handlers);
    shell_nmgr_group.mg_group_id = MGMT_GROUP_ID_STATS;

    rc = mgmt_group_register(&shell_nmgr_group);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

#endif /* MYNEWT_VAL(STATS_NEWTMGR) */
