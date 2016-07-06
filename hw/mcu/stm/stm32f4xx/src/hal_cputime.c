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
#include <stdint.h>
#include <assert.h>
#include "bsp/cmsis_nvic.h"
#include "hal/hal_cputime.h"
#include "mcu/stm32f4xx.h"
#include "mcu/stm32f4xx_hal_rcc.h"

/* XXX:
 *  - Must determine how to set priority of cpu timer interrupt
 *  - Determine if we should use a mutex as opposed to disabling interrupts
 *  - Should I use a macro for the timer being used? This is so I can
 *  easily change the timer from 2 to 5? What about compare channel?
 *  - Sync to OSTIME.
 */

void
cputime_disable_ocmp(void)
{
    TIM5->DIER &= ~TIM_DIER_CC4IE;
}

/**
 * cputime set ocmp
 *
 * Set the OCMP used by the cputime module to the desired cputime.
 *
 * @param timer Pointer to timer.
 */
void
cputime_set_ocmp(struct cpu_timer *timer)
{
    TIM5->DIER &= ~TIM_DIER_CC4IE;
    TIM5->CCR4 = timer->cputime;
    TIM5->SR = ~TIM_SR_CC4IF;
    TIM5->DIER |= TIM_DIER_CC4IE;
    if ((int32_t)(TIM5->CNT - timer->cputime) >= 0) {
        /* Force interrupt to occur as we may have missed it */
        TIM5->EGR = TIM_EGR_CC4G;
    }
}

/**
 * tim5 isr
 *
 * This is the global timer interrupt routine.
 *
 */
static void
cputime_isr(void)
{
    uint32_t sr;

    /* Clear the interrupt sources */
    sr = TIM5->SR;
    TIM5->SR = ~sr;

    /* Count # of interrupts */
    ++g_cputime.timer_isrs;

    /* If overflow, increment high word of cpu time */
    if (sr & TIM_SR_UIF) {
        ++g_cputime.uif_ints;
        ++g_cputime.cputime_high;
    }

    /* Check if output compare occurred */
    if (sr & TIM_SR_CC4IF) {
        if (TIM5->DIER & TIM_DIER_CC4IE) {
            ++g_cputime.ocmp_ints;
            cputime_chk_expiration();
        }
    }
}

/**
 * cputime hw init
 *
 * Initialize the cputime hw. This should be called only once and should be
 * called before the hardware timer is used.
 *
 * @param clock_freq The desired cputime frequency, in hertz (Hz).
 *
 * @return int 0 on success; -1 on error.
 */
int
cputime_hw_init(uint32_t clock_freq)
{
    uint32_t ctx;
    uint32_t max_freq;
    uint32_t pre_scalar;

    /* Clock frequency must be at least 1 MHz */
    if (clock_freq < 1000000U) {
        return -1;
    }

    /* Check if clock frequency exceeds max. range */
    max_freq = SystemCoreClock / 2;
    if (clock_freq > max_freq) {
        return -1;
    }

    /* Is this exact frequency obtainable? */
    pre_scalar = max_freq / clock_freq;
    if ((pre_scalar * clock_freq) != max_freq) {
        return -1;
    }

    /* disable interrupts */
    __HAL_DISABLE_INTERRUPTS(ctx);

    /* Set the clock frequency */
    g_cputime.ticks_per_usec = clock_freq / 1000000U;

    /* XXX: what about timer reset? */

    /* Enable the timer in the peripheral enable regiseter */
    __HAL_RCC_TIM5_CLK_ENABLE();

    /* In debug mode, we want this timer to be halted */
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM5_STOP;

    /*
     * Counter is an up counter with event generation disabled. We disable the
     * timer with this first write, just in case.
     */
    TIM5->DIER = 0;
    TIM5->CR1 = 0;
    TIM5->CR2 = 0;
    TIM5->SMCR = 0;

    /* Configure compare 4 mode register */
    TIM5->CCMR2 = TIM5->CCMR2 & 0xFF;

    /* Set the auto-reload to 0xFFFFFFFF */
    TIM5->ARR = 0xFFFFFFFF;

    /* Set the pre-scalar and load it */
    TIM5->PSC= pre_scalar - 1;
    TIM5->EGR |= TIM_EGR_UG;

    /* Clear overflow and compare interrupt flags */
    TIM5->SR = ~(TIM_SR_CC4IF | TIM_SR_UIF);

    /* Set isr in vector table and enable interrupt */
    NVIC_SetVector(TIM5_IRQn, (uint32_t)cputime_isr);
    NVIC_EnableIRQ(TIM5_IRQn);

    /* Enable overflow interrupt */
    TIM5->DIER = TIM_DIER_UIE;

    /* XXX: If we want to sync to os time, we can read SysTick and set
     the timer counter based on the Systick counter and current os time */
    /* Clear the counter (just in case) */
    TIM5->CNT = 0;

    /* Enable the timer */
    TIM5->CR1 = TIM_CR1_URS | TIM_CR1_CEN;

    __HAL_ENABLE_INTERRUPTS(ctx);

    return 0;
}

/**
 * cputime get64
 *
 * Returns cputime as a 64-bit number.
 *
 * @return uint64_t The 64-bit representation of cputime.
 */
uint64_t
cputime_get64(void)
{
    uint32_t ctx;
    uint32_t high;
    uint32_t low;
    uint64_t cpu_time;

    __HAL_DISABLE_INTERRUPTS(ctx);
    high = g_cputime.cputime_high;
    low = TIM5->CNT;
    if (TIM5->SR & TIM_SR_UIF) {
        ++high;
        low = TIM5->CNT;
    }
    __HAL_ENABLE_INTERRUPTS(ctx);

    cpu_time = ((uint64_t)high << 32) | low;

    return cpu_time;
}

/**
 * cputime get32
 *
 * Returns the low 32 bits of cputime.
 *
 * @return uint32_t The lower 32 bits of cputime
 */
uint32_t
cputime_get32(void)
{
    return TIM5->CNT;
}
