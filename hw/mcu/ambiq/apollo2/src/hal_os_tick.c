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

/**
 * NOTE: Unlike other MCUs, this one does not use SysTick to implement the
 * tickless idle state.  The SysTick timer uses HCLK as its source, and HCLK is
 * gated while this MCU is in deep sleep (e.g., during a `wfi` instruction).
 *
 * To enable a wake up from deep sleep, the idle state is instead implemented
 * using this MCU's system timer (STIMER) with LFRC as the source.
 */

#include <assert.h>
#include <os/os.h>
#include "mcu/system_apollo2.h"
#include "hal/hal_os_tick.h"
#include "bsp/cmsis_nvic.h"

#include "am_mcu_apollo.h"

#define APOLLO2_OS_TICK_FREQ    1024 /* Hz */
#define APOLLO2_OS_TICK_IRQ     STIMER_CMPR0_IRQn

/*** Value of timer when the ISR last executed. */
static uint32_t apollo2_os_tick_prev;

/*** Number of system ticks per single OS tick. */
static uint32_t apollo2_os_tick_dur;

static void
apollo2_os_tick_set_timer(int os_ticks)
{
    uint32_t sys_ticks;
    uint32_t cfg;

    OS_ASSERT_CRITICAL();

    sys_ticks = os_ticks * apollo2_os_tick_dur;

    /* Freeze time, set timer expiry, then unfreeze time. */
    cfg = am_hal_stimer_config(AM_HAL_STIMER_CFG_FREEZE);
    am_hal_stimer_compare_delta_set(0, sys_ticks);
    am_hal_stimer_config(cfg);
}

static void
apollo2_os_tick_handler(void)
{
    uint32_t cur;
    int os_ticks;
    int delta;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);

    /* Calculate elapsed ticks and advance OS time. */
    cur = am_hal_stimer_counter_get();
    delta = cur - apollo2_os_tick_prev;
    os_ticks = delta / apollo2_os_tick_dur;
    os_time_advance(os_ticks);

    /* Clear timer interrupt. */
    am_hal_stimer_int_clear(AM_HAL_STIMER_INT_COMPAREA);

    /* Update the time associated with the most recent tick. */
    apollo2_os_tick_prev += os_ticks * apollo2_os_tick_dur;

    /* Schedule timer to interrupt at the next tick. */
    apollo2_os_tick_set_timer(1);

    OS_EXIT_CRITICAL(sr);
}

void
os_tick_idle(os_time_t ticks)
{
    OS_ASSERT_CRITICAL();

    /* Since the STIMER only uses relative scheduling, all ticks values are
     * valid.  There is no need to check for wrap around.
     */

    /* Only set the timer for nonzero tick values.  For values of 0, just let
     * the timer expire on the next tick, as scheduled earlier.
     */
    if (ticks > 0) {
        apollo2_os_tick_set_timer(ticks);
    }

    __DSB();
    __WFI();

    if (ticks > 0) {
        apollo2_os_tick_handler();
    }
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    os_sr_t sr;

    /* Reset the timer to 0. */
    am_hal_stimer_counter_clear();

    /* The OS tick timer uses:
     * o The 1024 Hz low-frequency RC oscillator (LFRC)
     * o The first comparator (COMPAREA)
     */
    am_hal_stimer_config(AM_HAL_STIMER_LFRC_1KHZ |
                         AM_HAL_STIMER_CFG_COMPARE_A_ENABLE);
    am_hal_stimer_int_enable(AM_HAL_STIMER_INT_COMPAREA);

    apollo2_os_tick_dur = APOLLO2_OS_TICK_FREQ / os_ticks_per_sec;

    /* Enable the STIMER interrupt in the NVIC. */
    NVIC_SetPriority(APOLLO2_OS_TICK_IRQ, prio);
    NVIC_SetVector(APOLLO2_OS_TICK_IRQ, (uint32_t)apollo2_os_tick_handler);
    NVIC_EnableIRQ(APOLLO2_OS_TICK_IRQ);

    /* Schedule timer to interrupt at the next tick. */
    OS_ENTER_CRITICAL(sr);
    apollo2_os_tick_set_timer(1);
    OS_EXIT_CRITICAL(sr);
}
