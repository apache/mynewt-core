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
#if !MYNEWT_VAL(MCU_DEBUG_DSER_HAL_OS_TICK)
#define MCU_DIAG_SER_DISABLE
#endif

#include <assert.h>
#include "mcu/mcu.h"
#include "mcu/cmac_timer.h"
#include "hal/hal_os_tick.h"
#include "cmac_priv.h"

static uint32_t g_os_tick_last;

static void
os_tick_setup_for_next(uint32_t delta)
{
    cmac_timer_write_eq_hal_os_tick(g_os_tick_last + delta);
}

static void
os_tick_handle_tick(void)
{
    uint32_t cur_tick;
    uint32_t delta;

    cmac_timer_int_os_tick_clear();

    cur_tick = cmac_timer_get_hal_os_tick();
    delta = cur_tick - g_os_tick_last;

    os_time_advance(delta);

    g_os_tick_last = cur_tick;
    os_tick_setup_for_next(1024);
}

static void
os_tick_cmac_timer_cb(void)
{
    os_tick_handle_tick();
}

void
os_tick_idle(os_time_t ticks)
{
    MCU_DIAG_SER('(');

    os_arch_cmac_idle_section_enter();

    if (ticks > 0) {
        os_tick_setup_for_next(ticks);
    }

    cmac_sleep();

    os_tick_handle_tick();

    os_arch_cmac_idle_section_exit();

    MCU_DIAG_SER(')');
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    assert(os_ticks_per_sec == 31250);

    g_os_tick_last = 0;

    cmac_timer_int_os_tick_register(os_tick_cmac_timer_cb);

    os_tick_setup_for_next(1);
}
