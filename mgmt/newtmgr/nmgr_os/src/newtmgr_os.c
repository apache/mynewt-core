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

#include <os/os.h>
#include <os/endian.h>

#include <assert.h>
#include <string.h>

#include <hal/hal_system.h>

#include <mgmt/mgmt.h>

#include <console/console.h>
#include <datetime/datetime.h>
#include <reboot/log_reboot.h>

#include "nmgr_os/nmgr_os.h"

#include <tinycbor/cbor.h>
#include <cborattr/cborattr.h>

static struct os_callout nmgr_reset_callout;

static int nmgr_def_echo(struct mgmt_cbuf *);
static int nmgr_def_console_echo(struct mgmt_cbuf *);
static int nmgr_def_taskstat_read(struct mgmt_cbuf *njb);
static int nmgr_def_mpstat_read(struct mgmt_cbuf *njb);
static int nmgr_datetime_get(struct mgmt_cbuf *njb);
static int nmgr_datetime_set(struct mgmt_cbuf *njb);
static int nmgr_reset(struct mgmt_cbuf *njb);

static const struct mgmt_handler nmgr_def_group_handlers[] = {
    [NMGR_ID_ECHO] = {
        nmgr_def_echo, nmgr_def_echo
    },
    [NMGR_ID_CONS_ECHO_CTRL] = {
        nmgr_def_console_echo, nmgr_def_console_echo
    },
    [NMGR_ID_TASKSTATS] = {
        nmgr_def_taskstat_read, NULL
    },
    [NMGR_ID_MPSTATS] = {
        nmgr_def_mpstat_read, NULL
    },
    [NMGR_ID_DATETIME_STR] = {
        nmgr_datetime_get, nmgr_datetime_set
    },
    [NMGR_ID_RESET] = {
        NULL, nmgr_reset
    },
};

#define NMGR_DEF_GROUP_SZ                                               \
    (sizeof(nmgr_def_group_handlers) / sizeof(nmgr_def_group_handlers[0]))

static struct mgmt_group nmgr_def_group = {
    .mg_handlers = (struct mgmt_handler *)nmgr_def_group_handlers,
    .mg_handlers_count = NMGR_DEF_GROUP_SZ,
    .mg_group_id = MGMT_GROUP_ID_DEFAULT
};

static int
nmgr_def_echo(struct mgmt_cbuf *cb)
{
    char echo_buf[128] = {'\0'};
    CborEncoder *penc = &cb->encoder;
    CborError g_err = CborNoError;
    CborEncoder rsp;
    struct cbor_attr_t attrs[2] = {
        [0] = {
            .attribute = "d",
            .type = CborAttrTextStringType,
            .addr.string = echo_buf,
            .nodefault = 1,
            .len = sizeof(echo_buf),
        },
        [1] = {
            .attribute = NULL
        }
    };

    g_err |= cbor_encoder_create_map(penc, &rsp, CborIndefiniteLength);
    g_err |= cbor_encode_text_stringz(&rsp, "r");
    g_err |= cbor_read_object(&cb->it, attrs);
    g_err |= cbor_encode_text_string(&rsp, echo_buf, strlen(echo_buf));
    g_err |= cbor_encoder_close_container(penc, &rsp);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

static int
nmgr_def_console_echo(struct mgmt_cbuf *cb)
{
    long long int echo_on = 1;
    int rc;
    struct cbor_attr_t attrs[2] = {
        [0] = {
            .attribute = "echo",
            .type = CborAttrIntegerType,
            .addr.integer = &echo_on,
            .nodefault = 1
        },
        [1] = { 0 },
    };

    rc = cbor_read_object(&cb->it, attrs);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    if (echo_on) {
        console_echo(1);
    } else {
        console_echo(0);
    }
    return (0);
}

static int
nmgr_def_taskstat_read(struct mgmt_cbuf *cb)
{
    struct os_task *prev_task;
    struct os_task_info oti;

    CborError g_err = CborNoError;
    CborEncoder rsp, tasks, task;

    g_err |= cbor_encoder_create_map(&cb->encoder, &rsp, CborIndefiniteLength);
    g_err |= cbor_encode_text_stringz(&rsp, "rc");
    g_err |= cbor_encode_int(&rsp, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&rsp, "tasks");
    g_err |= cbor_encoder_create_map(&rsp, &tasks, CborIndefiniteLength);

    prev_task = NULL;
    while (1) {

        prev_task = os_task_info_get_next(prev_task, &oti);
        if (prev_task == NULL) {
            break;
        }

        g_err |= cbor_encode_text_stringz(&tasks, oti.oti_name);
        g_err |= cbor_encoder_create_map(&tasks, &task, CborIndefiniteLength);
        g_err |= cbor_encode_text_stringz(&rsp, "prio");
        g_err |= cbor_encode_uint(&rsp, oti.oti_prio);
        g_err |= cbor_encode_text_stringz(&rsp, "tid");
        g_err |= cbor_encode_uint(&rsp, oti.oti_taskid);
        g_err |= cbor_encode_text_stringz(&rsp, "state");
        g_err |= cbor_encode_uint(&rsp, oti.oti_state);
        g_err |= cbor_encode_text_stringz(&rsp, "stkuse");
        g_err |= cbor_encode_uint(&rsp, oti.oti_stkusage);
        g_err |= cbor_encode_text_stringz(&rsp, "stksiz");
        g_err |= cbor_encode_uint(&rsp, oti.oti_stksize);
        g_err |= cbor_encode_text_stringz(&rsp, "cswcnt");
        g_err |= cbor_encode_uint(&rsp, oti.oti_cswcnt);
        g_err |= cbor_encode_text_stringz(&rsp, "runtime");
        g_err |= cbor_encode_uint(&rsp, oti.oti_runtime);
        g_err |= cbor_encode_text_stringz(&rsp, "last_checkin");
        g_err |= cbor_encode_uint(&rsp, oti.oti_last_checkin);
        g_err |= cbor_encode_text_stringz(&rsp, "next_checkin");
        g_err |= cbor_encode_uint(&rsp, oti.oti_next_checkin);
        g_err |= cbor_encoder_close_container(&tasks, &task);
    }
    g_err |= cbor_encoder_close_container(&rsp, &tasks);
    g_err |= cbor_encoder_close_container(&cb->encoder, &rsp);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

static int
nmgr_def_mpstat_read(struct mgmt_cbuf *cb)
{
    struct os_mempool *prev_mp;
    struct os_mempool_info omi;
    CborError g_err = CborNoError;
    CborEncoder rsp, pools, pool;

    g_err |= cbor_encoder_create_map(&cb->encoder, &rsp, CborIndefiniteLength);
    g_err |= cbor_encode_text_stringz(&rsp, "rc");
    g_err |= cbor_encode_int(&rsp, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&rsp, "mpools");
    g_err |= cbor_encoder_create_map(&rsp, &pools, CborIndefiniteLength);

    prev_mp = NULL;
    while (1) {
        prev_mp = os_mempool_info_get_next(prev_mp, &omi);
        if (prev_mp == NULL) {
            break;
        }

        g_err |= cbor_encode_text_stringz(&pools, omi.omi_name);
        g_err |= cbor_encoder_create_map(&pools, &pool, CborIndefiniteLength);
        g_err |= cbor_encode_text_stringz(&rsp, "blksiz");
        g_err |= cbor_encode_uint(&rsp, omi.omi_block_size);
        g_err |= cbor_encode_text_stringz(&rsp, "nblks");
        g_err |= cbor_encode_uint(&rsp, omi.omi_num_blocks);
        g_err |= cbor_encode_text_stringz(&rsp, "nfree");
        g_err |= cbor_encode_uint(&rsp, omi.omi_num_free);
        g_err |= cbor_encoder_close_container(&pools, &pool);
    }

    g_err |= cbor_encoder_close_container(&rsp, &pools);
    g_err |= cbor_encoder_close_container(&cb->encoder, &rsp);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

static int
nmgr_datetime_get(struct mgmt_cbuf *cb)
{
    struct os_timeval tv;
    struct os_timezone tz;
    char buf[DATETIME_BUFSIZE];
    int rc;
    CborError g_err = CborNoError;
    CborEncoder rsp;

    g_err |= cbor_encoder_create_map(&cb->encoder, &rsp, CborIndefiniteLength);
    g_err |= cbor_encode_text_stringz(&rsp, "rc");
    g_err |= cbor_encode_int(&rsp, MGMT_ERR_EOK);

    /* Display the current datetime */
    rc = os_gettimeofday(&tv, &tz);
    assert(rc == 0);
    rc = datetime_format(&tv, &tz, buf, DATETIME_BUFSIZE);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }
    g_err |= cbor_encode_text_stringz(&rsp, "datetime");
    g_err |= cbor_encode_text_stringz(&rsp, buf);
    g_err |= cbor_encoder_close_container(&cb->encoder, &rsp);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;

err:
    return (rc);
}

static int
nmgr_datetime_set(struct mgmt_cbuf *cb)
{
    struct os_timeval tv;
    struct os_timezone tz;
    char buf[DATETIME_BUFSIZE];
    int rc = 0;
    const struct cbor_attr_t datetime_write_attr[3] = {
        [0] = {
            .attribute = "datetime",
            .type = CborAttrTextStringType,
            .addr.string = buf,
            .len = sizeof(buf),
        },
        [1] = {
            .attribute = "rc",
            .type = CborAttrIntegerType,
        },
        { 0 },
    };

    rc = cbor_read_object(&cb->it, datetime_write_attr);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto out;
    }

    /* Set the current datetime */
    rc = datetime_parse(buf, &tv, &tz);
    if (!rc) {
        rc = os_settimeofday(&tv, &tz);
        if (rc) {
          rc = MGMT_ERR_EINVAL;
          goto out;
        }
    } else {
        rc = MGMT_ERR_EINVAL;
        goto out;
    }

    rc = 0;
out:
    mgmt_cbuf_setoerr(cb, rc);
    return rc;
}

static void
nmgr_reset_tmo(struct os_event *ev)
{
    hal_system_reset();
}

static int
nmgr_reset(struct mgmt_cbuf *cb)
{
    os_callout_init(&nmgr_reset_callout, mgmt_evq_get(), nmgr_reset_tmo, NULL);

    log_reboot(HAL_RESET_SOFT);
    os_callout_reset(&nmgr_reset_callout, OS_TICKS_PER_SEC / 4);

    mgmt_cbuf_setoerr(cb, OS_OK);

    return 0;
}

int
nmgr_os_groups_register(void)
{
    return mgmt_group_register(&nmgr_def_group);
}

