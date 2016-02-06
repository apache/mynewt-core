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

/* CPUTIME data */
struct cputime_data
{
    uint32_t ticks_per_usec;    /* number of ticks per usec */
    uint32_t cputime_high;      /* high word of 64-bit cpu time */
    uint32_t timer_isrs;        /* Number of timer interrupts */
    uint32_t ocmp_ints;         /* Number of ocmp interrupts */
    uint32_t uif_ints;          /* Number of overflow interrupts */
};
struct cputime_data g_cputime;

/* Queue for timers */
TAILQ_HEAD(cputime_qhead, cpu_timer) g_cputimer_q;

/**
 * cputime set ocmp 
 *  
 * Set the OCMP used by the cputime module to the desired cputime. 
 * 
 * @param timer Pointer to timer.
 */
static void
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
 * cputime chk expiration 
 *  
 * Iterates through the cputimer queue to determine if any timers have expired. 
 * If the timer has expired the timer is removed from the queue and the timer 
 * callback function is executed. 
 * 
 */
static void
cputime_chk_expiration(void)
{
    uint32_t ctx;
    struct cpu_timer *timer;

    __HAL_DISABLE_INTERRUPTS(ctx);
    while ((timer = TAILQ_FIRST(&g_cputimer_q)) != NULL) {
        if ((int32_t)(cputime_get32() - timer->cputime) >= 0) {
            TAILQ_REMOVE(&g_cputimer_q, timer, link);
            timer->cb(timer->arg);
        } else {
            break;
        }
    }

    /* Any timers left on queue? If so, we need to set OCMP */
    timer = TAILQ_FIRST(&g_cputimer_q);
    if (timer) {
        cputime_set_ocmp(timer);
    } else {
        TIM5->DIER &= ~TIM_DIER_CC4IE;
    }
    __HAL_ENABLE_INTERRUPTS(ctx);
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
cputime_init(uint32_t clock_freq)
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

    /* Initialize the timer queue */
    TAILQ_INIT(&g_cputimer_q);

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

/**
 * cputime nsecs to ticks 
 *  
 * Converts the given number of nanoseconds into cputime ticks. 
 * 
 * @param usecs The number of nanoseconds to convert to ticks
 * 
 * @return uint32_t The number of ticks corresponding to 'nsecs'
 */
uint32_t 
cputime_nsecs_to_ticks(uint32_t nsecs)
{
    uint32_t ticks;

    ticks = ((nsecs * g_cputime.ticks_per_usec) + 999) / 1000;
    return ticks;
}

/**
 * cputime ticks to nsecs
 *  
 * Convert the given number of ticks into nanoseconds. 
 * 
 * @param ticks The number of ticks to convert to nanoseconds.
 * 
 * @return uint32_t The number of nanoseconds corresponding to 'ticks'
 */
uint32_t 
cputime_ticks_to_nsecs(uint32_t ticks)
{
    uint32_t nsecs;

    nsecs = ((ticks * 1000) + (g_cputime.ticks_per_usec - 1)) / 
            g_cputime.ticks_per_usec;

    return nsecs;
}

/**
 * cputime usecs to ticks 
 *  
 * Converts the given number of microseconds into cputime ticks. 
 * 
 * @param usecs The number of microseconds to convert to ticks
 * 
 * @return uint32_t The number of ticks corresponding to 'usecs'
 */
uint32_t 
cputime_usecs_to_ticks(uint32_t usecs)
{
    uint32_t ticks;

    ticks = (usecs * g_cputime.ticks_per_usec);
    return ticks;
}

/**
 * cputime ticks to usecs
 *  
 * Convert the given number of ticks into microseconds. 
 * 
 * @param ticks The number of ticks to convert to microseconds.
 * 
 * @return uint32_t The number of microseconds corresponding to 'ticks'
 */
uint32_t 
cputime_ticks_to_usecs(uint32_t ticks)
{
    uint32_t us;

    us =  (ticks + (g_cputime.ticks_per_usec - 1)) / g_cputime.ticks_per_usec;
    return us;
}

/**
 * cputime delay ticks
 *  
 * Wait until the number of ticks has elapsed. This is a blocking delay. 
 * 
 * @param ticks The number of ticks to wait.
 */
void 
cputime_delay_ticks(uint32_t ticks)
{
    uint32_t until;

    until = cputime_get32() + ticks;
    while ((int32_t)(cputime_get32() - until) < 0) {
        /* Loop here till finished */
    }
}

/**
 * cputime delay nsecs 
 *  
 * Wait until 'nsecs' nanoseconds has elapsed. This is a blocking delay. 
 *  
 * @param nsecs The number of nanoseconds to wait.
 */
void 
cputime_delay_nsecs(uint32_t nsecs)
{
    uint32_t ticks;

    ticks = cputime_nsecs_to_ticks(nsecs);
    cputime_delay_ticks(ticks);
}

/**
 * cputime delay usecs 
 *  
 * Wait until 'usecs' microseconds has elapsed. This is a blocking delay. 
 *  
 * @param usecs The number of usecs to wait.
 */
void 
cputime_delay_usecs(uint32_t usecs)
{
    uint32_t ticks;

    ticks = cputime_usecs_to_ticks(usecs);
    cputime_delay_ticks(ticks);
}

/**
 * cputime timer init
 * 
 * 
 * @param timer The timer to initialize. Cannot be NULL.
 * @param fp    The timer callback function. Cannot be NULL.
 * @param arg   Pointer to data object to pass to timer. 
 */
void 
cputime_timer_init(struct cpu_timer *timer, cputimer_func fp, void *arg)
{
    assert(timer != NULL);
    assert(fp != NULL);

    timer->cb = fp;
    timer->arg = arg;
    timer->link.tqe_prev = (void *) NULL;
}

/**
 * cputime timer start 
 *  
 * Start a cputimer that will expire at 'cputime'. If cputime has already 
 * passed, the timer callback will still be called (at interrupt context). 
 * 
 * @param timer     Pointer to timer to start. Cannot be NULL.
 * @param cputime   The cputime at which the timer should expire.
 */
void 
cputime_timer_start(struct cpu_timer *timer, uint32_t cputime)
{
    struct cpu_timer *entry;
    uint32_t ctx;

    assert(timer != NULL);

    /* XXX: should this use a mutex? not sure... */
    __HAL_DISABLE_INTERRUPTS(ctx);

    timer->cputime = cputime;
    if (TAILQ_EMPTY(&g_cputimer_q)) {
        TAILQ_INSERT_HEAD(&g_cputimer_q, timer, link);
    } else {
        TAILQ_FOREACH(entry, &g_cputimer_q, link) {
            if ((int32_t)(timer->cputime - entry->cputime) < 0) {
                TAILQ_INSERT_BEFORE(entry, timer, link);   
                break;
            }
        }
        if (!entry) {
            TAILQ_INSERT_TAIL(&g_cputimer_q, timer, link);
        }
    }

    /* If this is the head, we need to set new OCMP */
    if (timer == TAILQ_FIRST(&g_cputimer_q)) {
        cputime_set_ocmp(timer);
    }

    __HAL_ENABLE_INTERRUPTS(ctx);
}

/**
 * cputimer timer relative 
 *  
 * Sets a cpu timer that will expire 'usecs' microseconds from the current 
 * cputime. 
 * 
 * @param timer Pointer to timer. Cannot be NULL.
 * @param usecs The number of usecs from now at which the timer will expire.
 */
void 
cputime_timer_relative(struct cpu_timer *timer, uint32_t usecs)
{
    uint32_t cputime;

    assert(timer != NULL);

    cputime = cputime_get32() + cputime_usecs_to_ticks(usecs);
    cputime_timer_start(timer, cputime);
}

/**
 * cputime timer stop 
 *  
 * Stops a cputimer from running. The timer is removed from the timer queue 
 * and interrupts are disabled if no timers are left on the queue. Can be 
 * called even if timer is running. 
 * 
 * @param timer Pointer to cputimer to stop. Cannot be NULL.
 */
void 
cputime_timer_stop(struct cpu_timer *timer)
{
    int reset_ocmp;
    uint32_t ctx;
    struct cpu_timer *entry;

    assert(timer != NULL);

    __HAL_DISABLE_INTERRUPTS(ctx);

    /* If first on queue, we will need to reset OCMP */
    if (timer->link.tqe_prev != NULL) {
        reset_ocmp = 0;
        if (timer == TAILQ_FIRST(&g_cputimer_q)) {
            entry = TAILQ_NEXT(timer, link);
            reset_ocmp = 1;
        }
        TAILQ_REMOVE(&g_cputimer_q, timer, link);
        if (reset_ocmp) {
            if (entry) {
                cputime_set_ocmp(entry);
            } else {
                TIM5->DIER &= ~TIM_DIER_CC4IE;
            }
        }
    }

    __HAL_ENABLE_INTERRUPTS(ctx);
}

