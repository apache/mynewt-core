/**
 * Copyright (c) 2015 Stack Inc.
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
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "bsp/cmsis_nvic.h"
#include "hal/hal_cputime.h"
#include "mcu/nrf52.h"
#include "mcu/nrf52_bitfields.h"
#include "mcu/nrf52_hal.h"

/* Maximum timer frequency */
#define NRF52_MAX_TIMER_FREQ    (16000000)

/* Use these defines to select a timer and the compare channels */
#define CPUTIMER                NRF_TIMER0
#define CPUTIMER_IRQ            (TIMER0_IRQn)
#define CPUTIMER_CC_CNTR        (0)
#define CPUTIMER_CC_OVERFLOW    (1)
#define CPUTIMER_CC_INT         (2)

/* Interrupt mask for interrupt enable/clear */
#define CPUTIMER_INT_MASK(x)    ((1 << (uint32_t)(x)) << 16)

/* XXX:
 *  - Must determine how to set priority of cpu timer interrupt
 *  - Determine if we should use a mutex as opposed to disabling interrupts
 *  - Should I use macro for compare channel?
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

__STATIC_INLINE void
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
static void
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
        cputime_disable_ocmp();
    }
    __HAL_ENABLE_INTERRUPTS(ctx);
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
    uint32_t overflow;

    /* Check interrupt source. If set, clear them */
    compare = CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_INT];
    if (compare) {
        CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_INT] = 0;
    }
    overflow = CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_OVERFLOW];
    if (overflow) {
        CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_OVERFLOW] = 0;
    }

    /* Count # of interrupts */
    ++g_cputime.timer_isrs;

    /* If overflow, increment high word of cpu time */
    if (overflow) {
        ++g_cputime.uif_ints;
        ++g_cputime.cputime_high;
    }

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
cputime_init(uint32_t clock_freq)
{
    uint32_t ctx;
    uint32_t max_freq;
    uint32_t pre_scaler;

    /* Clock frequency must be at least 1 MHz */
    if (clock_freq < 1000000U) {
        return -1;
    }

    /* Check if clock frequency exceeds max. range */
    max_freq = NRF52_MAX_TIMER_FREQ;
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

    /* Initialize the timer queue */
    TAILQ_INIT(&g_cputimer_q);

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
    CPUTIMER->CC[CPUTIMER_CC_OVERFLOW] = 0;
    CPUTIMER->EVENTS_COMPARE[CPUTIMER_CC_OVERFLOW] = 0;
    CPUTIMER->INTENSET = CPUTIMER_INT_MASK(CPUTIMER_CC_OVERFLOW);

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
                cputime_disable_ocmp();
            }
        }
    }

    __HAL_ENABLE_INTERRUPTS(ctx);
}

