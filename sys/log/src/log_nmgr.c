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
static int log_nmgr_module_list(struct nmgr_jbuf *njb);
static int log_nmgr_level_list(struct nmgr_jbuf *njb);
static int log_nmgr_logs_list(struct nmgr_jbuf *njb);
static struct nmgr_group log_nmgr_group;


/* Newtmgr Log opcodes */
#define LOGS_NMGR_OP_READ         (0)
#define LOGS_NMGR_OP_CLEAR        (1)
#define LOGS_NMGR_OP_APPEND       (2)
#define LOGS_NMGR_OP_MODULE_LIST  (3)
#define LOGS_NMGR_OP_LEVEL_LIST   (4)
#define LOGS_NMGR_OP_LOGS_LIST    (5)

/* ORDER MATTERS HERE.
 * Each element represents the command ID, referenced from newtmgr.
 */
static struct nmgr_handler log_nmgr_group_handlers[] = {
    [LOGS_NMGR_OP_READ] = {log_nmgr_read, log_nmgr_read},
    [LOGS_NMGR_OP_CLEAR] = {log_nmgr_clear, log_nmgr_clear},
    [LOGS_NMGR_OP_MODULE_LIST] = {log_nmgr_module_list, NULL},
    [LOGS_NMGR_OP_LEVEL_LIST] = {log_nmgr_level_list, NULL},
    [LOGS_NMGR_OP_LOGS_LIST] = {log_nmgr_logs_list, NULL}
};

struct encode_off {
    struct json_encoder *eo_encoder;
    int64_t eo_ts;
    uint8_t eo_index;
    uint32_t rsp_len;
};

/**
 * Log encode entry
 * @param log structure, arg:struct passed locally, dataptr, len
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_encode_entry(struct log *log, void *arg, void *dptr, uint16_t len)
{
    struct encode_off *encode_off = (struct encode_off *)arg;
    struct log_entry_hdr ueh;
    char data[128];
    int dlen;
    struct json_value jv;
    int rc;
    int rsp_len;

    rc = log_read(log, dptr, &ueh, 0, sizeof(ueh));
    if (rc != sizeof(ueh)) {
        rc = OS_ENOENT;
        goto err;
    }
    rc = OS_OK;

    /* Matching timestamps and indices for sending a log entry */
    if (ueh.ue_ts < encode_off->eo_ts   ||
        (ueh.ue_ts == encode_off->eo_ts &&
         ueh.ue_index <= encode_off->eo_index)) {
        goto err;
    }

    dlen = min(len-sizeof(ueh), 128);

    rc = log_read(log, dptr, data, sizeof(ueh), dlen);
    if (rc < 0) {
        rc = OS_ENOENT;
        goto err;
    }
    data[rc] = 0;

    rsp_len = encode_off->rsp_len;
    /* Calculating entry len */
    rsp_len += strlen(data);

    /* Pre calculating MAX length of the json string */
    rsp_len += (sizeof(STR(INT64_MAX))  + sizeof("{,ts:")    +
                sizeof(STR(UINT8_MAX))  + sizeof(",level:")  +
                sizeof(STR(UINT32_MAX)) + sizeof(",index:")  +
                sizeof(STR(UINT16_MAX)) + sizeof(",module:}"));

    if (rsp_len > NMGR_MAX_MTU) {
        rc = OS_ENOMEM;
        goto err;
    }

    json_encode_object_start(encode_off->eo_encoder);

    JSON_VALUE_STRINGN(&jv, data, rc);
    rc = json_encode_object_entry(encode_off->eo_encoder, "msg", &jv);
    if (rc) {
        goto err;
    }

    JSON_VALUE_INT(&jv, ueh.ue_ts);
    rc = json_encode_object_entry(encode_off->eo_encoder, "ts", &jv);
    if (rc) {
        goto err;
    }

    JSON_VALUE_UINT(&jv, ueh.ue_level);
    rc = json_encode_object_entry(encode_off->eo_encoder, "level", &jv);
    if (rc) {
        goto err;
    }

    JSON_VALUE_UINT(&jv, ueh.ue_index);
    rc = json_encode_object_entry(encode_off->eo_encoder, "index", &jv);
    if (rc) {
        goto err;
    }

    JSON_VALUE_UINT(&jv, ueh.ue_module);
    rc = json_encode_object_entry(encode_off->eo_encoder, "module", &jv);
    if (rc) {
        goto err;
    }

    json_encode_object_finish(encode_off->eo_encoder);
    encode_off->rsp_len += rsp_len;

    return (0);
err:
    return (rc);
}

/**
 * Log encode entries
 * @param log structure, the encoder, timestamp, index
 * @return 0 on success; non-zero on failure
 */
static int
log_encode_entries(struct log *log, struct json_encoder *encoder,
                   int64_t ts, uint32_t index)
{
    int rc;
    struct encode_off encode_off;
    int rsp_len;

    memset(&encode_off, 0, sizeof(encode_off));
    /* Already encoded json string */
    rsp_len = strlen(encoder->je_encode_buf);
    /* Pre calculating json length */
    rsp_len += (sizeof("entries") + 3);
    rsp_len += encode_off.rsp_len;

    if (rsp_len > NMGR_MAX_MTU) {
        rc = OS_ENOMEM;
        goto err;
    }

    json_encode_array_name(encoder, "entries");
    json_encode_array_start(encoder);

    encode_off.eo_encoder  = encoder;
    encode_off.eo_index    = index;
    encode_off.eo_ts       = ts;

    encode_off.rsp_len = rsp_len;

    rc = log_walk(log, log_nmgr_encode_entry, &encode_off);
    json_encode_array_finish(encoder);

err:
    return rc;
}

/**
 * Log encode function
 * @param log structure, the encoder, json_value,
 *        timestamp, index
 * @return 0 on success; non-zero on failure
 */
static int
log_encode(struct log *log, struct json_encoder *encoder,
           struct json_value *jv, int64_t ts, uint32_t index)
{
    int rc;

    json_encode_object_start(encoder);
    JSON_VALUE_STRING(jv, log->l_name);
    json_encode_object_entry(encoder, "name", jv);

    JSON_VALUE_UINT(jv, log->l_log->log_type);
    json_encode_object_entry(encoder, "type", jv);

    rc = log_encode_entries(log, encoder, ts, index);
    json_encode_object_finish(encoder);

    return rc;
}

/**
 * Newtmgr Log read handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_read(struct nmgr_jbuf *njb)
{
    struct log *log;
    int rc;
    struct json_value jv;
    struct json_encoder *encoder;
    char name[LOG_NAME_MAX_LEN] = {0};
    int name_len;
    int64_t ts;
    uint64_t index;

    const struct json_attr_t attr[4] = {
        [0] = {
            .attribute = "log_name",
            .type = t_string,
            .addr.string = name,
            .len = sizeof(name)
        },
        [1] = {
            .attribute = "ts",
            .type = t_integer,
            .addr.integer = &ts
        },
        [2] = {
            .attribute = "index",
            .type = t_uinteger,
            .addr.uinteger = &index
        },
        [3] = {
            .attribute = NULL
        }
    };

    rc = json_read_object(&njb->njb_buf, attr);
    if (rc) {
        return rc;
    }

    encoder = (struct json_encoder *) &nmgr_task_jbuf.njb_enc;

    json_encode_object_start(encoder);
    json_encode_array_name(encoder, "logs");
    json_encode_array_start(encoder);

    name_len = strlen(name);
    log = NULL;
    while (1) {
        log = log_list_get_next(log);
        if (!log) {
            break;
        }

        if (log->l_log->log_type == LOG_TYPE_STREAM) {
            continue;
        }

        /* Conditions for returning specific logs */
        if ((name_len > 0) && strcmp(name, log->l_name)) {
            continue;
        }

        rc = log_encode(log, encoder, &jv, ts, index);
        if (rc) {
            goto err;
        }

        /* If a log was found, encode and break */
        if (name_len > 0) {
            break;
        }
    }


    /* Running out of logs list and we have a specific log to look for */
    if (!log && name_len > 0) {
        rc = OS_EINVAL;
    }

err:
    json_encode_array_finish(encoder);
    JSON_VALUE_INT(&jv, rc);
    json_encode_object_entry(encoder, "rc", &jv);
    json_encode_object_finish(encoder);
    rc = 0;
    return (rc);
}

/**
 * Newtmgr Module list handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_module_list(struct nmgr_jbuf *njb)
{
    struct json_value jv;
    struct json_encoder *encoder;
    int module;
    char *str;

    encoder = (struct json_encoder *) &nmgr_task_jbuf.njb_enc;

    json_encode_object_start(encoder);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(encoder, "rc", &jv);
    json_encode_object_key(encoder, "module_map");
    json_encode_object_start(encoder);

    module = LOG_MODULE_DEFAULT;
    while (module < LOG_MODULE_MAX) {
        str = LOG_MODULE_STR(module);
        if (!strcmp(str, "UNKNOWN")) {
            module++;
            continue;
        }

        JSON_VALUE_UINT(&jv, module);
        json_encode_object_entry(encoder, str, &jv);

        module++;
    }


    json_encode_object_finish(encoder);
    json_encode_object_finish(encoder);

    return (0);
}

/**
 * Newtmgr Log list handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_logs_list(struct nmgr_jbuf *njb)
{
    struct json_value jv;
    struct json_encoder *encoder;
    struct log *log;

    encoder = (struct json_encoder *) &nmgr_task_jbuf.njb_enc;

    json_encode_object_start(encoder);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(encoder, "rc", &jv);
    json_encode_array_name(encoder, "log_list");
    json_encode_array_start(encoder);

    log = NULL;
    while (1) {
        log = log_list_get_next(log);
        if (!log) {
            break;
        }

        if (log->l_log->log_type == LOG_TYPE_STREAM) {
            continue;
        }

        JSON_VALUE_STRING(&jv, log->l_name);
        json_encode_array_value(encoder, &jv);
    }

    json_encode_array_finish(encoder);
    json_encode_object_finish(encoder);

    return (0);
}

/**
 * Newtmgr Log Level list handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_level_list(struct nmgr_jbuf *njb)
{
    struct json_value jv;
    struct json_encoder *encoder;
    int level;
    char *str;

    encoder = (struct json_encoder *) &nmgr_task_jbuf.njb_enc;

    json_encode_object_start(encoder);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(encoder, "rc", &jv);
    json_encode_object_key(encoder, "level_map");
    json_encode_object_start(encoder);

    level = LOG_LEVEL_DEBUG;
    while (level < LOG_LEVEL_MAX) {
        str = LOG_LEVEL_STR(level);
        if (!strcmp(str, "UNKNOWN")) {
            level++;
            continue;
        }

        JSON_VALUE_UINT(&jv, level);
        json_encode_object_entry(encoder, str, &jv);

        level++;
    }

    json_encode_object_finish(encoder);
    json_encode_object_finish(encoder);

    return (0);
}

/**
 * Newtmgr log clear handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
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
        if (rc) {
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
 * @return 0 on success; non-zero on failure
 */
int
log_nmgr_register_group(void)
{
    int rc;

    NMGR_GROUP_SET_HANDLERS(&log_nmgr_group, log_nmgr_group_handlers);
    log_nmgr_group.ng_group_id = NMGR_GROUP_ID_LOGS;

    rc = nmgr_group_register(&log_nmgr_group);
    if (rc) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

#endif /* NEWTMGR_PRESENT */
