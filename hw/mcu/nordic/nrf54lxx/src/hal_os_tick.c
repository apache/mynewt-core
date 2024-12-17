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
#include <nrf_grtc.h>

/* The OS scheduler requires a low-frequency timer. */
#if MYNEWT_VAL(OS_SCHEDULING) && !MYNEWT_VAL(MCU_LFCLK_SOURCE)
#error The OS scheduler requires a low-frequency timer; configure MCU_LFCLK_SOURCE
#endif

#define GRTC_FREQ           1000000       /* in Hz */
#define GRTC_COMPARE_INT_MASK(ccreg) (1UL << (ccreg))

/*
 * Use two compare channels - one for os_tick and one to wake up from idle state.
 * This way we can utilize INTERVAL register used with CC[0] for OS ticks. To wake up from
 * idle state CC[1] channel is used.
 */
#define OS_TICK_CMPREG      0
#define OS_TICK_CMPEV       NRF_GRTC_EVENT_COMPARE_0
#define OS_IDLE_CMPREG      1
#define OS_IDLE_CMPEV       NRF_GRTC_EVENT_COMPARE_1

struct hal_os_tick {
    int ticks_per_ostick;
    os_time_t max_idle_ticks;
    uint64_t lastocmp;
};

struct hal_os_tick g_hal_os_tick;

static inline uint64_t
nrf54l_os_tick_counter(void)
{
    uint32_t counterl_val, counterh_val, counterh;
    uint64_t counter;

    do {
        counterl_val = nrf_grtc_sys_counter_low_get(NRF_GRTC);
        counterh = nrf_grtc_sys_counter_high_get(NRF_GRTC);
        counterh_val = counterh & NRF_GRTC_SYSCOUNTERH_VALUE_MASK;
    } while (counterh & NRF_GRTC_SYSCOUNTERH_BUSY_MASK);

    if (counterh & NRF_GRTC_SYSCOUNTERH_OVERFLOW_MASK) {
        --counterh_val;
    }

    counter = ((uint64_t)counterh_val << 32) | counterl_val;
    return counter;
}

static inline void
nrf54l_os_idle_set_ocmp(uint64_t ocmp)
{
    int delta;
    uint64_t counter;

    OS_ASSERT_CRITICAL();
    while (1) {
        nrf_grtc_sys_counter_cc_set(NRF_GRTC, OS_IDLE_CMPREG, ocmp);
        counter = nrf54l_os_tick_counter();

        delta = ocmp - counter;
        if (delta > 0) {
            break;
        }
        ocmp += g_hal_os_tick.ticks_per_ostick;
    }
}

static void
nrf54l_timer_handler(void)
{
    int delta;
    int ticks;
    os_sr_t sr;
    uint64_t counter;

    os_trace_isr_enter();
    OS_ENTER_CRITICAL(sr);

    /* Calculate elapsed ticks and advance OS time. */
    counter = nrf54l_os_tick_counter();
    delta = counter - g_hal_os_tick.lastocmp;
    ticks = delta / g_hal_os_tick.ticks_per_ostick;
    os_time_advance(ticks);

    /* Clear timer interrupt */
    nrf_grtc_event_clear(NRF_GRTC, OS_TICK_CMPEV);
    nrf_grtc_event_clear(NRF_GRTC, OS_IDLE_CMPEV);

    /* Update the time associated with the most recent tick */
    g_hal_os_tick.lastocmp = (g_hal_os_tick.lastocmp +
                              (ticks * g_hal_os_tick.ticks_per_ostick));

    OS_EXIT_CRITICAL(sr);
    os_trace_isr_exit();
}

void
os_tick_idle(os_time_t ticks)
{
    uint64_t ocmp;

    OS_ASSERT_CRITICAL();

    if (ticks > 0) {
        /*
         * Enter tickless regime during long idle durations.
         */
        if (ticks > g_hal_os_tick.max_idle_ticks) {
            ticks = g_hal_os_tick.max_idle_ticks;
        }

        /* Disable OS tick interrupt */
        nrf_grtc_int_disable(NRF_GRTC, GRTC_COMPARE_INT_MASK(OS_TICK_CMPREG));

        /* Set ocmp for wake up */
        ocmp = g_hal_os_tick.lastocmp + (ticks * g_hal_os_tick.ticks_per_ostick);
        nrf54l_os_idle_set_ocmp(ocmp);
    }

    __DSB();
    __WFI();

    if (ticks > 0) {
        /*
         * Update OS time and re-enable OS tick interrupt before anything else
         * when coming out of the tickless regime.
         */
        nrf_grtc_int_enable(NRF_GRTC, GRTC_COMPARE_INT_MASK(OS_TICK_CMPREG));
        nrf54l_timer_handler();
    }
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t sr;

    assert(GRTC_FREQ % os_ticks_per_sec == 0);

    g_hal_os_tick.lastocmp = 0;
    g_hal_os_tick.ticks_per_ostick = GRTC_FREQ / os_ticks_per_sec;

    /* XXX: selected this value by intuition, trying to make it roughly appropriate */
    g_hal_os_tick.max_idle_ticks = (1UL << 27) / g_hal_os_tick.ticks_per_ostick;

    /* disable interrupts */
    OS_ENTER_CRITICAL(sr);

    /* Set isr in vector table and enable interrupt */
    NVIC_SetPriority(GRTC_IRQn, prio);
    NVIC_SetVector(GRTC_IRQn, (uint32_t)nrf54l_timer_handler);
    NVIC_EnableIRQ(GRTC_IRQn);

    /*
     * Program the NRF_GRTC to trigger an interrupt
     * at an interval of 'ticks_per_ostick' and initialize CC channel
     * for waking up from idle state.
     */
    nrf_grtc_task_trigger(NRF_GRTC, NRF_GRTC_TASK_STOP);
    nrf_grtc_task_trigger(NRF_GRTC, NRF_GRTC_TASK_CLEAR);

    nrf_grtc_event_disable(NRF_GRTC, 0xffffffff);

    nrf_grtc_int_disable(NRF_GRTC, 0xffffffff);
    nrf_grtc_int_enable(NRF_GRTC, GRTC_COMPARE_INT_MASK(OS_TICK_CMPREG));
    nrf_grtc_int_enable(NRF_GRTC, GRTC_COMPARE_INT_MASK(OS_IDLE_CMPREG));

    nrf_grtc_clksel_set(NRF_GRTC, NRF_GRTC_CLKSEL_LFXO);

    nrf_grtc_event_clear(NRF_GRTC, OS_TICK_CMPEV);
    nrf_grtc_event_clear(NRF_GRTC, OS_IDLE_CMPEV);
    nrf_grtc_sys_counter_compare_event_enable(NRF_GRTC, OS_TICK_CMPREG);
    nrf_grtc_sys_counter_compare_event_enable(NRF_GRTC, OS_IDLE_CMPREG);

    nrf_grtc_sys_counter_interval_set(NRF_GRTC, g_hal_os_tick.ticks_per_ostick);
    nrf_grtc_sys_counter_set(NRF_GRTC, true);
    nrf_grtc_task_trigger(NRF_GRTC, NRF_GRTC_TASK_START);

    OS_EXIT_CRITICAL(sr);
}
