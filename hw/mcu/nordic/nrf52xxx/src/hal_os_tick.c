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
#include <os/os.h>
#include <os/os_trace_api.h>
#include "syscfg/syscfg.h"
#include "hal/hal_os_tick.h"
#include "nrf.h"
#include "bsp/cmsis_nvic.h"


#if MYNEWT_VAL(XTAL_32768)
#define RTC_FREQ            32768       /* in Hz */
#define OS_TICK_TIMER       NRF_RTC1
#define OS_TICK_IRQ         RTC1_IRQn
#define OS_TICK_CMPREG      3   /* generate timer interrupt */
#define OS_TICK_PRESCALER   1   /* prescaler to generate timer freq */
#define TIMER_LT(__t1, __t2)    ((((__t1) - (__t2)) & 0xffffff) > 0x800000)
#define RTC_COMPARE_INT_MASK(ccreg) (1UL << ((ccreg) + 16))
#else
#define OS_TICK_TIMER       NRF_TIMER1
#define OS_TICK_IRQ         TIMER1_IRQn
#define OS_TICK_CMPREG      0   /* generate timer interrupt */
#define OS_TICK_COUNTER     1   /* capture current timer value */
#define OS_TICK_PRESCALER   4   /* prescaler to generate 1MHz timer freq */
#define TIMER_LT(__t1, __t2)    ((int32_t)((__t1) - (__t2)) < 0)
#define TIMER_COMPARE_INT_MASK(ccreg)   (1UL << ((ccreg) + 16))
#endif

struct hal_os_tick
{
    int ticks_per_ostick;
    os_time_t max_idle_ticks;
    uint32_t lastocmp;
};

struct hal_os_tick g_hal_os_tick;

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
nrf52_os_tick_counter(void)
{
    /*
     * Make sure we are not interrupted between invoking the capture task
     * and reading the value.
     */
    OS_ASSERT_CRITICAL();

#if MYNEWT_VAL(XTAL_32768)
    return OS_TICK_TIMER->COUNTER;
#else
    /*
     * Capture the current timer value and return it.
     */
    OS_TICK_TIMER->TASKS_CAPTURE[OS_TICK_COUNTER] = 1;
    return (OS_TICK_TIMER->CC[OS_TICK_COUNTER]);
#endif
}

static inline void
nrf52_os_tick_set_ocmp(uint32_t ocmp)
{
    uint32_t counter;

    OS_ASSERT_CRITICAL();
    while (1) {
#if MYNEWT_VAL(XTAL_32768)
        int delta;

        ocmp &= 0xffffff;
        OS_TICK_TIMER->CC[OS_TICK_CMPREG] = ocmp;
        counter = nrf52_os_tick_counter();
        /*
         * From nRF52 Product specification
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
#else
        OS_TICK_TIMER->CC[OS_TICK_CMPREG] = ocmp;
        counter = nrf52_os_tick_counter();
        if (TIMER_LT(counter, ocmp)) {
            break;
        }
#endif
        ocmp += g_hal_os_tick.ticks_per_ostick;
    }
}

static void
nrf52_timer_handler(void)
{
    int ticks;
    os_sr_t sr;
    uint32_t counter;

    os_trace_enter_isr();
    OS_ENTER_CRITICAL(sr);

    /* Calculate elapsed ticks and advance OS time. */
#if MYNEWT_VAL(XTAL_32768)
    int delta;

    counter = nrf52_os_tick_counter();
    delta = sub24(counter, g_hal_os_tick.lastocmp);
    ticks = delta / g_hal_os_tick.ticks_per_ostick;
    os_time_advance(ticks);

    /* Clear timer interrupt */
    OS_TICK_TIMER->EVENTS_COMPARE[OS_TICK_CMPREG] = 0;

    /* Update the time associated with the most recent tick */
    g_hal_os_tick.lastocmp = (g_hal_os_tick.lastocmp +
        (ticks * g_hal_os_tick.ticks_per_ostick)) & 0xffffff;
#else
    counter = nrf52_os_tick_counter();
    ticks = (counter - g_hal_os_tick.lastocmp) / g_hal_os_tick.ticks_per_ostick;
    os_time_advance(ticks);

    /* Clear timer interrupt */
    OS_TICK_TIMER->EVENTS_COMPARE[OS_TICK_CMPREG] = 0;

    /* Update the time associated with the most recent tick */
    g_hal_os_tick.lastocmp += ticks * g_hal_os_tick.ticks_per_ostick;
#endif

    /* Update the output compare to interrupt at the next tick */
    nrf52_os_tick_set_ocmp(g_hal_os_tick.lastocmp + g_hal_os_tick.ticks_per_ostick);

    OS_EXIT_CRITICAL(sr);
    os_trace_exit_isr();
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
        nrf52_os_tick_set_ocmp(ocmp);
    }

    __DSB();
    __WFI();

    if (ticks > 0) {
        /*
         * Update OS time before anything else when coming out of
         * the tickless regime.
         */
        nrf52_timer_handler();
    }
}

#if MYNEWT_VAL(XTAL_32768)
void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t sr;
    uint32_t mask;

    assert(RTC_FREQ % os_ticks_per_sec == 0);

    g_hal_os_tick.lastocmp = 0;
    g_hal_os_tick.ticks_per_ostick = RTC_FREQ / os_ticks_per_sec;

    /*
     * The maximum number of OS ticks allowed to elapse during idle is
     * limited to 1/4th the number of timer ticks before the 24-bit counter
     * rolls over.
     */
    g_hal_os_tick.max_idle_ticks = (1UL << 22) / g_hal_os_tick.ticks_per_ostick;

    /* Turn on the LFCLK */
    NRF_CLOCK->TASKS_LFCLKSTOP = 1;
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal;
    NRF_CLOCK->TASKS_LFCLKSTART = 1;

    /* Wait here till started! */
    mask = CLOCK_LFCLKSTAT_STATE_Msk | CLOCK_LFCLKSTAT_SRC_Xtal;
    while (1) {
        if (NRF_CLOCK->EVENTS_LFCLKSTARTED) {
            if ((NRF_CLOCK->LFCLKSTAT & mask) == mask) {
                break;
            }
        }
    }

    /* disable interrupts */
    OS_ENTER_CRITICAL(sr);

    /* Set isr in vector table and enable interrupt */
    NVIC_SetPriority(OS_TICK_IRQ, prio);
    NVIC_SetVector(OS_TICK_IRQ, (uint32_t)nrf52_timer_handler);
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
    OS_TICK_TIMER->CC[OS_TICK_CMPREG] = g_hal_os_tick.ticks_per_ostick;

    OS_TICK_TIMER->TASKS_START = 1;

    OS_EXIT_CRITICAL(sr);
}
#else
void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    g_hal_os_tick.ticks_per_ostick = 1000000 / os_ticks_per_sec;

    /*
     * The maximum number of timer ticks allowed to elapse during idle is
     * limited to 1/4th the number of timer ticks before the 32-bit counter
     * rolls over.
     */
    g_hal_os_tick.max_idle_ticks = (1UL << 30) / g_hal_os_tick.ticks_per_ostick;

    /*
     * Program OS_TICK_TIMER to operate at 1MHz and trigger an output
     * compare interrupt at a rate of 'os_ticks_per_sec'.
     */
    OS_TICK_TIMER->TASKS_STOP = 1;
    OS_TICK_TIMER->TASKS_CLEAR = 1;
    OS_TICK_TIMER->MODE = TIMER_MODE_MODE_Timer;
    OS_TICK_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    OS_TICK_TIMER->PRESCALER = OS_TICK_PRESCALER;

    OS_TICK_TIMER->CC[OS_TICK_CMPREG] = g_hal_os_tick.ticks_per_ostick;
    OS_TICK_TIMER->INTENSET = TIMER_COMPARE_INT_MASK(OS_TICK_CMPREG);
    OS_TICK_TIMER->EVENTS_COMPARE[OS_TICK_CMPREG] = 0;

    NVIC_SetPriority(OS_TICK_IRQ, prio);
    NVIC_SetVector(OS_TICK_IRQ, (uint32_t)nrf52_timer_handler);
    NVIC_EnableIRQ(OS_TICK_IRQ);

    OS_TICK_TIMER->TASKS_START = 1;     /* start the os tick timer */
}
#endif
