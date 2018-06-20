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
#include "os/mynewt.h"
#include "hal/hal_os_tick.h"
#include "inc/arc/arc.h"
#include "inc/arc/arc_exception.h"
#include "inc/arc/arc_timer.h"

struct hal_os_tick
{
    uint8_t timer_num;
    uint8_t vecnum;
    int ticks_per_ostick;
    os_time_t max_idle_ticks;
    uint32_t lastocmp;
};

struct hal_os_tick g_hal_os_tick;

void
os_tick_idle(os_time_t ticks)
{
    (void)ticks;
}

static void
arc_timer_handler(void *arg)
{
    uint32_t sr;

    os_trace_isr_enter();
    arc_timer_int_clear(g_hal_os_tick.timer_num);

    OS_ENTER_CRITICAL(sr);

    /* TODO: for now, just advance one tick */
    os_time_advance(1);

    OS_EXIT_CRITICAL(sr);
    os_trace_isr_exit();
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    int32_t rc;
    uint32_t sr;

    assert(ARC_FEATURE_CPU_CLOCK_FREQ % os_ticks_per_sec == 0);

    g_hal_os_tick.ticks_per_ostick = ARC_FEATURE_CPU_CLOCK_FREQ /
        os_ticks_per_sec;

    /* Determine if timer 0 or timer 1 present */
    if (arc_timer_present(TIMER_0)) {
        g_hal_os_tick.timer_num = TIMER_0;
        g_hal_os_tick.vecnum = 16;
    } else if (arc_timer_present(TIMER_1)) {
        g_hal_os_tick.timer_num = TIMER_1;
        g_hal_os_tick.vecnum = 17;
    } else {
        /* Must be at least timer */
        assert(0);
    }

    /* disable interrupts */
    OS_ENTER_CRITICAL(sr);

    /* Set isr in vector table and enable interrupt */
    rc = int_pri_set(g_hal_os_tick.vecnum, (uint32_t)prio);
    assert(rc == 0);

    rc = int_handler_install(g_hal_os_tick.vecnum, arc_timer_handler);
    assert(rc == 0);

    rc = int_enable(g_hal_os_tick.vecnum);
    assert(rc == 0);

    arc_timer_stop(g_hal_os_tick.timer_num);
    arc_timer_start(g_hal_os_tick.timer_num,
                    TIMER_CTRL_IE | TIMER_CTRL_NH,
                    g_hal_os_tick.ticks_per_ostick);

    OS_EXIT_CRITICAL(sr);
}
