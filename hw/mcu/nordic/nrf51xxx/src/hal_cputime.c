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

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "bsp/cmsis_nvic.h"
#include "hal/hal_cputime.h"
#include "mcu/nrf51.h"
#include "mcu/nrf51_bitfields.h"
#include "mcu/nrf51_hal.h"

/* Maximum timer frequency */
#define NRF51_MAX_TIMER_FREQ    (16000000)

#undef HAL_CPUTIME_USE_OVERFLOW

/* The RF peripheral uses CC registers 0 and 1 for RF events. */
#define CPUTIMER                NRF_TIMER0
#define CPUTIMER_IRQ            (TIMER0_IRQn)
#define CPUTIMER_CC_CNTR        (2)
#ifdef HAL_CPUTIME_USE_OVERFLOW
#define CPUTIMER_CC_OVERFLOW    (2)
#endif
#define CPUTIMER_CC_INT         (3)

/* Interrupt mask for interrupt enable/clear */
#define CPUTIMER_INT_MASK(x)    ((1 << (uint32_t)(x)) << 16)

/* XXX:
 *  - Must determine how to set priority of cpu timer interrupt
 *  - Determine if we should use a mutex as opposed to disabling interrupts
 *  - Should I use macro for compare channel?
 *  - Sync to OSTIME.
 */

void
cputime_disable_ocmp(void)
{
    CPUTIMER->INTENCLR = CPUTIMER_INT_MASK(CPUTIMER_CC_INT);
}

/**
 * cputime set ocmp
 *
 * Set the OCMP used by the cputime module to the desired cputime.
 *
 * NOTE: Must be called with interrupts disabled.
 *
 * @param timer Pointer to timer.
 */
void
cputime_set_ocmp(struct cpu_timer *timer)
{
    /* Disable ocmp interrupt and set new value */
    cputime_disable_ocmp();

    /* Set output compare register to timer expiration */
    CPUTIMER->CC[CPUTIMER_CC_INT] = timer->cputime;

    /* Clear interrupt flag*/
    CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_INT] = 0;

    /* Enable the output compare interrupt */
    CPUTIMER->INTENSET = CPUTIMER_INT_MASK(CPUTIMER_CC_INT);

    /* Force interrupt to occur as we may have missed it */
    if ((int32_t)(cputime_get32() - timer->cputime) >= 0) {
        NVIC_SetPendingIRQ(CPUTIMER_IRQ);
    }
}

/**
 * cputime isr
 *
 * This is the global timer interrupt routine.
 *
 */
static void
cputime_isr(void)
{
    uint32_t compare;
#ifdef HAL_CPUTIME_USE_OVERFLOW
    uint32_t overflow;
#endif

    /* Check interrupt source. If set, clear them */
    compare = CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_INT];
    if (compare) {
        CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_INT] = 0;
    }

#ifdef HAL_CPUTIME_USE_OVERFLOW
    overflow = CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_OVERFLOW];
    if (overflow) {
        CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_OVERFLOW] = 0;
        ++g_cputime.uif_ints;
        ++g_cputime.cputime_high;
    }
#endif

    /* Count # of interrupts */
    ++g_cputime.timer_isrs;

    /*
     * NOTE: we dont check the 'compare' variable here due to how the timer
     * is implemented on this chip. There is no way to force an output
     * compare, so if we are late setting the output compare (i.e. the timer
     * counter is already passed the output compare value), we use the NVIC
     * to set a pending interrupt. This means that there will be no compare
     * flag set, so all we do is check to see if the compare interrupt is
     * enabled.
     */
    if (CPUTIMER->INTENCLR & CPUTIMER_INT_MASK(CPUTIMER_CC_INT)) {
        ++g_cputime.ocmp_ints;
        cputime_chk_expiration();

        /* XXX: Recommended by nordic to make sure interrupts are cleared */
        compare = CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_INT];
    }
}

/**
 * cputime init
 *
 * Initialize the cputime module. This must be called after os_init is called
 * and before any other timer API are used. This should be called only once
 * and should be called before the hardware timer is used.
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
    uint32_t pre_scaler;

    /* Clock frequency must be at least 1 MHz */
    if (clock_freq < 1000000U) {
        return -1;
    }

    /* Check if clock frequency exceeds max. range */
    max_freq = NRF51_MAX_TIMER_FREQ;
    if (clock_freq > max_freq) {
        return -1;
    }

    /* Is this exact frequency obtainable? */
    pre_scaler = max_freq / clock_freq;
    if ((pre_scaler * clock_freq) != max_freq) {
        return -1;
    }

    /*
     * Pre-scaler is 4 bits and is a 2^n, so the only possible values that
     * work are 1, 2, 4, 8 and 16, which gives a valid pre-scaler of 0, 1, 2,
     * 3 or 4.
     */
    switch (pre_scaler) {
    case 1:
        pre_scaler = 0;
        break;
    case 2:
        pre_scaler = 1;
        break;
    case 4:
        pre_scaler = 2;
        break;
    case 8:
        pre_scaler = 3;
        break;
    case 16:
        pre_scaler = 4;
        break;
    default:
        pre_scaler = 0xFFFFFFFF;
        break;
    }

    if (pre_scaler == 0xFFFFFFFF) {
        return -1;
    }

    /* disable interrupts */
    __HAL_DISABLE_INTERRUPTS(ctx);

    /* Set the clock frequency */
    g_cputime.ticks_per_usec = clock_freq / 1000000U;

    /* XXX: no way to halt the timer in debug mode that I can see */

    /* Stop the timer first */
    CPUTIMER->TASKS_STOP = 1;

    /* Put the timer in timer mode using 32 bits. */
    CPUTIMER->MODE = TIMER_MODE_MODE_Timer;
    CPUTIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;

    /* Set the pre-scaler*/
    CPUTIMER->PRESCALER = pre_scaler;

    /* Start the timer */
    CPUTIMER->TASKS_START = 1;

    /*  Use an output compare to generate an overflow */
#ifdef HAL_CPUTIME_USE_OVERFLOW
    CPUTIMER->CC[CPUTIMER_CC_OVERFLOW] = 0;
    CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_OVERFLOW] = 0;
    CPUTIMER->INTENSET = CPUTIMER_INT_MASK(CPUTIMER_CC_OVERFLOW);
#endif

    /* Set isr in vector table and enable interrupt */
    NVIC_SetVector(CPUTIMER_IRQ, (uint32_t)cputime_isr);
    NVIC_EnableIRQ(CPUTIMER_IRQ);

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
#ifdef HAL_CPUTIME_USE_OVERFLOW
uint64_t
cputime_get64(void)
{
    uint32_t ctx;
    uint32_t high;
    uint32_t low;
    uint64_t cpu_time;

    __HAL_DISABLE_INTERRUPTS(ctx);
    high = g_cputime.cputime_high;
    low = cputime_get32();
    if (CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_OVERFLOW]) {
        ++high;
        low = cputime_get32();
    }
    __HAL_ENABLE_INTERRUPTS(ctx);

    cpu_time = ((uint64_t)high << 32) | low;

    return cpu_time;
}
#endif

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
    uint32_t cpu_time;

    /* Force a capture of the timer into 'cntr' capture channel; read it */
    CPUTIMER->TASKS_CAPTURE[CPUTIMER_CC_CNTR] = 1;
    cpu_time = CPUTIMER->CC[CPUTIMER_CC_CNTR];

    return cpu_time;
}
