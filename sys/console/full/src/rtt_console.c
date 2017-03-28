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

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(CONSOLE_RTT)
#include <ctype.h>

#include "os/os.h"
#include "rtt/SEGGER_RTT.h"
#include "console/console.h"
#include "console_priv.h"

#if MYNEWT_VAL(CONSOLE_INPUT)
#define RTT_TASK_PRIO         (5)
#define RTT_STACK_SIZE        (512)
static struct os_task rtt_task;
static os_stack_t rtt_task_stack[RTT_STACK_SIZE];
#endif

static const char CR = '\r';

int
console_out(int character)
{
    char c = (char)character;

    if ('\n' == c) {
        SEGGER_RTT_WriteNoLock(0, &CR, 1);
        console_is_midline = 0;
    } else {
        console_is_midline = 1;
    }

    SEGGER_RTT_WriteNoLock(0, &c, 1);

    return character;
}

#if MYNEWT_VAL(CONSOLE_INPUT)
void
rtt(void *arg)
{
    int sr;
    int key;
    int i = 0;

    while (1) {
        OS_ENTER_CRITICAL(sr);
        key = SEGGER_RTT_GetKey();
        if (key >= 0) {
            console_handle_char((char)key);
            i = 0;
        }
        OS_EXIT_CRITICAL(sr);
        /* These values were selected to keep the shell responsive
         * and at the same time reduce context switches.
         * Min sleep is 50ms and max is 250ms.
         */
        if (i < 5) {
            ++i;
        }
        os_time_delay((OS_TICKS_PER_SEC / 20) * i);
    }
}

static void
init_task(void)
{
    /* Initialize the task */
    os_task_init(&rtt_task, "rtt", rtt, NULL, RTT_TASK_PRIO,
                 OS_WAIT_FOREVER, rtt_task_stack, RTT_STACK_SIZE);
}
#endif

int
rtt_console_is_init(void)
{
#if MYNEWT_VAL(CONSOLE_INPUT)
    return rtt_task.t_state == OS_TASK_READY ||
           rtt_task.t_state == OS_TASK_SLEEP;
#else
    return 1;
#endif
}

int
rtt_console_init(void)
{
    SEGGER_RTT_Init();
#if MYNEWT_VAL(CONSOLE_INPUT)
    init_task();
#endif
    return 0;
}

#endif /* MYNEWT_VAL(CONSOLE_RTT) */
