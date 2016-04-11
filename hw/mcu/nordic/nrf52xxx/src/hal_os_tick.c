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
#include <assert.h>
#include <os/os.h>
#include <hal/hal_os_tick.h>
#include <mcu/nrf52_bitfields.h>
#include <bsp/cmsis_nvic.h>

#define OS_TICK_TIMER       NRF_TIMER1
#define OS_TICK_IRQ         TIMER1_IRQn
#define OS_TICK_CMPREG      0   /* generate timer interrupt */
#define OS_TICK_COUNTER     1   /* capture current timer value */
#define OS_TICK_PRESCALER   4   /* prescaler to generate 1MHz timer freq */
#define TIMER_LT(__t1, __t2)    ((int32_t)((__t1) - (__t2)) < 0)

static int timer_ticks_per_ostick;
static os_time_t nrf52_max_idle_ticks;
static uint32_t lastocmp;

static inline uint32_t
nrf52_os_tick_counter(void)
{
    /*
     * Make sure we are not interrupted between invoking the capture task
     * and reading the value.
     */
    OS_ASSERT_CRITICAL();

    /*
     * Capture the current timer value and return it.
     */
    OS_TICK_TIMER->TASKS_CAPTURE[OS_TICK_COUNTER] = 1;
    return (OS_TICK_TIMER->CC[OS_TICK_COUNTER]);
}

static inline void
nrf52_os_tick_set_ocmp(uint32_t ocmp)
{
    uint32_t counter;

    OS_ASSERT_CRITICAL();
    while (1) {
        OS_TICK_TIMER->CC[OS_TICK_CMPREG] = ocmp;
        counter = nrf52_os_tick_counter();
        if (TIMER_LT(counter, ocmp)) {
            break;
        }
        ocmp += timer_ticks_per_ostick;
    }
}

static void
nrf52_timer_handler(void)
{
    int ticks;
    os_sr_t sr;
    uint32_t counter;

    OS_ENTER_CRITICAL(sr);

    /*
     * Calculate elapsed ticks and advance OS time.
     */
    counter = nrf52_os_tick_counter();
    ticks = (counter - lastocmp) / timer_ticks_per_ostick;
    os_time_advance(ticks);

    /* Clear timer interrupt */
    OS_TICK_TIMER->EVENTS_COMPARE[OS_TICK_CMPREG] = 0;

    /* Update the time associated with the most recent tick */
    lastocmp += ticks * timer_ticks_per_ostick;

    /* Update the output compare to interrupt at the next tick */
    nrf52_os_tick_set_ocmp(lastocmp + timer_ticks_per_ostick);

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
        if (ticks > nrf52_max_idle_ticks) {
            ticks = nrf52_max_idle_ticks;
        }
        ocmp = lastocmp + ticks * timer_ticks_per_ostick;
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

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    lastocmp = 0;
    timer_ticks_per_ostick = 1000000 / os_ticks_per_sec;

    /*
     * The maximum number of timer ticks allowed to elapse during idle is
     * limited to 1/4th the number of timer ticks before the 32-bit counter
     * rolls over.
     */
    nrf52_max_idle_ticks = (1UL << 30) / timer_ticks_per_ostick;

    /*
     * Program OS_TICK_TIMER to operate at 1MHz and trigger an output
     * compare interrupt at a rate of 'os_ticks_per_sec'.
     */
    OS_TICK_TIMER->TASKS_STOP = 1;
    OS_TICK_TIMER->TASKS_CLEAR = 1;
    OS_TICK_TIMER->MODE = TIMER_MODE_MODE_Timer;
    OS_TICK_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    OS_TICK_TIMER->PRESCALER = OS_TICK_PRESCALER;

    OS_TICK_TIMER->CC[OS_TICK_CMPREG] = timer_ticks_per_ostick;
    OS_TICK_TIMER->INTENSET = TIMER_COMPARE_INT_MASK(OS_TICK_CMPREG);
    OS_TICK_TIMER->EVENTS_COMPARE[OS_TICK_CMPREG] = 0;

    NVIC_SetPriority(OS_TICK_IRQ, prio);
    NVIC_SetVector(OS_TICK_IRQ, (uint32_t)nrf52_timer_handler);
    NVIC_EnableIRQ(OS_TICK_IRQ);

    OS_TICK_TIMER->TASKS_START = 1;     /* start the os tick timer */
}
