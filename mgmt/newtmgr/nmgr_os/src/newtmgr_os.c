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

#include <assert.h>
#include <string.h>

#include "os/mynewt.h"

#include <hal/hal_system.h>
#include <hal/hal_watchdog.h>

#include <mgmt/mgmt.h>

#include <console/console.h>
#include <datetime/datetime.h>

#if MYNEWT_VAL(LOG_SOFT_RESET)
#include <reboot/log_reboot.h>
#endif

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
    CborError g_err = CborNoError;

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

    g_err |= cbor_encode_text_stringz(&cb->encoder, "r");
    g_err |= cbor_read_object(&cb->it, attrs);
    g_err |= cbor_encode_text_string(&cb->encoder, echo_buf, strlen(echo_buf));

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
    CborEncoder tasks;
    CborEncoder task;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "tasks");
    g_err |= cbor_encoder_create_map(&cb->encoder, &tasks,
                                     CborIndefiniteLength);

    prev_task = NULL;
    while (1) {
        prev_task = os_task_info_get_next(prev_task, &oti);
        if (prev_task == NULL) {
            break;
        }

        g_err |= cbor_encode_text_stringz(&tasks, oti.oti_name);
        g_err |= cbor_encoder_create_map(&tasks, &task, CborIndefiniteLength);
        g_err |= cbor_encode_text_stringz(&task, "prio");
        g_err |= cbor_encode_uint(&task, oti.oti_prio);
        g_err |= cbor_encode_text_stringz(&task, "tid");
        g_err |= cbor_encode_uint(&task, oti.oti_taskid);
        g_err |= cbor_encode_text_stringz(&task, "state");
        g_err |= cbor_encode_uint(&task, oti.oti_state);
        g_err |= cbor_encode_text_stringz(&task, "stkuse");
        g_err |= cbor_encode_uint(&task, oti.oti_stkusage);
        g_err |= cbor_encode_text_stringz(&task, "stksiz");
        g_err |= cbor_encode_uint(&task, oti.oti_stksize);
        g_err |= cbor_encode_text_stringz(&task, "cswcnt");
        g_err |= cbor_encode_uint(&task, oti.oti_cswcnt);
        g_err |= cbor_encode_text_stringz(&task, "runtime");
        g_err |= cbor_encode_uint(&task, oti.oti_runtime);
        g_err |= cbor_encode_text_stringz(&task, "last_checkin");
        g_err |= cbor_encode_uint(&task, oti.oti_last_checkin);
        g_err |= cbor_encode_text_stringz(&task, "next_checkin");
        g_err |= cbor_encode_uint(&task, oti.oti_next_checkin);
        g_err |= cbor_encoder_close_container(&tasks, &task);
    }
    g_err |= cbor_encoder_close_container(&cb->encoder, &tasks);

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
    CborEncoder pools;
    CborEncoder pool;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "mpools");
    g_err |= cbor_encoder_create_map(&cb->encoder, &pools,
                                     CborIndefiniteLength);

    prev_mp = NULL;
    while (1) {
        prev_mp = os_mempool_info_get_next(prev_mp, &omi);
        if (prev_mp == NULL) {
            break;
        }

        g_err |= cbor_encode_text_stringz(&pools, omi.omi_name);
        g_err |= cbor_encoder_create_map(&pools, &pool, CborIndefiniteLength);
        g_err |= cbor_encode_text_stringz(&pool, "blksiz");
        g_err |= cbor_encode_uint(&pool, omi.omi_block_size);
        g_err |= cbor_encode_text_stringz(&pool, "nblks");
        g_err |= cbor_encode_uint(&pool, omi.omi_num_blocks);
        g_err |= cbor_encode_text_stringz(&pool, "nfree");
        g_err |= cbor_encode_uint(&pool, omi.omi_num_free);
        g_err |= cbor_encode_text_stringz(&pool, "min");
        g_err |= cbor_encode_uint(&pool, omi.omi_min_free);
        g_err |= cbor_encoder_close_container(&pools, &pool);
    }

    g_err |= cbor_encoder_close_container(&cb->encoder, &pools);

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

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    /* Display the current datetime */
    rc = os_gettimeofday(&tv, &tz);
    assert(rc == 0);
    rc = datetime_format(&tv, &tz, buf, DATETIME_BUFSIZE);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }
    g_err |= cbor_encode_text_stringz(&cb->encoder, "datetime");
    g_err |= cbor_encode_text_stringz(&cb->encoder, buf);

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
    const struct cbor_attr_t datetime_write_attr[] = {
        [0] = {
            .attribute = "datetime",
            .type = CborAttrTextStringType,
            .addr.string = buf,
            .len = sizeof(buf),
        },
        { 0 },
    };

    rc = cbor_read_object(&cb->it, datetime_write_attr);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    /* Set the current datetime */
    rc = datetime_parse(buf, &tv, &tz);
    if (!rc) {
        rc = os_settimeofday(&tv, &tz);
        if (rc) {
          return MGMT_ERR_EINVAL;
        }
    } else {
        return MGMT_ERR_EINVAL;
    }

    rc = mgmt_cbuf_setoerr(cb, 0);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static void
nmgr_reset_tmo(struct os_event *ev)
{
    /*
     * Tickle watchdog just before re-entering bootloader.
     * Depending on what system has been doing lately, watchdog
     * timer might be close to firing.
     */
    hal_watchdog_tickle();
    hal_system_reset();
}

static int
nmgr_reset(struct mgmt_cbuf *cb)
{
    int rc;

    os_callout_init(&nmgr_reset_callout, mgmt_evq_get(), nmgr_reset_tmo, NULL);

#if MYNEWT_VAL(LOG_SOFT_RESET)
    log_reboot(HAL_RESET_REQUESTED);
#endif
    os_callout_reset(&nmgr_reset_callout, OS_TICKS_PER_SEC / 4);

    rc = mgmt_cbuf_setoerr(cb, 0);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
nmgr_os_groups_register(void)
{
    return mgmt_group_register(&nmgr_def_group);
}

