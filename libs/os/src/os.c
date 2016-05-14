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

#include "os/os.h"
#include "os/queue.h"

#include "hal/hal_os_tick.h"

#include <assert.h>

struct os_task g_idle_task;
os_stack_t g_idle_task_stack[OS_STACK_ALIGN(OS_IDLE_STACK_SIZE)];

uint32_t g_os_idle_ctr;
/* Default zero.  Set by the architecture specific code when os is started.
 */
int g_os_started;

#ifdef ARCH_sim
#define MIN_IDLE_TICKS  1
#else
#define MIN_IDLE_TICKS  (100 * OS_TICKS_PER_SEC / 1000) /* 100 msec */
#endif
#define MAX_IDLE_TICKS  (600 * OS_TICKS_PER_SEC)        /* 10 minutes */

/**
 * Idle operating system task, runs when no other tasks are running.
 * The idle task operates in tickless mode, which means it looks for
 * the next time an event in the system needs to run, and then tells
 * the architecture specific functions to sleep until that time.
 *
 * @param arg unused
 */
void
os_idle_task(void *arg)
{
    os_sr_t sr;
    os_time_t now;
    os_time_t iticks, sticks, cticks;

    while (1) {
        ++g_os_idle_ctr;
        OS_ENTER_CRITICAL(sr);
        now = os_time_get();
        sticks = os_sched_wakeup_ticks(now);
        cticks = os_callout_wakeup_ticks(now);
        iticks = min(sticks, cticks);
        if (iticks < MIN_IDLE_TICKS) {
            iticks = 0;
        } else if (iticks > MAX_IDLE_TICKS) {
            iticks = MAX_IDLE_TICKS;
        } else {
            /* NOTHING */
        }
        /* Tell the architecture specific support to put the processor to sleep
         * for 'n' ticks.
         */
        os_tick_idle(iticks);
        OS_EXIT_CRITICAL(sr);
    }
}

/**
 * Has the operating system started.
 *
 * @return 1 if the operating system has started, 0 if it hasn't
 */
int
os_started(void)
{
    return (g_os_started);
}


void
os_init_idle_task(void)
{
    os_task_init(&g_idle_task, "idle", os_idle_task, NULL,
            OS_IDLE_PRIO, OS_WAIT_FOREVER, g_idle_task_stack,
            OS_STACK_ALIGN(OS_IDLE_STACK_SIZE));
}

/**
 * Initialize the operating system, calls into the architecture specific
 * support to initialize the operating system.
 */
void
os_init(void)
{
    os_error_t err;

    err = os_arch_os_init();
    assert(err == OS_OK);
}

/**
 * Start the operating system, calls into the architecture specific support
 * to start the operating system.
 */
void
os_start(void)
{
    os_error_t err;

    err = os_arch_os_start();
    assert(err == OS_OK);
}
