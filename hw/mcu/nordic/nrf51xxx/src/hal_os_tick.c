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
#include <hal/hal_os_tick.h>
#include <hal/hal_system.h>
#include <mcu/cmsis_nvic.h>
#include <nrf51.h>
#include <nrf51_bitfields.h>
#include <mcu/nrf51_hal.h>

/* The OS scheduler requires a low-frequency timer. */
#if MYNEWT_VAL(OS_SCHEDULING)       && \
    !MYNEWT_VAL(XTAL_32768)         && \
    !MYNEWT_VAL(XTAL_RC)            && \
    !MYNEWT_VAL(XTAL_32768_SYNTH)

    #error The OS scheduler requires a low-frequency timer; enable one of: XTAL_32768, XTAL_RC, or XTAL_32768_SYNTH
#endif

#define RTC_FREQ        32768
#define OS_TICK_TIMER   NRF_RTC1
#define OS_TICK_IRQ     RTC1_IRQn
#define OS_TICK_CMPREG      3   /* generate timer interrupt */
#define RTC_COMPARE_INT_MASK(ccreg) (1UL << ((ccreg) + 16))

static uint32_t lastocmp;
static uint32_t timer_ticks_per_ostick;
static uint32_t nrf51_max_idle_ticks;

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
nrf51_os_tick_counter(void)
{
    return (OS_TICK_TIMER->COUNTER);
}

static inline void
nrf51_os_tick_set_ocmp(uint32_t ocmp)
{
    int delta;
    uint32_t counter;

    OS_ASSERT_CRITICAL();
    while (1) {
        ocmp &= 0xffffff;
        OS_TICK_TIMER->CC[OS_TICK_CMPREG] = ocmp;
        counter = nrf51_os_tick_counter();
        /*
         * From section 19.1.7 "Compare Feature" nRF51 Reference Manual 3.0:
         *
         * - If Counter is 'N' writing (N) or (N + 1) to CC register
         *   may not trigger a compare event.
         *
         * - If Counter is 'N' writing (N + 2) to CC register is guaranteed
         *   to trigger a compare event at 'N + 2'.
         */
        delta = sub24(ocmp, counter);
        if (delta > 2) {
            break;
        }
        ocmp += timer_ticks_per_ostick;
    }
}

static void
rtc1_timer_handler(void)
{
    int ticks, delta;
    os_sr_t sr;
    uint32_t counter;

    OS_ENTER_CRITICAL(sr);

    /*
     * Calculate elapsed ticks and advance OS time.
     */
    counter = nrf51_os_tick_counter();
    delta = sub24(counter, lastocmp);
    ticks = delta / timer_ticks_per_ostick;
    os_time_advance(ticks);

    /* Clear timer interrupt */
    OS_TICK_TIMER->EVENTS_COMPARE[OS_TICK_CMPREG] = 0;

    /* Update the time associated with the most recent tick */
    lastocmp = (lastocmp + ticks * timer_ticks_per_ostick) & 0xffffff;

    /* Update the output compare to interrupt at the next tick */
    nrf51_os_tick_set_ocmp(lastocmp + timer_ticks_per_ostick);

    OS_EXIT_CRITICAL(sr);
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
        if (ticks > nrf51_max_idle_ticks) {
            ticks = nrf51_max_idle_ticks;
        }
        ocmp = lastocmp + ticks * timer_ticks_per_ostick;
        nrf51_os_tick_set_ocmp(ocmp);
    }

    __DSB();
    __WFI();

    if (ticks > 0) {
        /*
         * Update OS time before anything else when coming out of
         * the tickless regime.
         */
        rtc1_timer_handler();
    }
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t ctx;

    assert(RTC_FREQ % os_ticks_per_sec == 0);

    lastocmp = 0;
    timer_ticks_per_ostick = RTC_FREQ / os_ticks_per_sec;

    /*
     * The maximum number of OS ticks allowed to elapse during idle is
     * limited to 1/4th the number of timer ticks before the 24-bit counter
     * rolls over.
     */
    nrf51_max_idle_ticks = (1UL << 22) / timer_ticks_per_ostick;

    /* disable interrupts */
    __HAL_DISABLE_INTERRUPTS(ctx);

    /* Set isr in vector table and enable interrupt */
    NVIC_SetPriority(OS_TICK_IRQ, prio);
    NVIC_SetVector(OS_TICK_IRQ, (uint32_t)rtc1_timer_handler);
    NVIC_EnableIRQ(OS_TICK_IRQ);

    /*
     * Program the OS_TICK_TIMER to operate at 32KHz and trigger an output
     * compare interrupt at a rate of 'os_ticks_per_sec'.
     */
    OS_TICK_TIMER->TASKS_STOP = 1;
    OS_TICK_TIMER->TASKS_CLEAR = 1;

    OS_TICK_TIMER->EVTENCLR = 0xffffffff;
    OS_TICK_TIMER->INTENCLR = 0xffffffff;
    OS_TICK_TIMER->INTENSET = RTC_COMPARE_INT_MASK(OS_TICK_CMPREG);

    OS_TICK_TIMER->EVENTS_COMPARE[OS_TICK_CMPREG] = 0;
    OS_TICK_TIMER->CC[OS_TICK_CMPREG] = timer_ticks_per_ostick;

    OS_TICK_TIMER->TASKS_START = 1;

    __HAL_ENABLE_INTERRUPTS(ctx);
}
