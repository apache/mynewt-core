/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mcu/cortex_m0.h>
#include "hal/hal_system.h"
/* XXX */
#include "bsp/cmsis_nvic.h"
#include "mcu/nrf51.h"
#include "mcu/nrf51_bitfields.h"
#include "mcu/nrf51_hal.h"
/* XXX */

void
system_reset(void)
{
    while (1) {
        NVIC_SystemReset();
    }
}

/* XXX*/
extern void timer_handler(void);

static void
rtc0_timer_handler(void)
{
    if (NRF_RTC0->EVENTS_TICK) {
        NRF_RTC0->EVENTS_TICK = 0;
        timer_handler();
    }
}

void
system_os_tick_init(uint32_t os_ticks_per_sec)
{
    uint32_t ctx;
    uint32_t mask;
    uint32_t pre_scaler;

    /* Turn on the LFCLK */
    NRF_CLOCK->XTALFREQ = CLOCK_XTALFREQ_XTALFREQ_16MHz;
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

    /* Is this exact frequency obtainable? */
    pre_scaler = (32768 / os_ticks_per_sec) - 1;

    /* disable interrupts */
    __HAL_DISABLE_INTERRUPTS(ctx);

    NRF_RTC0->TASKS_STOP = 1;
    NRF_RTC0->EVENTS_TICK = 0;
    NRF_RTC0->PRESCALER = pre_scaler;
    NRF_RTC0->INTENCLR = 0xffffffff;
    NRF_RTC0->TASKS_CLEAR = 1;

    /* Set isr in vector table and enable interrupt */
    NVIC_SetVector(RTC0_IRQn, (uint32_t)rtc0_timer_handler);
    NVIC_EnableIRQ(RTC0_IRQn);

    NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Msk;
    NRF_RTC0->TASKS_START = 1;

    __HAL_ENABLE_INTERRUPTS(ctx);
}

