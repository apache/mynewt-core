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

#include "os/mynewt.h"
#include "hal/hal_system.h"
#include "hal/hal_watchdog.h"
#include "tinycbor/cbor.h"
#include "cborattr/cborattr.h"
#include "mgmt/mgmt.h"
#include "os_mgmt/os_mgmt.h"
#if MYNEWT_VAL(LOG_SOFT_RESET)
#include "reboot/log_reboot.h"
#include "img_mgmt/img_mgmt.h"
#endif
#if MYNEWT_VAL(OS_MGMT_DATETIME)
#include "datetime/datetime.h"
#endif

static struct os_callout os_mgmt_reset_callout;

static void
os_mgmt_reset_tmo(struct os_event *ev)
{
    /* Tickle watchdog just before re-entering bootloader.  Depending on what
     * the system has been doing lately, the watchdog timer might be close to
     * firing.
     */
    hal_watchdog_tickle();
    hal_system_reset();
}

#if MYNEWT_VAL(OS_MGMT_TASKSTAT)
static uint16_t
os_mgmt_stack_usage(const struct os_task *task)
{
    struct os_task_info oti;

    os_task_info_get(task, &oti);

    return oti.oti_stkusage;
}

static const struct os_task *
os_mgmt_task_at(int idx)
{
    const struct os_task *task;
    int i;

    task = STAILQ_FIRST(&g_os_task_list);
    for (i = 0; i < idx; i++) {
        if (task == NULL) {
            break;
        }

        task = STAILQ_NEXT(task, t_os_task_list);
    }

    return task;
}

static int
os_mgmt_task_info_get(int idx, struct os_mgmt_task_info *out_info)
{
    const struct os_task *task;

    task = os_mgmt_task_at(idx);
    if (task == NULL) {
        return MGMT_ERR_ENOENT;
    }

    out_info->oti_prio = task->t_prio;
    out_info->oti_taskid = task->t_taskid;
    out_info->oti_state = task->t_state;
    out_info->oti_stkusage = os_mgmt_stack_usage(task);
    out_info->oti_stksize = task->t_stacksize;
    out_info->oti_cswcnt = task->t_ctx_sw_cnt;
    out_info->oti_runtime = task->t_run_time;
    out_info->oti_last_checkin = task->t_sanity_check.sc_checkin_last;
    out_info->oti_next_checkin = task->t_sanity_check.sc_checkin_last +
                                 task->t_sanity_check.sc_checkin_itvl;
    strncpy(out_info->oti_name, task->t_name, sizeof out_info->oti_name - 1);
    out_info->oti_name[sizeof out_info->oti_name - 1] = '\0';

    return 0;
}
#endif

#if MYNEWT_VAL(OS_MGMT_DATETIME)
static int
os_mgmt_datetime_get(char *datetime, size_t size)
{
    struct os_timeval tv;
    struct os_timezone tz;
    int rc;

    if (size < DATETIME_BUFSIZE) {
        return MGMT_ERR_ENOMEM;
    }

    rc = os_gettimeofday(&tv, &tz);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    rc = datetime_format(&tv, &tz, datetime, size);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    return MGMT_ERR_EOK;
}

static int
os_mgmt_datetime_set(char *datetime)
{
    struct os_timeval tv;
    struct os_timezone tz;
    int rc = 0;

    rc = datetime_parse(datetime, &tv, &tz);
    if (rc != 0) {
        return MGMT_ERR_ECORRUPT;
    }

    return os_settimeofday(&tv, &tz);
}
#endif

static int
os_mgmt_reset_schedule(unsigned int delay_ms)
{
#if MYNEWT_VAL(LOG_SOFT_RESET)
    struct log_reboot_info info = {
        .reason = HAL_RESET_REQUESTED,
        .file = NULL,
        .line = 0,
        .pc = 0,
    };

    if (img_mgmt_state_any_pending()) {
        info.reason = HAL_RESET_DFU;
    }
#endif
    os_callout_init(&os_mgmt_reset_callout, os_eventq_dflt_get(),
                    os_mgmt_reset_tmo, NULL);

#if MYNEWT_VAL(LOG_SOFT_RESET)
    log_reboot(&info);
#endif
    os_callout_reset(&os_mgmt_reset_callout,
                     delay_ms * OS_TICKS_PER_SEC / 1000);

    return 0;
}

#if MYNEWT_VAL(OS_MGMT_ECHO)
static mgmt_handler_fn os_mgmt_echo;
#endif

static mgmt_handler_fn os_mgmt_reset;

#if MYNEWT_VAL(OS_MGMT_TASKSTAT)
static mgmt_handler_fn os_mgmt_taskstat_read;
#endif

#if MYNEWT_VAL(OS_MGMT_DATETIME)
static mgmt_handler_fn os_mgmt_datetime_read;
static mgmt_handler_fn os_mgmt_datetime_write;
#endif

static const struct mgmt_handler os_mgmt_group_handlers[] = {
#if MYNEWT_VAL(OS_MGMT_ECHO)
    [OS_MGMT_ID_ECHO] = {
        os_mgmt_echo, os_mgmt_echo
    },
#endif
#if MYNEWT_VAL(OS_MGMT_TASKSTAT)
    [OS_MGMT_ID_TASKSTAT] = {
        os_mgmt_taskstat_read, NULL
    },
#endif
#if MYNEWT_VAL(OS_MGMT_DATETIME)
    [OS_MGMT_ID_DATETIME_STR] = {
        os_mgmt_datetime_read, os_mgmt_datetime_write
    },
#endif
    [OS_MGMT_ID_RESET] = {
        NULL, os_mgmt_reset
    },
};

#define OS_MGMT_GROUP_SZ    \
    (sizeof os_mgmt_group_handlers / sizeof os_mgmt_group_handlers[0])

static struct mgmt_group os_mgmt_group = {
    .mg_handlers = os_mgmt_group_handlers,
    .mg_handlers_count = OS_MGMT_GROUP_SZ,
    .mg_group_id = MGMT_GROUP_ID_OS,
};

/**
 * Command handler: os echo
 */
#if MYNEWT_VAL(OS_MGMT_ECHO)
static int
os_mgmt_echo(struct mgmt_ctxt *ctxt)
{
    char echo_buf[128];
    CborError err;

    const struct cbor_attr_t attrs[2] = {
        [0] = {
            .attribute = "d",
            .type = CborAttrTextStringType,
            .addr.string = echo_buf,
            .nodefault = 1,
            .len = sizeof echo_buf,
        },
        [1] = {
            .attribute = NULL
        }
    };

    echo_buf[0] = '\0';

    err = cbor_read_object(&ctxt->it, attrs);
    if (err != 0) {
        return MGMT_ERR_EINVAL;
    }

    err |= cbor_encode_text_stringz(&ctxt->encoder, "r");
    err |= cbor_encode_text_string(&ctxt->encoder, echo_buf, strlen(echo_buf));

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}
#endif

#if MYNEWT_VAL(OS_MGMT_TASKSTAT)
/**
 * Encodes a single taskstat entry.
 */
static int
os_mgmt_taskstat_encode_one(struct CborEncoder *encoder,
                            const struct os_mgmt_task_info *task_info)
{
    CborEncoder task_map;
    CborError err;

    err = 0;
    err |= cbor_encode_text_stringz(encoder, task_info->oti_name);
    err |= cbor_encoder_create_map(encoder, &task_map, CborIndefiniteLength);
    err |= cbor_encode_text_stringz(&task_map, "prio");
    err |= cbor_encode_uint(&task_map, task_info->oti_prio);
    err |= cbor_encode_text_stringz(&task_map, "tid");
    err |= cbor_encode_uint(&task_map, task_info->oti_taskid);
    err |= cbor_encode_text_stringz(&task_map, "state");
    err |= cbor_encode_uint(&task_map, task_info->oti_state);
    err |= cbor_encode_text_stringz(&task_map, "stkuse");
    err |= cbor_encode_uint(&task_map, task_info->oti_stkusage);
    err |= cbor_encode_text_stringz(&task_map, "stksiz");
    err |= cbor_encode_uint(&task_map, task_info->oti_stksize);
    err |= cbor_encode_text_stringz(&task_map, "cswcnt");
    err |= cbor_encode_uint(&task_map, task_info->oti_cswcnt);
    err |= cbor_encode_text_stringz(&task_map, "runtime");
    err |= cbor_encode_uint(&task_map, task_info->oti_runtime);
    err |= cbor_encode_text_stringz(&task_map, "last_checkin");
    err |= cbor_encode_uint(&task_map, task_info->oti_last_checkin);
    err |= cbor_encode_text_stringz(&task_map, "next_checkin");
    err |= cbor_encode_uint(&task_map, task_info->oti_next_checkin);
    err |= cbor_encoder_close_container(encoder, &task_map);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Command handler: os taskstat
 */
static int
os_mgmt_taskstat_read(struct mgmt_ctxt *ctxt)
{
    struct os_mgmt_task_info task_info;
    struct CborEncoder tasks_map;
    CborError err;
    int task_idx;
    int rc;

    err = 0;
    err |= cbor_encode_text_stringz(&ctxt->encoder, "tasks");
    err |= cbor_encoder_create_map(&ctxt->encoder, &tasks_map,
                                   CborIndefiniteLength);
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    /* Iterate the list of tasks, encoding each. */
    for (task_idx = 0; ; task_idx++) {
        rc = os_mgmt_task_info_get(task_idx, &task_info);
        if (rc == MGMT_ERR_ENOENT) {
            /* No more tasks to encode. */
            break;
        } else if (rc != 0) {
            return rc;
        }

        rc = os_mgmt_taskstat_encode_one(&tasks_map, &task_info);
        if (rc != 0) {
            cbor_encoder_close_container(&ctxt->encoder, &tasks_map);
            return rc;
        }
    }

    err = cbor_encoder_close_container(&ctxt->encoder, &tasks_map);
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}
#endif

#if MYNEWT_VAL(OS_MGMT_DATETIME)
/**
 * Command handler: os datetime
 */
static int
os_mgmt_datetime_read(struct mgmt_ctxt *ctxt)
{
    char buf[DATETIME_BUFSIZE];
    CborError err = 0;

    err = cbor_encode_text_stringz(&ctxt->encoder, "datetime");
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    err = os_mgmt_datetime_get(buf, sizeof(buf));
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    err = cbor_encode_text_stringz(&ctxt->encoder, buf);
    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return MGMT_ERR_EOK;
}

/**
 * Command handler: os datetime
 */
static int
os_mgmt_datetime_write(struct mgmt_ctxt *ctxt)
{
    char datetime_buf[DATETIME_BUFSIZE];
    CborError err;

    const struct cbor_attr_t attrs[2] = {
        [0] = {
               .attribute = "datetime",
               .type = CborAttrTextStringType,
               .addr.string = datetime_buf,
               .nodefault = 1,
               .len = sizeof datetime_buf,
               },
        [1] = { .attribute = NULL }
    };

    datetime_buf[0] = '\0';

    err = cbor_read_object(&ctxt->it, attrs);
    if (err != 0 || datetime_buf[0] == '\0') {
        return MGMT_ERR_EINVAL;
    }

    err = os_mgmt_datetime_set(datetime_buf);
    if (err != 0) {
        return MGMT_ERR_ECORRUPT;
    }

    return MGMT_ERR_EOK;
}
#endif

/**
 * Command handler: os reset
 */
static int
os_mgmt_reset(struct mgmt_ctxt *ctxt)
{
    return os_mgmt_reset_schedule(MYNEWT_VAL(OS_MGMT_RESET_MS));
}

void
os_mgmt_register_group(void)
{
    mgmt_register_group(&os_mgmt_group);
}

void
os_mgmt_module_init(void)
{
    os_mgmt_register_group();
}
