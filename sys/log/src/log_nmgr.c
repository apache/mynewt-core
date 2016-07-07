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
#include "log/log.h"

/* Source code is only included if the newtmgr library is enabled.  Otherwise
 * this file is compiled out for code size.
 */

static int log_nmgr_read(struct nmgr_jbuf *njb);
static int log_nmgr_clear(struct nmgr_jbuf *njb);
static struct nmgr_group log_nmgr_group;

#define LOGS_NMGR_ID_READ  (0)
#define LOGS_NMGR_OP_CLEAR (1)

/* ORDER MATTERS HERE.
 * Each element represents the command ID, referenced from newtmgr.
 */
static struct nmgr_handler log_nmgr_group_handlers[] = {
    [LOGS_NMGR_ID_READ] = {log_nmgr_read, log_nmgr_read},
    [LOGS_NMGR_OP_CLEAR] = {log_nmgr_clear, log_nmgr_clear}
};

static int
log_nmgr_add_entry(struct log *log, void *arg, void *dptr, uint16_t len)
{
    struct log_entry_hdr ueh;
    char data[128];
    int dlen;
    struct json_encoder *encoder;
    struct json_value jv;
    int rc;

    rc = log_read(log, dptr, &ueh, 0, sizeof(ueh));
    if (rc != sizeof(ueh)) {
        goto err;
    }

    dlen = min(len-sizeof(ueh), 128);

    rc = log_read(log, dptr, data, sizeof(ueh), dlen);
    if (rc < 0) {
        goto err;
    }
    data[rc] = 0;

    encoder = (struct json_encoder *) arg;

    json_encode_object_start(encoder);

    JSON_VALUE_STRINGN(&jv, data, rc);
    rc = json_encode_object_entry(encoder, "msg", &jv);
    if (rc != 0) {
        goto err;
    }

    JSON_VALUE_INT(&jv, ueh.ue_ts);
    rc = json_encode_object_entry(encoder, "ts", &jv);
    if (rc != 0) {
        goto err;
    }

    JSON_VALUE_UINT(&jv, ueh.ue_level);
    rc = json_encode_object_entry(encoder, "level", &jv);
    if (rc != 0) {
        goto err;
    }

    JSON_VALUE_UINT(&jv, ueh.ue_index);
    rc = json_encode_object_entry(encoder, "index", &jv);
    if (rc != 0) {
        goto err;
    }

    json_encode_object_finish(encoder);

    return (0);
err:
    return (rc);
}

static int
log_nmgr_read(struct nmgr_jbuf *njb)
{
    struct log *log;
    int rc;
    struct json_value jv;
    struct json_encoder *encoder;

    encoder = (struct json_encoder *) &nmgr_task_jbuf.njb_enc;

    json_encode_object_start(encoder);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(encoder, "rc", &jv);
    json_encode_array_name(encoder, "logs");
    json_encode_array_start(encoder);

    log = NULL;
    while (1) {
        log = log_list_get_next(log);
        if (log == NULL) {
            break;
        }

        if (log->l_log->log_type == LOG_TYPE_STREAM) {
            continue;
        }

        json_encode_object_start(encoder);
        JSON_VALUE_STRING(&jv, log->l_name);
        json_encode_object_entry(encoder, "name", &jv);

        JSON_VALUE_INT(&jv, log->l_log->log_type);
        json_encode_object_entry(encoder, "type", &jv);

        json_encode_array_name(encoder, "entries");
        json_encode_array_start(encoder);

        rc = log_walk(log, log_nmgr_add_entry, encoder);
        if (rc != 0) {
            goto err;
        }

        json_encode_array_finish(encoder);
        json_encode_object_finish(encoder);
    }

    json_encode_array_finish(encoder);
    json_encode_object_finish(encoder);

    return (0);
err:
    nmgr_jbuf_setoerr(njb, rc);
    return (0);
}

static int
log_nmgr_clear(struct nmgr_jbuf *njb)
{
    struct log *log;
    int rc;
    struct json_encoder *encoder;

    log = NULL;
    while (1) {
        log = log_list_get_next(log);
        if (log == NULL) {
            break;
        }

        if (log->l_log->log_type == LOG_TYPE_STREAM) {
            continue;
        }

        rc = log_flush(log);
        if (rc != 0) {
            goto err;
        }
    }

    encoder = (struct json_encoder *) &nmgr_task_jbuf.njb_enc;

    json_encode_object_start(encoder);
    json_encode_object_finish(encoder);

    return 0;
err:
    nmgr_jbuf_setoerr(njb, rc);
    return (rc);
}

/**
 * Register nmgr group handlers.
 */
int
log_nmgr_register_group(void)
{
    int rc;

    NMGR_GROUP_SET_HANDLERS(&log_nmgr_group, log_nmgr_group_handlers);
    log_nmgr_group.ng_group_id = NMGR_GROUP_ID_LOGS;

    rc = nmgr_group_register(&log_nmgr_group);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


#endif /* NEWTMGR_PRESENT */
