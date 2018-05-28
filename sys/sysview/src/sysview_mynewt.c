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

#include "os/mynewt.h"
#include "mcu/mcu.h"
#include "bsp/bsp.h"
#include "hal/hal_timer.h"
#include "sysview/vendor/SEGGER_SYSVIEW.h"
#include "sysview/vendor/SEGGER_SYSVIEW_Conf.h"

#define STRX(x)         #x
#define STR(x)          STRX(x)

#if MYNEWT_VAL(SYSVIEW_TIMESTAMP_USE_TIMER)

#define SYSVIEW_TIMESTAMP_FREQ          (MYNEWT_VAL(SYSVIEW_TIMESTAMP_TIMER_FREQ))

U32
SEGGER_SYSVIEW_X_GetTimestamp(void)
{
    return hal_timer_read(MYNEWT_VAL(SYSVIEW_TIMESTAMP_TIMER_NUM));
}

#elif SEGGER_SYSVIEW_CORE == SEGGER_SYSVIEW_CORE_CM3

#define SYSVIEW_TIMESTAMP_FREQ          (SystemCoreClock)

#else

#define SYSVIEW_TIMESTAMP_FREQ          (OS_TICKS_PER_SEC)

U32
SEGGER_SYSVIEW_X_GetTimestamp(void)
{
    /* XXX Just need to return some kind of timestamp... */
    return os_time_get();
}

#endif

#define SYSVIEW_CPU_FREQ                (SystemCoreClock)
#define SYSVIEW_RAM_BASE                ((uint32_t)&_ram_start)

static U64
sysview_os_api_get_time_func(void)
{
    return (U64)os_get_uptime_usec();
}

static void
sysview_os_api_send_task_list_func(void)
{
    struct os_task *t;

    STAILQ_FOREACH(t, &g_os_task_list, t_os_task_list) {
        os_trace_task_info(t);
    }
}

static const SEGGER_SYSVIEW_OS_API sysview_os_api = {
    sysview_os_api_get_time_func,
    sysview_os_api_send_task_list_func,
};

static void
sysview_os_api_send_interrupts_desc(void)
{
#ifdef MCU_SYSVIEW_INTERRUPTS
    static U8 pkt[32];
    U8 *payload;
    const char *s;
    const char *ptr;

    /*
     * Interrupts description can be very long thus we cannot send it as single
     * system description string due to length limit. Instead, we'll tokenize it
     * into separate interrupt descriptions and send one by one manually.
     */

    s = MCU_SYSVIEW_INTERRUPTS;

    do {
        ptr = s;

        while (*ptr &&  *ptr != ',') {
            ptr++;
        }

        /* Leave 4 bytes for header */
        payload = &pkt[4];
        payload = SEGGER_SYSVIEW_EncodeString(payload, s,
                          min(sizeof(pkt) - SEGGER_SYSVIEW_INFO_SIZE, ptr - s));
        SEGGER_SYSVIEW_SendPacket(pkt, payload, SYSVIEW_EVTID_SYSDESC);

        if (*ptr) {
            s = ptr + 1;
        }
    } while (*ptr);
#endif
}

static void
sysview_os_api_send_sys_desc_func(void)
{
    SEGGER_SYSVIEW_SendSysDesc("O=Apache Mynewt,"
                               "N=" STR(APP_NAME) ","
                               "D=" STR(BSP_NAME) ","
                               "C=" STR(ARCH_NAME));

    sysview_os_api_send_interrupts_desc();
}

void
sysview_init(void)
{
#if MYNEWT_VAL(SYSVIEW_TIMESTAMP_USE_TIMER)
    hal_timer_config(MYNEWT_VAL(SYSVIEW_TIMESTAMP_TIMER_NUM),
                     MYNEWT_VAL(SYSVIEW_TIMESTAMP_TIMER_FREQ));
#elif SEGGER_SYSVIEW_CORE == SEGGER_SYSVIEW_CORE_CM3
    /* Enable Cycle Counter on Cortex-M3/M4 to get accurate timestamps */
    DWT->CTRL |= (1 << DWT_CTRL_CYCCNTENA_Pos);
#endif

    SEGGER_SYSVIEW_Init(SYSVIEW_TIMESTAMP_FREQ, SYSVIEW_CPU_FREQ,
                        &sysview_os_api, sysview_os_api_send_sys_desc_func);
    SEGGER_SYSVIEW_SetRAMBase(SYSVIEW_RAM_BASE);
    SEGGER_SYSVIEW_Start();
}
