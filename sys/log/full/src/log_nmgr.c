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

#if MYNEWT_VAL(LOG_NEWTMGR)

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "tinycbor/cbor_cnt_writer.h"
#include "log/log.h"

/* Source code is only included if the newtmgr library is enabled.  Otherwise
 * this file is compiled out for code size.
 */

static int log_nmgr_read(struct mgmt_cbuf *njb);
static int log_nmgr_clear(struct mgmt_cbuf *njb);
static int log_nmgr_module_list(struct mgmt_cbuf *njb);
static int log_nmgr_level_list(struct mgmt_cbuf *njb);
static int log_nmgr_logs_list(struct mgmt_cbuf *njb);
static struct mgmt_group log_nmgr_group;


/* ORDER MATTERS HERE.
 * Each element represents the command ID, referenced from newtmgr.
 */
static struct mgmt_handler log_nmgr_group_handlers[] = {
    [LOGS_NMGR_OP_READ] = {log_nmgr_read, log_nmgr_read},
    [LOGS_NMGR_OP_CLEAR] = {log_nmgr_clear, log_nmgr_clear},
    [LOGS_NMGR_OP_MODULE_LIST] = {log_nmgr_module_list, NULL},
    [LOGS_NMGR_OP_LEVEL_LIST] = {log_nmgr_level_list, NULL},
    [LOGS_NMGR_OP_LOGS_LIST] = {log_nmgr_logs_list, NULL}
};

struct log_encode_data {
    uint32_t counter;
    CborEncoder *enc;
};

/**
 * Log encode entry
 * @param log structure, log_offset, dataptr, len
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_encode_entry(struct log *log, struct log_offset *log_offset,
                      void *dptr, uint16_t len)
{
    struct log_entry_hdr ueh;
    uint8_t data[128];
    int rc;
    int rsp_len;
    CborError g_err = CborNoError;
    struct log_encode_data *ed = log_offset->lo_arg;
    CborEncoder rsp;
    struct CborCntWriter cnt_writer;
    CborEncoder cnt_encoder;
#if MYNEWT_VAL(LOG_VERSION) > 2
    CborEncoder str_encoder;
    int off;
#endif

    rc = log_read(log, dptr, &ueh, 0, sizeof(ueh));
    if (rc != sizeof(ueh)) {
        rc = OS_ENOENT;
        goto err;
    }
    rc = OS_OK;

    /* If specified timestamp is nonzero, it is the primary criterion, and the
     * specified index is the secondary criterion.  If specified timetsamp is
     * zero, specified index is the only criterion.
     *
     * If specified timestamp == 0: encode entries whose index >=
     *     specified index.
     * Else: encode entries whose timestamp >= specified timestamp and whose
     *      index >= specified index
     */

    if (log_offset->lo_ts == 0) {
        if (log_offset->lo_index > ueh.ue_index) {
            goto err;
        }
    } else if (ueh.ue_ts < log_offset->lo_ts   ||
               (ueh.ue_ts == log_offset->lo_ts &&
                ueh.ue_index < log_offset->lo_index)) {
        goto err;
    }

#if MYNEWT_VAL(LOG_VERSION) < 3
    rc = log_read(log, dptr, data, sizeof(ueh), min(len - sizeof(ueh), 128));
    if (rc < 0) {
        rc = OS_ENOENT;
        goto err;
    }
    data[rc] = 0;
#endif

    /*calculate whether this would fit */
    /* create a counting encoder for cbor */
    cbor_cnt_writer_init(&cnt_writer);
    cbor_encoder_init(&cnt_encoder, &cnt_writer.enc, 0);

    /* NOTE This code should exactly match what is below */
    rsp_len = log_offset->lo_data_len;
    g_err |= cbor_encoder_create_map(&cnt_encoder, &rsp, CborIndefiniteLength);
#if MYNEWT_VAL(LOG_VERSION) > 2
    switch (ueh.ue_etype) {
    case LOG_ETYPE_CBOR:
        g_err |= cbor_encode_text_stringz(&rsp, "type");
        g_err |= cbor_encode_text_stringz(&rsp, "cbor");
        break;
    case LOG_ETYPE_BINARY:
        g_err |= cbor_encode_text_stringz(&rsp, "type");
        g_err |= cbor_encode_text_stringz(&rsp, "bin");
        break;
    case LOG_ETYPE_STRING:
    default:
        /* no need for type here */
        g_err |= cbor_encode_text_stringz(&rsp, "type");
        g_err |= cbor_encode_text_stringz(&rsp, "str");
        break;
    }

    g_err |= cbor_encode_text_stringz(&rsp, "msg");

    /*
     * Write entry data as byte string. Since this may not fit into single
     * chunk of data we will write as indefinite-length byte string which is
     * basically a indefinite-length container with definite-length strings
     * inside.
     */
    g_err |= cbor_encoder_create_indef_byte_string(&rsp, &str_encoder);
    for (off = sizeof(ueh); (off < len) && !g_err; ) {
        rc = log_read(log, dptr, data, off, sizeof(data));
        if (rc < 0) {
            g_err |= 1;
            break;
        }
        g_err |= cbor_encode_byte_string(&str_encoder, data, rc);
        off += rc;
    }
    g_err |= cbor_encoder_close_container(&rsp, &str_encoder);
#else
    g_err |= cbor_encode_text_stringz(&rsp, "msg");
    g_err |= cbor_encode_text_stringz(&rsp, (char *)data);
#endif
    g_err |= cbor_encode_text_stringz(&rsp, "ts");
    g_err |= cbor_encode_int(&rsp, ueh.ue_ts);
    g_err |= cbor_encode_text_stringz(&rsp, "level");
    g_err |= cbor_encode_uint(&rsp, ueh.ue_level);
    g_err |= cbor_encode_text_stringz(&rsp, "index");
    g_err |= cbor_encode_uint(&rsp,  ueh.ue_index);
    g_err |= cbor_encode_text_stringz(&rsp, "module");
    g_err |= cbor_encode_uint(&rsp,  ueh.ue_module);
    g_err |= cbor_encoder_close_container(&cnt_encoder, &rsp);
    rsp_len += cbor_encode_bytes_written(&cnt_encoder);
    /*
     * Make sure that at least single entry is returned, even in case response
     * exceeds this magic value. This is to make sure we can read long log
     * entries, even if they have to be read one by one.
     */
    if ((rsp_len > 400) && (ed->counter > 0)) {
        rc = OS_ENOMEM;
        goto err;
    }
    log_offset->lo_data_len = rsp_len;

    g_err |= cbor_encoder_create_map(ed->enc, &rsp, CborIndefiniteLength);
#if MYNEWT_VAL(LOG_VERSION) > 2
    switch (ueh.ue_etype) {
    case LOG_ETYPE_CBOR:
        g_err |= cbor_encode_text_stringz(&rsp, "type");
        g_err |= cbor_encode_text_stringz(&rsp, "cbor");
        break;
    case LOG_ETYPE_BINARY:
        g_err |= cbor_encode_text_stringz(&rsp, "type");
        g_err |= cbor_encode_text_stringz(&rsp, "bin");
        break;
    case LOG_ETYPE_STRING:
    default:
        /* no need for type here */
        g_err |= cbor_encode_text_stringz(&rsp, "type");
        g_err |= cbor_encode_text_stringz(&rsp, "str");
        break;
    }

    g_err |= cbor_encode_text_stringz(&rsp, "msg");

    /*
     * Write entry data as byte string. Since this may not fit into single
     * chunk of data we will write as indefinite-length byte string which is
     * basically a indefinite-length container with definite-length strings
     * inside.
     */
    g_err |= cbor_encoder_create_indef_byte_string(&rsp, &str_encoder);
    for (off = sizeof(ueh); (off < len) && !g_err; ) {
        rc = log_read(log, dptr, data, off, sizeof(data));
        if (rc < 0) {
            g_err |= 1;
            break;
        }
        g_err |= cbor_encode_byte_string(&str_encoder, data, rc);
        off += rc;
    }
    g_err |= cbor_encoder_close_container(&rsp, &str_encoder);
#else
    g_err |= cbor_encode_text_stringz(&rsp, "msg");
    g_err |= cbor_encode_text_stringz(&rsp, (char *)data);
#endif
    g_err |= cbor_encode_text_stringz(&rsp, "ts");
    g_err |= cbor_encode_int(&rsp, ueh.ue_ts);
    g_err |= cbor_encode_text_stringz(&rsp, "level");
    g_err |= cbor_encode_uint(&rsp, ueh.ue_level);
    g_err |= cbor_encode_text_stringz(&rsp, "index");
    g_err |= cbor_encode_uint(&rsp,  ueh.ue_index);
    g_err |= cbor_encode_text_stringz(&rsp, "module");
    g_err |= cbor_encode_uint(&rsp,  ueh.ue_module);
    g_err |= cbor_encoder_close_container(ed->enc, &rsp);

    ed->counter++;

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
err:
    ed->counter++;
    return (rc);
}

/**
 * Log encode entries
 * @param log structure, the encoder, timestamp, index
 * @return 0 on success; non-zero on failure
 */
static int
log_encode_entries(struct log *log, CborEncoder *cb,
                   int64_t ts, uint32_t index)
{
    int rc;
    struct log_offset log_offset;
    int rsp_len = 0;
    CborEncoder entries;
    CborError g_err = CborNoError;
    struct CborCntWriter cnt_writer;
    CborEncoder cnt_encoder;
    struct log_encode_data ed;

    memset(&log_offset, 0, sizeof(log_offset));

    /* this code counts how long the message would be if we encoded
     * this outer structure using cbor. */
    cbor_cnt_writer_init(&cnt_writer);
    cbor_encoder_init(&cnt_encoder, &cnt_writer.enc, 0);
    g_err |= cbor_encode_text_stringz(&cnt_encoder, "entries");
    g_err |= cbor_encoder_create_array(&cnt_encoder, &entries,
                                       CborIndefiniteLength);
    g_err |= cbor_encoder_close_container(&cnt_encoder, &entries);
    rsp_len = cbor_encode_bytes_written(cb) +
              cbor_encode_bytes_written(&cnt_encoder);
    if (rsp_len > 400) {
        rc = OS_ENOMEM;
        goto err;
    }

    g_err |= cbor_encode_text_stringz(cb, "entries");
    g_err |= cbor_encoder_create_array(cb, &entries, CborIndefiniteLength);

    ed.counter = 0;
    ed.enc = &entries;

    log_offset.lo_arg       = &ed;
    log_offset.lo_index     = index;
    log_offset.lo_ts        = ts;
    log_offset.lo_data_len  = rsp_len;

    rc = log_walk(log, log_nmgr_encode_entry, &log_offset);

    g_err |= cbor_encoder_close_container(cb, &entries);

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
log_encode(struct log *log, CborEncoder *cb,
            int64_t ts, uint32_t index)
{
    int rc;
    CborEncoder logs;
    CborError g_err = CborNoError;

    g_err |= cbor_encoder_create_map(cb, &logs, CborIndefiniteLength);
    g_err |= cbor_encode_text_stringz(&logs, "name");
    g_err |= cbor_encode_text_stringz(&logs, log->l_name);

    g_err |= cbor_encode_text_stringz(&logs, "type");
    g_err |= cbor_encode_uint(&logs, log->l_log->log_type);

    rc = log_encode_entries(log, &logs, ts, index);
    g_err |= cbor_encoder_close_container(cb, &logs);
    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return rc;
}

/**
 * Newtmgr Log read handler
 * @param cbor buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_read(struct mgmt_cbuf *cb)
{
    struct log *log;
    int rc;
    char name[LOG_NAME_MAX_LEN] = {0};
    int name_len;
    int64_t ts;
    uint64_t index;
    CborError g_err = CborNoError;
    CborEncoder logs;

    const struct cbor_attr_t attr[4] = {
        [0] = {
            .attribute = "log_name",
            .type = CborAttrTextStringType,
            .addr.string = name,
            .len = sizeof(name)
        },
        [1] = {
            .attribute = "ts",
            .type = CborAttrIntegerType,
            .addr.integer = &ts
        },
        [2] = {
            .attribute = "index",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &index
        },
        [3] = {
            .attribute = NULL
        }
    };

    rc = cbor_read_object(&cb->it, attr);
    if (rc) {
        return rc;
    }

    g_err |= cbor_encode_text_stringz(&cb->encoder, "next_index");
    g_err |= cbor_encode_int(&cb->encoder, g_log_info.li_next_index);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "logs");
    g_err |= cbor_encoder_create_array(&cb->encoder, &logs,
                                       CborIndefiniteLength);

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

        rc = log_encode(log, &logs, ts, index);
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
    g_err |= cbor_encoder_close_container(&cb->encoder, &logs);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, rc);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    rc = 0;
    return (rc);
}

/**
 * Newtmgr Module list handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_module_list(struct mgmt_cbuf *cb)
{
    int module;
    const char *str;
    CborError g_err = CborNoError;
    CborEncoder modules;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "module_map");
    g_err |= cbor_encoder_create_map(&cb->encoder, &modules,
                                     CborIndefiniteLength);

    module = LOG_MODULE_DEFAULT;
    while (module < LOG_MODULE_MAX) {
        str = LOG_MODULE_STR(module);
        if (!str) {
            module++;
            continue;
        }

        g_err |= cbor_encode_text_stringz(&modules, str);
        g_err |= cbor_encode_uint(&modules, module);
        module++;
    }

    g_err |= cbor_encoder_close_container(&cb->encoder, &modules);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

/**
 * Newtmgr Log list handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_logs_list(struct mgmt_cbuf *cb)
{
    CborError g_err = CborNoError;
    CborEncoder log_list;
    struct log *log;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "log_list");
    g_err |= cbor_encoder_create_array(&cb->encoder, &log_list,
                                       CborIndefiniteLength);

    log = NULL;
    while (1) {
        log = log_list_get_next(log);
        if (!log) {
            break;
        }

        if (log->l_log->log_type == LOG_TYPE_STREAM) {
            continue;
        }

        g_err |= cbor_encode_text_stringz(&log_list, log->l_name);
    }

    g_err |= cbor_encoder_close_container(&cb->encoder, &log_list);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

/**
 * Newtmgr Log Level list handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_level_list(struct mgmt_cbuf *cb)
{
    CborError g_err = CborNoError;
    CborEncoder level_map;
    int level;
    char *str;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "level_map");
    g_err |= cbor_encoder_create_map(&cb->encoder, &level_map,
                                     CborIndefiniteLength);

    level = LOG_LEVEL_DEBUG;
    while (level < LOG_LEVEL_MAX) {
        str = LOG_LEVEL_STR(level);
        if (!strcmp(str, "UNKNOWN")) {
            level++;
            continue;
        }

        g_err |= cbor_encode_text_stringz(&level_map, str);
        g_err |= cbor_encode_uint(&level_map, level);
        level++;
    }

    g_err |= cbor_encoder_close_container(&cb->encoder, &level_map);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

/**
 * Newtmgr log clear handler
 * @param nmgr json buffer
 * @return 0 on success; non-zero on failure
 */
static int
log_nmgr_clear(struct mgmt_cbuf *cb)
{
    struct log *log;
    int rc;

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
            return rc;
        }
    }

    rc = mgmt_cbuf_setoerr(cb, 0);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Register nmgr group handlers.
 * @return 0 on success; non-zero on failure
 */
int
log_nmgr_register_group(void)
{
    int rc;

    MGMT_GROUP_SET_HANDLERS(&log_nmgr_group, log_nmgr_group_handlers);
    log_nmgr_group.mg_group_id = MGMT_GROUP_ID_LOGS;

    rc = mgmt_group_register(&log_nmgr_group);
    if (rc) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

#endif
