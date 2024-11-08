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
#include <os/mynewt.h>
#include <hal/hal_os_tick.h>
#include <nrf.h>
#include <mcu/cmsis_nvic.h>

/* The OS scheduler requires a low-frequency timer. */
#if MYNEWT_VAL(OS_SCHEDULING) && !MYNEWT_VAL(MCU_LFCLK_SOURCE)
    #error The OS scheduler requires a low-frequency timer; configure MCU_LFCLK_SOURCE
#endif

#define RTC_FREQ            32768       /* in Hz */

struct hal_os_tick {
    int ticks_per_ostick;
    os_time_t max_idle_ticks;
    uint32_t lastocmp;
};

struct hal_os_tick g_hal_os_tick;
struct hal_timer os_tick_timer;

/*
 * Implement (x - y) where the range of both 'x' and 'y' is limited to 24-bits.
 *
 * For example:
 *
 * sub24(0, 0xffffff) = 1
 * sub24(0xffffff, 0xfffffe) = 1
 * sub24(0xffffff, 0) = -1
 * sub24(0x7fffff, 0) = 8388607
 * sub24(0x800000, 0) = -8388608
 */
static inline int
sub24(uint32_t x, uint32_t y)
{
    int result;

    assert(x <= 0xffffff);
    assert(y <= 0xffffff);

    result = x - y;
    if (result & 0x800000) {
        return (result | 0xff800000);
    } else {
        return (result & 0x007fffff);
    }
}

static inline uint32_t
nrf5340_net_os_tick_counter(void)
{
    return os_cputime_get32();
}

static inline void
nrf5340_net_os_tick_set_ocmp(uint32_t ocmp)
{
    OS_ASSERT_CRITICAL();
    ocmp &= 0xffffff;
    os_cputime_timer_start(&os_tick_timer, ocmp);
}

static void
nrf5340_net_timer_handler(void *arg)
{
    int delta;
    int ticks;
    os_sr_t sr;
    uint32_t counter;

    os_trace_isr_enter();
    OS_ENTER_CRITICAL(sr);

    /* Calculate elapsed ticks and advance OS time. */

    counter = nrf5340_net_os_tick_counter();
    delta = sub24(counter, g_hal_os_tick.lastocmp);
    ticks = delta / g_hal_os_tick.ticks_per_ostick;
    os_time_advance(ticks);

    /* Clear timer interrupt */

    /* Update the time associated with the most recent tick */
    g_hal_os_tick.lastocmp = (g_hal_os_tick.lastocmp +
                              (ticks * g_hal_os_tick.ticks_per_ostick)) & 0xffffff;

    /* Update the output compare to interrupt at the next tick */
    nrf5340_net_os_tick_set_ocmp(g_hal_os_tick.lastocmp + g_hal_os_tick.ticks_per_ostick);

    OS_EXIT_CRITICAL(sr);
    os_trace_isr_exit();
}

void
os_tick_idle(os_time_t ticks)
{
    uint32_t ocmp;

    OS_ASSERT_CRITICAL();

    if (ticks > 0) {
        /*
         * Enter tickless regime during long idle durations.
         */
        if (ticks > g_hal_os_tick.max_idle_ticks) {
            ticks = g_hal_os_tick.max_idle_ticks;
        }
        ocmp = g_hal_os_tick.lastocmp + (ticks*g_hal_os_tick.ticks_per_ostick);
        nrf5340_net_os_tick_set_ocmp(ocmp);
    }

    __DSB();
    __WFI();

    if (ticks > 0) {
        /*
         * Update OS time before anything else when coming out of
         * the tickless regime.
         */
        nrf5340_net_timer_handler(NULL);
    }
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    assert(RTC_FREQ % os_ticks_per_sec == 0);

    g_hal_os_tick.lastocmp = 0;
    g_hal_os_tick.ticks_per_ostick = RTC_FREQ / os_ticks_per_sec;

    /*
     * The maximum number of OS ticks allowed to elapse during idle is
     * limited to 1/4th the number of timer ticks before the 24-bit counter
     * rolls over.
     */
    g_hal_os_tick.max_idle_ticks = (1UL << 22) / g_hal_os_tick.ticks_per_ostick;

    os_cputime_timer_init(&os_tick_timer, nrf5340_net_timer_handler, NULL);
    os_cputime_timer_start(&os_tick_timer, g_hal_os_tick.ticks_per_ostick);
}
