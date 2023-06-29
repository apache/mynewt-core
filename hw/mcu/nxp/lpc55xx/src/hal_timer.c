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

#include <os/mynewt.h>
#include <hal/hal_timer.h>
#include <mcu/cmsis_nvic.h>

#include <fsl_ctimer.h>

struct ctimer_hal_tmr;
struct ctimer_hal_tmr_cfg {
    CTIMER_Type *base;
    clock_ip_name_t clock_gate;
    clock_attach_id_t clock_id;
    IRQn_Type irqn;
    void (*isr)(void);
};

struct ctimer_hal_tmr {
    const struct ctimer_hal_tmr_cfg *cfg;
    uint32_t freq;
#if MYNEWT_VAL(CTIMER_AUTO_OFF_COUNT)
    uint8_t overflow_count;
#endif

    TAILQ_HEAD(hal_timer_qhead, hal_timer) hal_timer_q;
};

#if MYNEWT_VAL(CTIMER_AUTO_OFF_COUNT)
#define INC_OVERFLOW(tmr) (++(tmr->overflow_count))
#define OVERFLOW_COUNT(tmr) tmr->overflow_count
#define OVERFLOW_COUNT_RESET(tmr) tmr->overflow_count = 0
#else
#define INC_OVERFLOW(tmr)
#define OVERFLOW_COUNT(tmr) 0
#define OVERFLOW_COUNT_RESET(tmr)
#endif

static void timer_irq_handler(struct ctimer_hal_tmr *tmr);

#define TIMER_CFG_DEF(n) \
    static void CTIMER ## n ## _IRQHandler(void); \
    struct ctimer_hal_tmr_cfg TIMER_ ## n ## _cfg = { \
        .base = CTIMER ## n, \
        .clock_gate = kCLOCK_Timer ## n, \
        .clock_id = kMAIN_CLK_to_CTIMER ## n, \
        .irqn = CTIMER ## n ## _IRQn, \
        .isr = CTIMER ## n ## _IRQHandler, \
    }; \
    struct ctimer_hal_tmr ctimer_hal_tmr_ ## n = { \
        .cfg = &TIMER_ ## n ## _cfg, \
    }; \
    static void CTIMER ## n ## _IRQHandler(void) \
    { \
        timer_irq_handler(&ctimer_hal_tmr_ ## n); \
    }

TIMER_CFG_DEF(0);
TIMER_CFG_DEF(1);
TIMER_CFG_DEF(2);
TIMER_CFG_DEF(3);
TIMER_CFG_DEF(4);

#define TIMER_REF(n) (MYNEWT_VAL_TIMER_ ## n) ? &ctimer_hal_tmr_ ## n : NULL,

static struct ctimer_hal_tmr *const timers[FSL_FEATURE_SOC_CTIMER_COUNT] = {
    TIMER_REF(0)
    TIMER_REF(1)
    TIMER_REF(2)
    TIMER_REF(3)
    TIMER_REF(4)
};

static uint32_t
ctimer_tmr_read(struct ctimer_hal_tmr *tmr)
{
    CTIMER_Type *base = tmr->cfg->base;
    if (MYNEWT_VAL(CTIMER_AUTO_OFF_COUNT) && 0 == (base->TCR & CTIMER_TCR_CEN_MASK)) {
        CTIMER_StartTimer(base);
        OVERFLOW_COUNT_RESET(tmr);
    }
    return CTIMER_GetTimerCountValue(tmr->cfg->base);
}

static uint32_t
ctimer_tmr_get_freq(const struct ctimer_hal_tmr *tmr)
{
    return tmr->freq;
}

static void
ctimer_tmr_config_freq(ctimer_config_t *config, uint32_t freq_hz)
{
    config->prescale = (SystemCoreClock / freq_hz) - 1;
}

static void
timer_irq_handler(struct ctimer_hal_tmr *tmr)
{
    struct hal_timer *timer;
    CTIMER_Type *base = tmr->cfg->base;
    bool empty_run = true;
    struct _ctimer_match_config match_action = {
        .enableInterrupt = true
    };

    while ((timer = TAILQ_FIRST(&tmr->hal_timer_q)) != NULL) {
        if ((int32_t)(CTIMER_GetTimerCountValue(base) - timer->expiry) >= 0) {
            TAILQ_REMOVE(&tmr->hal_timer_q, timer, link);
            timer->link.tqe_prev = NULL;
            timer->cb_func(timer->cb_arg);
            empty_run = false;
        } else {
            break;
        }
    }

    timer = TAILQ_FIRST(&tmr->hal_timer_q);
    if (timer) {
        match_action.matchValue = timer->expiry;
        CTIMER_SetupMatch(base, kCTIMER_Match_0, &match_action);
    } else {
        if (MYNEWT_VAL(CTIMER_AUTO_OFF_COUNT)) {
            if (empty_run && OVERFLOW_COUNT(tmr) == MYNEWT_VAL(CTIMER_AUTO_OFF_COUNT)) {
                CTIMER_StopTimer(base);
            } else {
                INC_OVERFLOW(tmr);
                match_action.matchValue = CTIMER_GetTimerCountValue(base);
                CTIMER_SetupMatch(base, kCTIMER_Match_0, &match_action);
            }
        } else {
            CTIMER_DisableInterrupts(base, kCTIMER_Match0InterruptEnable);
        }
    }
    CTIMER_ClearStatusFlags(base, kCTIMER_Match0Flag);
}

int
hal_timer_init(int num, void *cfg)
{
    const struct ctimer_hal_tmr *tmr;
    ctimer_config_t default_config;
    IRQn_Type irqn;
    CTIMER_Type *base;

    (void)cfg;

    if (num >= FSL_FEATURE_SOC_CTIMER_COUNT || !(tmr = timers[num])) {
        return -1;
    }

    base = tmr->cfg->base;

    CLOCK_AttachClk(tmr->cfg->clock_id);
    CLOCK_EnableClock(tmr->cfg->clock_gate);
    CTIMER_GetDefaultConfig(&default_config);
    CTIMER_Init(base, &default_config);

    irqn = tmr->cfg->irqn;
    NVIC_SetPriority(irqn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(irqn, (uint32_t)tmr->cfg->isr);
    NVIC_EnableIRQ(irqn);
    if (MYNEWT_VAL_CTIMER_AUTO_OFF_COUNT) {
        ctimer_match_config_t match = {
            .matchValue = base->TC,
            .enableInterrupt = true,
        };
        CTIMER_SetupMatch(base, kCTIMER_Match_0, &match);

    }
    CTIMER_StartTimer(base);

    return 0;
}

int
hal_timer_deinit(int num)
{
    const struct ctimer_hal_tmr *tmr;

    if (num >= FSL_FEATURE_SOC_CTIMER_COUNT || !(tmr = timers[num])) {
        return -1;
    }

    CTIMER_Deinit(tmr->cfg->base);

    return 0;
}

int
hal_timer_config(int num, uint32_t freq_hz)
{
    struct ctimer_hal_tmr *tmr;
    ctimer_config_t timer_config;
    CTIMER_Type *base;
    os_sr_t sr;

    if (num >= FSL_FEATURE_SOC_CTIMER_COUNT || !(tmr = timers[num])) {
        return -1;
    }
    base = tmr->cfg->base;

    OS_ENTER_CRITICAL(sr);

    CTIMER_GetDefaultConfig(&timer_config);
    ctimer_tmr_config_freq(&timer_config, freq_hz);
    CTIMER_StopTimer(base);
    CTIMER_Init(base, &timer_config);
    tmr->freq = SystemCoreClock / (timer_config.prescale + 1);
    CTIMER_EnableInterrupts(base, kCTIMER_Match0InterruptEnable);
    CTIMER_StartTimer(base);

    OS_EXIT_CRITICAL(sr);

    return 0;
}

uint32_t
hal_timer_get_resolution(int num)
{
    const struct ctimer_hal_tmr *tmr;

    if (num >= FSL_FEATURE_SOC_CTIMER_COUNT || !(tmr = timers[num])) {
        return 0;
    }

    return (1000000000 / ctimer_tmr_get_freq(tmr));
}

uint32_t
hal_timer_read(int num)
{
    struct ctimer_hal_tmr *tmr;

    if (num >= FSL_FEATURE_SOC_CTIMER_COUNT || !(tmr = timers[num])) {
        return 0;
    }

    return ctimer_tmr_read(tmr);
}

int
hal_timer_delay(int num, uint32_t ticks)
{
    struct ctimer_hal_tmr *tmr;
    uint32_t until;

    if (num >= FSL_FEATURE_SOC_CTIMER_COUNT || !(tmr = timers[num])) {
        return -1;
    }

    until = ctimer_tmr_read(tmr) + ticks;
    while ((int32_t)(ctimer_tmr_read(tmr) - until) <= 0) {
    }

    return 0;
}

int
hal_timer_set_cb(int num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    const struct ctimer_hal_tmr *tmr;

    if (num >= FSL_FEATURE_SOC_CTIMER_COUNT || !(tmr = timers[num])) {
        return -1;
    }

    timer->cb_func = cb_func;
    timer->cb_arg = arg;
    timer->bsp_timer = (struct ctimer_hal_tmr *)tmr;
    timer->link.tqe_prev = NULL;

    return 0;
}

int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    struct ctimer_hal_tmr *tmr;
    uint32_t tick;

    tmr = (struct ctimer_hal_tmr *)timer->bsp_timer;

    tick = ticks + CTIMER_GetTimerCountValue(tmr->cfg->base);

    return hal_timer_start_at(timer, tick);
}

int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    struct ctimer_hal_tmr *tmr;
    struct hal_timer *entry;
    CTIMER_Type *base;
    os_sr_t sr;

    if (!timer || timer->link.tqe_prev || !timer->cb_func) {
        return -1;
    }

    tmr = (struct ctimer_hal_tmr *)timer->bsp_timer;
    base = tmr->cfg->base;
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
        ctimer_match_config_t match = {
            .matchValue = timer->expiry,
            .enableInterrupt = true,
        };
        CTIMER_SetupMatch(base, kCTIMER_Match_0, &match);
        CTIMER_EnableResetMatchChannel(base, kCTIMER_Match_0, true);
        OVERFLOW_COUNT_RESET(tmr);
    }

    if (0 == (base->TCR & CTIMER_TCR_CEN_MASK)) {
        CTIMER_StartTimer(base);
    }

    OS_EXIT_CRITICAL(sr);

    return 0;
}

int
hal_timer_stop(struct hal_timer *timer)
{
    struct ctimer_hal_tmr *tmr;
    struct hal_timer *entry = NULL;
    bool reset_period;
    CTIMER_Type *base;
    os_sr_t sr;

    tmr = (struct ctimer_hal_tmr *)timer->bsp_timer;
    base = tmr->cfg->base;

    OS_ENTER_CRITICAL(sr);

    if (timer->link.tqe_prev != NULL) {
        reset_period = false;
        /* Is timer first in the queue? */
        if (timer == TAILQ_FIRST(&tmr->hal_timer_q)) {
            entry = TAILQ_NEXT(timer, link);
            reset_period = true;
        }
        TAILQ_REMOVE(&tmr->hal_timer_q, timer, link);
        timer->link.tqe_prev = NULL;
        if (reset_period) {
            if (entry) {
                ctimer_match_config_t match = {
                    .matchValue = entry->expiry,
                    .enableInterrupt = true,
                };
                CTIMER_SetupMatch(base, kCTIMER_Match_0, &match);
            } else {
                CTIMER_StopTimer(tmr->cfg->base);
            }
        }
    }

    OS_EXIT_CRITICAL(sr);

    return 0;
}
