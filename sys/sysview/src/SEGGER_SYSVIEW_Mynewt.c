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

#include <ctype.h>
#include <stdint.h>

#include "os/os.h"
#include "os/os_trace_api.h"

#include "sysview/vendor/SEGGER_SYSVIEW.h"
#include "sysview/vendor/Global.h"
#include "sysview/SEGGER_SYSVIEW_Mynewt.h"

void os_trace_enter_isr(void)
{
    SEGGER_SYSVIEW_RecordEnterISR();
}

void os_trace_exit_isr(void)
{
    SEGGER_SYSVIEW_RecordExitISR();
}

void os_trace_exit_isr_to_scheduler(void)
{
    SEGGER_SYSVIEW_RecordExitISRToScheduler();
}

void os_trace_task_info(const struct os_task *t)
{
    SEGGER_SYSVIEW_TASKINFO Info;
    Info.TaskID = t->t_taskid;
    Info.sName = t->t_name;
    Info.Prio = t->t_prio;
    Info.StackBase = (uint32_t)t->t_stackptr;
    Info.StackSize = t->t_stacksize;
    SEGGER_SYSVIEW_SendTaskInfo(&Info);
}

void os_trace_task_create(uint32_t task_id)
{
    SEGGER_SYSVIEW_OnTaskCreate(task_id);
}

void os_trace_task_start_exec(uint32_t task_id)
{
    SEGGER_SYSVIEW_OnTaskStartExec(task_id);
}

void os_trace_task_stop_exec(void)
{
    SEGGER_SYSVIEW_OnTaskStopExec();
}

void os_trace_task_start_ready(uint32_t task_id)
{
    SEGGER_SYSVIEW_OnTaskStartReady(task_id);
}

void os_trace_task_stop_ready(uint32_t task_id, unsigned reason)
{
    SEGGER_SYSVIEW_OnTaskStopReady(task_id, reason);
}

void os_trace_idle(void)
{
    SEGGER_SYSVIEW_OnIdle();
}

void os_trace_void(unsigned id)
{
    SEGGER_SYSVIEW_RecordVoid(id);
}

void os_trace_u32(unsigned id, uint32_t para0)
{
    SEGGER_SYSVIEW_RecordU32(id, para0);
}

void os_trace_u32x2(unsigned id, uint32_t para0, uint32_t para1)
{
    SEGGER_SYSVIEW_RecordU32x2(id, para0, para1);
}

void os_trace_u32x3(unsigned id, uint32_t para0, uint32_t para1, uint32_t para2)
{
    SEGGER_SYSVIEW_RecordU32x3(id, para0, para1, para2);
}

void os_trace_u32x4(unsigned id, uint32_t para0, uint32_t para1, uint32_t para2,
        uint32_t para3)
{
    SEGGER_SYSVIEW_RecordU32x4(id, para0, para1, para2, para3);
}

void os_trace_u32x5(unsigned id, uint32_t para0, uint32_t para1, uint32_t para2,
        uint32_t para3, uint32_t para4)
{
    SEGGER_SYSVIEW_RecordU32x5(id, para0, para1, para2, para3, para4);
}

void os_trace_enter_timer(uint32_t timer_id)
{
    SEGGER_SYSVIEW_RecordEnterTimer(timer_id);
}

void os_trace_exit_timer(void)
{
    SEGGER_SYSVIEW_RecordExitTimer();
}

void os_trace_end_call(unsigned id)
{
    SEGGER_SYSVIEW_RecordEndCall(id);
}

void os_trace_end_call_return_value(unsigned id, uint32_t return_value)
{
    SEGGER_SYSVIEW_RecordEndCallU32(id, return_value);
}

static void send_task_list_cb(void) {
    struct os_task *prev_task;
    struct os_task_info oti;
    SEGGER_SYSVIEW_TASKINFO Info;

    prev_task = NULL;
    while (1) {
        prev_task = os_task_info_get_next(prev_task, &oti);
        if (prev_task == NULL) {
            break;
        }
        Info.TaskID = oti.oti_taskid;
        Info.sName = oti.oti_name;
        Info.StackSize = oti.oti_stksize;
        Info.Prio = oti.oti_prio;
        SEGGER_SYSVIEW_SendTaskInfo(&Info);
    }
}

static U64 get_time_cb(void) {
    return (U64)os_time_get();
}

const SEGGER_SYSVIEW_OS_API SYSVIEW_X_OS_TraceAPI = {
    get_time_cb,
    send_task_list_cb,
};

/*
 * Package initialization function
 */
void
sysview_init(void)
{
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();
}
