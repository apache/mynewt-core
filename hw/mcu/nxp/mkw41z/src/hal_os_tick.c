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
#include <mcu/cmsis_nvic.h>
//#include <mkw41z4.h>
#include <mcu/mkw41z_hal.h>

/* Timer frequency used for os tick */
#define MKW41Z_LPTMR_FREQ   (1000)      /* in Hz */

static uint16_t timer_ticks_per_ostick;

/*
 * LPTMR irq handler
 *
 * This IRQ handles OS time. Currently, this MCU does not have the tickless
 * OS implemented. It is also possible to miss OS ticks if interrupts are
 * disabled for too long (longer than one tick).
 */
static void
mkw41z_os_tick_handler(void)
{
    uint32_t csr;
    uint32_t sr;

    OS_ENTER_CRITICAL(sr);

    /* Must make sure flag is set when we get interrupt */
    csr = LPTMR0->CSR;
    if (csr & LPTMR_CSR_TCF_MASK) {
        /* Advance os time */
        os_time_advance(1);

        /* Clear mask */
        LPTMR0->CSR = csr;
    }

    OS_EXIT_CRITICAL(sr);
}

void
os_tick_idle(os_time_t ticks)
{
    OS_ASSERT_CRITICAL();

    __DSB();
    __WFI();
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t ctx;
    uint32_t prescaler_reg;

    /* Make os ticks per sec divides evenly into frequency */
    timer_ticks_per_ostick = MKW41Z_LPTMR_FREQ / os_ticks_per_sec;
    assert((timer_ticks_per_ostick * os_ticks_per_sec) == MKW41Z_LPTMR_FREQ);

    /* disable interrupts */
    __HAL_DISABLE_INTERRUPTS(ctx);

    /* Enable access to LPTMR module. LPTMR is bit 0 */
    SIM->SCGC5 |= 1;

    /* Make sure timer is disabled */
    LPTMR0->CSR = 0;

    /* Set isr in vector table and enable interrupt */
    NVIC_SetPriority(LPTMR0_IRQn, prio);
    NVIC_SetVector(LPTMR0_IRQn, (uint32_t)mkw41z_os_tick_handler);
    NVIC_EnableIRQ(LPTMR0_IRQn);

    /* Set prescaler register. Bypass prescalar and use LPO (clock 1) */
    prescaler_reg = LPTMR_PSR_PBYP_MASK | 1;
    LPTMR0->PSR = prescaler_reg;

    /* Write output compare while disabled */
    LPTMR0->CMR = timer_ticks_per_ostick - 1;

    /* Enable the timer: note you cannot alter bits 5 to 1 (inclusive) */
    LPTMR0->CSR = LPTMR_CSR_TIE_MASK | LPTMR_CSR_TEN_MASK;

    __HAL_ENABLE_INTERRUPTS(ctx);
}
