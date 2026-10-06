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

#include "hal/hal_system.h"
#include "hal/hal_watchdog.h"
#include "os/os.h"
#if MYNEWT_VAL(LOG_SOFT_RESET)
#include "reboot/log_reboot.h"
#include "img_mgmt/img_mgmt.h"
#endif
#include "os_mgmt/os_mgmt_impl.h"
#include "os_mgmt/os_mgmt.h"
#include "mgmt/mgmt.h"
#if MYNEWT_VAL(OS_MGMT_DATETIME)
#include "datetime/datetime.h"
#endif

static struct os_callout mynewt_os_mgmt_reset_callout;

static void
mynewt_os_mgmt_reset_tmo(struct os_event *ev)
{
    /* Tickle watchdog just before re-entering bootloader.  Depending on what
     * the system has been doing lately, the watchdog timer might be close to
     * firing.
     */
    hal_watchdog_tickle();
    hal_system_reset();
}

static uint16_t
mynewt_os_mgmt_stack_usage(const struct os_task *task)
{
    struct os_task_info oti;

    os_task_info_get(task, &oti);

    return oti.oti_stkusage;
}

static const struct os_task *
mynewt_os_mgmt_task_at(int idx)
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

int
os_mgmt_impl_task_info(int idx, struct os_mgmt_task_info *out_info)
{
    const struct os_task *task;

    task = mynewt_os_mgmt_task_at(idx);
    if (task == NULL) {
        return MGMT_ERR_ENOENT;
    }

    out_info->oti_prio = task->t_prio;
    out_info->oti_taskid = task->t_taskid;
    out_info->oti_state = task->t_state;
    out_info->oti_stkusage = mynewt_os_mgmt_stack_usage(task);
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

#if MYNEWT_VAL(OS_MGMT_DATETIME)
int
os_mgmt_impl_datetime_info(char *datetime, size_t size)
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

int
os_mgmt_impl_datetime_set(char *datetime)
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

int
os_mgmt_impl_reset(unsigned int delay_ms)
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
    os_callout_init(&mynewt_os_mgmt_reset_callout, os_eventq_dflt_get(),
                    mynewt_os_mgmt_reset_tmo, NULL);

#if MYNEWT_VAL(LOG_SOFT_RESET)
    log_reboot(&info);
#endif
    os_callout_reset(&mynewt_os_mgmt_reset_callout,
                     delay_ms * OS_TICKS_PER_SEC / 1000);

    return 0;
}
