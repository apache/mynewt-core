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

#include <stdlib.h>
#include <assert.h>

#include "os/mynewt.h"
#include "hal/hal_timer.h"
#include "mcu/cmsis_nvic.h"

#include "fsl_lptmr.h"

#define KINETIS_TIMERS_MAX 2

#if (FSL_FEATURE_LPTMR_CNR_WIDTH_IS_32B != 0)
#error "The Kinetis hal_timer driver currently only supports 16-bit timers"
#endif

#if MYNEWT_VAL(TIMER_1) && (KINETIS_TIMERS_MAX < 2)
#error "This MCU does not support TIMER_1"
#endif

/*
 * This value for MCGIRCLK matches the current configuration for Kinetis BSPs
 * where the slow clock is used and the divider is set to 1 (FCRDIV==0).
 */
#define MCGIRCLK_HZ 32768

struct kinetis_hal_tmr {
    LPTMR_Type *base;
    uint16_t overflow; /* MSB of the current counter is handled manually */
    uint16_t last_irq_cnt; /* Last counter on the interrupt handler */
    TAILQ_HEAD(hal_timer_qhead, hal_timer) hal_timer_q;
};

#if MYNEWT_VAL(TIMER_0)
struct kinetis_hal_tmr kinetis_tmr0 = {
    .base = LPTMR0,
};
#endif
#if MYNEWT_VAL(TIMER_1)
struct kinetis_hal_tmr kinetis_tmr1 = {
    .base = LPTMR1,
};
#endif

static const IRQn_Type kinetis_timer_irqs[] = LPTMR_IRQS;

static const struct kinetis_hal_tmr *kinetis_timers[KINETIS_TIMERS_MAX] = {
#if MYNEWT_VAL(TIMER_0)
    &kinetis_tmr0,
#else
    NULL,
#endif
#if MYNEWT_VAL(TIMER_1)
    &kinetis_tmr1,
#else
    NULL,
#endif
};

static uint32_t
kinetis_tmr_read(const struct kinetis_hal_tmr *tmr)
{
    return ((uint32_t)(tmr->overflow << 16) + LPTMR_GetCurrentTimerCount(tmr->base));
}

static void
kinetis_tmr_set_period(struct kinetis_hal_tmr *tmr, uint32_t tick)
{
    /* CMR cannot be zero, so add one cycle */
    if ((tick & 0xffff) == 0) {
        tick++;
    }

    /* XXX will only set the lower 16-bits */
    LPTMR_SetTimerPeriod(tmr->base, tick);
}

static uint32_t
kinetis_tmr_get_freq(const struct kinetis_hal_tmr *tmr)
{
    uint32_t psr;
    uint32_t freq;

    psr = tmr->base->PSR;
    switch (psr & LPTMR_PSR_PCS_MASK) {
    case 0:
        freq = MCGIRCLK_HZ;
        /* if the clock divisor is not bypassed, apply the divider */
        if (!(psr & LPTMR_PSR_PBYP_MASK)) {
            freq >>= ((psr & LPTMR_PSR_PRESCALE_MASK) >> LPTMR_PSR_PRESCALE_SHIFT) + 1;
        }
        break;
    default:
        /* Only MCGIRCLK is supported at the moment */
        assert(0);
    }

    return freq;
}

static void
kinetis_tmr_config_freq(lptmr_config_t *config, uint32_t freq_hz)
{
    uint32_t tmr_freq;
    uint8_t prescaler;

    config->prescalerClockSource = kLPTMR_PrescalerClock_0;

    tmr_freq = MCGIRCLK_HZ;
    prescaler = 0xff; /* overflow on purpose */
    while (freq_hz < tmr_freq) {
        tmr_freq /= 2;
        prescaler++;
        /* avoid a second overflow */
        if (prescaler == 0xff) {
            break;
        }
    }

    if (tmr_freq != MCGIRCLK_HZ) {
        config->bypassPrescaler = false;
        config->value = prescaler;
    } else {
        config->bypassPrescaler = true;
        config->value = 0;
    }
}

static void
timer_irq_handler(struct kinetis_hal_tmr *tmr)
{
    struct hal_timer *timer;
    uint16_t cur_cnt;

    /* Check if 16-bit counter rotated */
    cur_cnt = (uint16_t)LPTMR_GetCurrentTimerCount(tmr->base);
    if (cur_cnt <= tmr->last_irq_cnt) {
        tmr->overflow++;
    }
    tmr->last_irq_cnt = cur_cnt;

    while ((timer = TAILQ_FIRST(&tmr->hal_timer_q)) != NULL) {
        if ((int32_t)(kinetis_tmr_read(tmr) - timer->expiry) >= 0) {
            TAILQ_REMOVE(&tmr->hal_timer_q, timer, link);
            timer->link.tqe_prev = NULL;
            timer->cb_func(timer->cb_arg);
        } else {
            break;
        }
    }

    timer = TAILQ_FIRST(&tmr->hal_timer_q);
    if (timer) {
        kinetis_tmr_set_period(tmr, timer->expiry);
    }

    LPTMR_ClearStatusFlags(tmr->base, kLPTMR_TimerCompareFlag);
}

#if (FSL_FEATURE_LPTMR_HAS_SHARED_IRQ_HANDLER != 0)
static void
timer0_1_irq_handler(void)
{
    const struct kinetis_hal_tmr *tmr;
    int i;

    for (i = 0; i < KINETIS_TIMERS_MAX; i++) {
        tmr = kinetis_timers[i];
        if (tmr && tmr->base->CSR & LPTMR_CSR_TCF_MASK) {
            timer_irq_handler((struct kinetis_hal_tmr *)tmr);
        }
    }
}

#else /* (FSL_FEATURE_LPTMR_HAS_SHARED_IRQ_HANDLER != 0) */

#if MYNEWT_VAL(TIMER_0)
static void
timer0_irq_handler(void)
{
    timer_irq_handler(&kinetis_tmr0);
}
#endif

#if MYNEWT_VAL(TIMER_1)
static void
timer1_irq_handler(void)
{
    timer_irq_handler(&kinetis_tmr1);
}
#endif
#endif /* (FSL_FEATURE_LPTMR_HAS_SHARED_IRQ_HANDLER != 0) */

int
hal_timer_init(int num, void *cfg)
{
    const struct kinetis_hal_tmr *tmr;
    lptmr_config_t default_config;
    IRQn_Type irqn;

    if (num >= KINETIS_TIMERS_MAX || !(tmr = kinetis_timers[num])) {
        return -1;
    }

    LPTMR_GetDefaultConfig(&default_config);
    LPTMR_Init(tmr->base, &default_config);

    irqn = kinetis_timer_irqs[num];
    NVIC_SetPriority(irqn, (1 << __NVIC_PRIO_BITS) - 1);
#if (FSL_FEATURE_LPTMR_HAS_SHARED_IRQ_HANDLER != 0)
    NVIC_SetVector(irqn, (uint32_t)timer0_1_irq_handler);
#else
    switch (num) {
#if MYNEWT_VAL(TIMER_0)
    case 0:
        NVIC_SetVector(irqn, (uint32_t)timer0_irq_handler);
        break;
#endif
#if MYNEWT_VAL(TIMER_1)
    case 1:
        NVIC_SetVector(irqn, (uint32_t)timer1_irq_handler);
        break;
#endif
    }
#endif /* (FSL_FEATURE_LPTMR_HAS_SHARED_IRQ_HANDLER != 0) */

    NVIC_EnableIRQ(irqn);

    return 0;
}

int
hal_timer_deinit(int num)
{
    const struct kinetis_hal_tmr *tmr;

    if (num >= KINETIS_TIMERS_MAX || !(tmr = kinetis_timers[num])) {
        return -1;
    }

    LPTMR_Deinit(tmr->base);

    return 0;
}

int
hal_timer_config(int num, uint32_t freq_hz)
{
    const struct kinetis_hal_tmr *tmr;
    lptmr_config_t timer_config;
    os_sr_t sr;

    if (num >= KINETIS_TIMERS_MAX || !(tmr = kinetis_timers[num])) {
        return -1;
    }

    OS_ENTER_CRITICAL(sr);

    LPTMR_GetDefaultConfig(&timer_config);
    kinetis_tmr_config_freq(&timer_config, freq_hz);
    timer_config.enableFreeRunning = true;
    LPTMR_StopTimer(tmr->base);
    LPTMR_Init(tmr->base, &timer_config);
    LPTMR_EnableInterrupts(tmr->base, kLPTMR_TimerInterruptEnable);
    LPTMR_StartTimer(tmr->base);

    OS_EXIT_CRITICAL(sr);

    return 0;
}

uint32_t
hal_timer_get_resolution(int num)
{
    const struct kinetis_hal_tmr *tmr;

    if (num >= KINETIS_TIMERS_MAX || !(tmr = kinetis_timers[num])) {
        return 0;
    }

    return (1000000000 / kinetis_tmr_get_freq(tmr));
}

uint32_t
hal_timer_read(int num)
{
    const struct kinetis_hal_tmr *tmr;

    if (num >= KINETIS_TIMERS_MAX || !(tmr = kinetis_timers[num])) {
        return -1;
    }

    return kinetis_tmr_read(tmr);
}

int
hal_timer_delay(int num, uint32_t ticks)
{
    const struct kinetis_hal_tmr *tmr;
    uint32_t until;

    if (num >= KINETIS_TIMERS_MAX || !(tmr = kinetis_timers[num])) {
        return -1;
    }

    until = kinetis_tmr_read(tmr) + ticks;
    while ((int32_t)(kinetis_tmr_read(tmr) - until) <= 0) {
        ;
    }

    return 0;
}

int
hal_timer_set_cb(int num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    const struct kinetis_hal_tmr *tmr;

    if (num >= KINETIS_TIMERS_MAX || !(tmr = kinetis_timers[num])) {
        return -1;
    }

    timer->cb_func = cb_func;
    timer->cb_arg = arg;
    timer->bsp_timer = (struct kinetis_hal_tmr *)tmr;
    timer->link.tqe_prev = NULL;

    return 0;
}

int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    struct kinetis_hal_tmr *tmr;
    uint32_t tick;

    tmr = (struct kinetis_hal_tmr *)timer->bsp_timer;

    tick = ticks + kinetis_tmr_read(tmr);
    return hal_timer_start_at(timer, tick);
}

int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    struct kinetis_hal_tmr *tmr;
    struct hal_timer *entry;
    os_sr_t sr;

    if (!timer || timer->link.tqe_prev || !timer->cb_func) {
        return -1;
    }

    tmr = (struct kinetis_hal_tmr *)timer->bsp_timer;
    timer->expiry = tick;

    OS_ENTER_CRITICAL(sr);

    if (TAILQ_EMPTY(&tmr->hal_timer_q)) {
        TAILQ_INSERT_HEAD(&tmr->hal_timer_q, timer, link);
    } else {
        TAILQ_FOREACH(entry, &tmr->hal_timer_q, link) {
            if ((int32_t)(timer->expiry - entry->expiry) < 0) {
                TAILQ_INSERT_BEFORE(entry, timer, link);
                break;
            }
        }
        if (!entry) {
            TAILQ_INSERT_TAIL(&tmr->hal_timer_q, timer, link);
        }
    }

    if (timer == TAILQ_FIRST(&tmr->hal_timer_q)) {
        kinetis_tmr_set_period(tmr, tick);
    }

    LPTMR_EnableInterrupts(tmr->base, kLPTMR_TimerInterruptEnable);
    LPTMR_StartTimer(tmr->base);

    OS_EXIT_CRITICAL(sr);

    return 0;
}

int
hal_timer_stop(struct hal_timer *timer)
{
    struct kinetis_hal_tmr *tmr;
    struct hal_timer *entry = NULL;
    bool reset_period;
    os_sr_t sr;

    tmr = (struct kinetis_hal_tmr *)timer->bsp_timer;

    OS_ENTER_CRITICAL(sr);

    if (timer->link.tqe_prev != NULL) {
        reset_period = false;
        if (timer == TAILQ_FIRST(&tmr->hal_timer_q)) {
            entry = TAILQ_NEXT(timer, link);
            reset_period = true;
        }
        TAILQ_REMOVE(&tmr->hal_timer_q, timer, link);
        timer->link.tqe_prev = NULL;
        if (reset_period) {
            if (entry) {
                kinetis_tmr_set_period((struct kinetis_hal_tmr *)entry->bsp_timer,
                                       entry->expiry);
            } else {
                LPTMR_StopTimer(tmr->base);
            }
        }
    }

    OS_EXIT_CRITICAL(sr);

    return 0;
}
