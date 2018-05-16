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

#ifndef OS_TRACE_API_H
#define OS_TRACE_API_H

#include <stdint.h>
#include "syscfg/syscfg.h"
#if MYNEWT_VAL(OS_SYSVIEW)
#include "sysview/vendor/SEGGER_SYSVIEW.h"
#endif

#define OS_TRACE_ID_OFFSET                    (32u)

#define OS_TRACE_ID_EVQ_PUT                   (1u + OS_TRACE_ID_OFFSET)
#define OS_TRACE_ID_EVQ_GET                   (2u + OS_TRACE_ID_OFFSET)
#define OS_TRACE_ID_MUTEX_INIT                (3u + OS_TRACE_ID_OFFSET)
#define OS_TRACE_ID_MUTEX_RELEASE             (4u + OS_TRACE_ID_OFFSET)
#define OS_TRACE_ID_MUTEX_PEND                (5u + OS_TRACE_ID_OFFSET)

#if MYNEWT_VAL(OS_SYSVIEW)

static inline void
os_trace_isr_enter(void)
{
    SEGGER_SYSVIEW_RecordEnterISR();
}

static inline void
os_trace_isr_exit(void)
{
    SEGGER_SYSVIEW_RecordExitISR();
}

static inline void
os_trace_task_info(const struct os_task *t)
{
    SEGGER_SYSVIEW_TASKINFO ti;

    ti.TaskID = (uint32_t)t;
    ti.sName = t->t_name;
    ti.Prio = t->t_prio;
    ti.StackBase = (uint32_t)&t->t_stacktop;
    ti.StackSize = t->t_stacksize * sizeof(os_stack_t);

    SEGGER_SYSVIEW_SendTaskInfo(&ti);
}

static inline void
os_trace_task_create(const struct os_task *t)
{
    SEGGER_SYSVIEW_OnTaskCreate((uint32_t)t);
}

static inline void
os_trace_task_start_exec(const struct os_task *t)
{
    SEGGER_SYSVIEW_OnTaskStartExec((uint32_t)t);
}

static inline void
os_trace_task_stop_exec(void)
{
    SEGGER_SYSVIEW_OnTaskStopExec();
}

static inline void
os_trace_task_start_ready(const struct os_task *t)
{
    SEGGER_SYSVIEW_OnTaskStartReady((uint32_t)t);
}

static inline void
os_trace_task_stop_ready(const struct os_task *t, unsigned reason)
{
    SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)t, reason);
}

static inline void
os_trace_idle(void)
{
    SEGGER_SYSVIEW_OnIdle();
}

static inline void
os_trace_api_void(unsigned id)
{
    SEGGER_SYSVIEW_RecordVoid(id);
}

static inline void
os_trace_api_u32(unsigned id, uint32_t p0)
{
    SEGGER_SYSVIEW_RecordU32(id, p0);
}

static inline void
os_trace_api_u32x2(unsigned id, uint32_t p0, uint32_t p1)
{
    SEGGER_SYSVIEW_RecordU32x2(id, p0, p1);
}

static inline void
os_trace_api_u32x3(unsigned id, uint32_t p0, uint32_t p1, uint32_t p2)
{
    SEGGER_SYSVIEW_RecordU32x3(id, p0, p1, p2);
}

static inline void
os_trace_api_ret(unsigned id)
{
    SEGGER_SYSVIEW_RecordEndCall(id);
}

static inline void
os_trace_api_ret_u32(unsigned id, uint32_t ret)
{
    SEGGER_SYSVIEW_RecordEndCallU32(id, ret);
}

#else

static inline void
os_trace_isr_enter(void)
{
}

static inline void
os_trace_isr_exit(void)
{
}

static inline void
os_trace_task_info(const struct os_task *t)
{
}

static inline void
os_trace_task_create(const struct os_task *t)
{
}

static inline void
os_trace_task_start_exec(const struct os_task *t)
{
}

static inline void
os_trace_task_stop_exec(void)
{
}

static inline void
os_trace_task_start_ready(const struct os_task *t)
{
}

static inline void
os_trace_task_stop_ready(const struct os_task *t, unsigned reason)
{
}

static inline void
os_trace_idle(void)
{
}

static inline void
os_trace_api_void(unsigned id)
{
}

static inline void
os_trace_api_u32(unsigned id, uint32_t p0)
{
}

static inline void
os_trace_api_u32x2(unsigned id, uint32_t p0, uint32_t p1)
{
}

static inline void
os_trace_api_u32x3(unsigned id, uint32_t p0, uint32_t p1, uint32_t p2)
{
}

static inline void
os_trace_api_ret(unsigned id)
{
}

static inline void
os_trace_api_ret_u32(unsigned id, uint32_t return_value)
{
}

#endif

#endif
