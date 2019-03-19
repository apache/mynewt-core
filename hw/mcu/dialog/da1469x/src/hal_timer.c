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
#include <stdint.h>
#include "syscfg/syscfg.h"
#include "mcu/da1469x_hal.h"
#include "hal/hal_timer.h"
#include "defs/error.h"
#include "DA1469xAB.h"

#define DA1469X_TIMER_COUNT         (3)

/* Hardware timers are 24 bit only thus we need to compare values accordingly */
#define TICKS24_GT(_t1, _t2)        ((int32_t)(((_t1) - (_t2)) << 8) > 0)
#define TICKS24_GTE(_t1, _t2)       ((int32_t)(((_t1) - (_t2)) << 8) >= 0)
#define TICKS24_LT(_t1, _t2)        ((int32_t)(((_t1) - (_t2)) << 8) < 0)
#define TICKS24_LTE(_t1, _t2)       ((int32_t)(((_t1) - (_t2)) << 8) <= 0)
#define TICKS_GT(_t1, _t2)          ((int32_t)((_t1) - (_t2)) > 0)
#define TICKS_GTE(_t1, _t2)         ((int32_t)((_t1) - (_t2)) >= 0)
#define TICKS_LT(_t1, _t2)          ((int32_t)((_t1) - (_t2)) < 0)
#define TICKS_LTE(_t1, _t2)         ((int32_t)((_t1) - (_t2)) <= 0)

struct da1469x_timer {
    /*
     * Each Timer peripheral has separate registers definition, but those used
     * by hal_timer have the same offset thus we can safely use single type here.
     */
    TIMER_Type *regs;
    IRQn_Type irqn;
    TAILQ_HEAD(hal_timer_qhead, hal_timer) queue;
    /*
     * Upper 8 bits extend hardware counter register to be 32 bits.
     * Lower 24 bits keep last read value from hardware.
     * When new value is read and is lower then old one upper 8 bits are
     * incremented.
     */
    uint32_t tmr_cntr;
};

#if MYNEWT_VAL(TIMER_0)
struct da1469x_timer da1469x_timer_0;
#endif
#if MYNEWT_VAL(TIMER_1)
struct da1469x_timer da1469x_timer_1;
#endif
#if MYNEWT_VAL(TIMER_2)
struct da1469x_timer da1469x_timer_2;
#endif

static struct da1469x_timer *da1469x_timers[DA1469X_TIMER_COUNT] = {
#if MYNEWT_VAL(TIMER_0)
    &da1469x_timer_0,
#else
    NULL,
#endif
#if MYNEWT_VAL(TIMER_1)
    &da1469x_timer_1,
#else
    NULL,
#endif
#if MYNEWT_VAL(TIMER_2)
    &da1469x_timer_2,
#else
    NULL,
#endif
};

static inline struct da1469x_timer *
da1469x_timer_resolve(unsigned timer_num)
{
    if (timer_num >= DA1469X_TIMER_COUNT) {
        return NULL;
    }

    return da1469x_timers[timer_num];
}

static inline uint32_t
da1469x_timer_get_value_nolock(struct da1469x_timer *tmr)
{
    uint32_t v = tmr->regs->TIMER_TIMER_VAL_REG;

    if (v < (tmr->tmr_cntr & 0x00FFFFFFU)) {
        tmr->tmr_cntr = (tmr->tmr_cntr & 0xFF000000U) + v + 0x01000000U;
    } else {
        tmr->tmr_cntr = (tmr->tmr_cntr & 0xFF000000U) + v;
    }

    return tmr->tmr_cntr;
}

static inline uint32_t
da1469x_timer_get_value(struct da1469x_timer *tmr)
{
    uint32_t primask;
    uint32_t val;

    __HAL_DISABLE_INTERRUPTS(primask);
    val = da1469x_timer_get_value_nolock(tmr);
    __HAL_ENABLE_INTERRUPTS(primask);

    return val;
}

static void
da1469x_timer_set_trigger(struct da1469x_timer *tmr, uint32_t tick)
{
    TIMER_Type *regs;

    __HAL_ASSERT_CRITICAL();

    regs = tmr->regs;

    regs->TIMER_CTRL_REG &= ~TIMER_TIMER_CTRL_REG_TIM_IRQ_EN_Msk;
    regs->TIMER_RELOAD_REG = tick;
    regs->TIMER_CTRL_REG |= TIMER_TIMER_CTRL_REG_TIM_IRQ_EN_Msk;

    /* Force interrupt to occur as we may have missed it */
    if (TICKS_LTE(tick, da1469x_timer_get_value_nolock(tmr))) {
        NVIC_SetPendingIRQ(tmr->irqn);
    }
}

static void
da1469x_timer_set_half_time_trigger(struct da1469x_timer *tmr)
{
    TIMER_Type *regs;
    uint32_t tick;

    __HAL_ASSERT_CRITICAL();

    regs = tmr->regs;

    regs->TIMER_CTRL_REG &= ~TIMER_TIMER_CTRL_REG_TIM_IRQ_EN_Msk;
    tick = da1469x_timer_get_value_nolock(tmr) + 0x00800000U;
    regs->TIMER_RELOAD_REG = tick;
    regs->TIMER_CTRL_REG |= TIMER_TIMER_CTRL_REG_TIM_IRQ_EN_Msk;
}

#if MYNEWT_VAL(TIMER_0) || MYNEWT_VAL(TIMER_1) || MYNEWT_VAL(TIMER_2)
static void
da1469x_timer_check_queue(struct da1469x_timer *tmr)
{
    uint32_t primask;
    struct hal_timer *timer;

    __HAL_DISABLE_INTERRUPTS(primask);

    /* Remove and process each expired timer in the sorted queue. */
    while ((timer = TAILQ_FIRST(&tmr->queue)) != NULL) {
        if (TICKS_GT(timer->expiry, da1469x_timer_get_value_nolock(tmr))) {
            break;
        }

        TAILQ_REMOVE(&tmr->queue, timer, link);
        timer->link.tqe_prev = NULL;
        timer->cb_func(timer->cb_arg);
    }

    /* Schedule interrupt for next timer, if any. */
    if (timer != NULL) {
        da1469x_timer_set_trigger(tmr, timer->expiry);
    } else {
        da1469x_timer_set_half_time_trigger(tmr);
    }

    __HAL_ENABLE_INTERRUPTS(primask);
}

static void
da1469x_timer_common_isr(struct da1469x_timer *tmr)
{
    /* Note that TIMER_Type differs from TIMER{2/3/4}_Type in
     * the way that TIMER_Type has additional 5 fields just before the
     * last field which is common field for all of the timers: TIMER_CLEAR_IRQ_REQ.
     * Below make sure to use correct CLEAR_IRQ_REQ depends on timer used.
     */
    if (tmr->irqn == TIMER_IRQn) {
        tmr->regs->TIMER_CLEAR_IRQ_REG = 1;
    } else {
        ((TIMER3_Type *)tmr->regs)->TIMER3_CLEAR_IRQ_REG = 1;
    }

    da1469x_timer_check_queue(tmr);
}
#endif

#if MYNEWT_VAL(TIMER_0)
static void
da1469x_timer_isr(void)
{
    da1469x_timer_common_isr(&da1469x_timer_0);
}
#endif

#if MYNEWT_VAL(TIMER_1)
static void
da1469x_timer3_isr(void)
{
    da1469x_timer_common_isr(&da1469x_timer_1);
}
#endif

#if MYNEWT_VAL(TIMER_2)
static void
da1469x_timer4_isr(void)
{
    da1469x_timer_common_isr(&da1469x_timer_2);
}
#endif

int
hal_timer_init(int timer_num, void *vcfg)
{
    struct da1469x_timer *tmr;
    TIMER_Type *regs;
    IRQn_Type irqn;
    void (* isr)(void);

    tmr = da1469x_timer_resolve(timer_num);
    if (!tmr) {
        return SYS_EINVAL;
    }

    switch (timer_num) {
#if MYNEWT_VAL(TIMER_0)
    case 0:
        regs = (void *)TIMER_BASE;
        irqn = TIMER_IRQn;
        isr = da1469x_timer_isr;
        break;
#endif
#if MYNEWT_VAL(TIMER_1)
    case 1:
        regs = (void *)TIMER3_BASE;
        irqn = TIMER3_IRQn;
        isr = da1469x_timer3_isr;
        break;
#endif
#if MYNEWT_VAL(TIMER_2)
    case 2:
        regs = (void *)TIMER4_BASE;
        irqn = TIMER4_IRQn;
        isr = da1469x_timer4_isr;
        break;
#endif
    default:
        return SYS_EINVAL;
    }

    tmr->regs = regs;
    tmr->irqn = irqn;
    TAILQ_INIT(&tmr->queue);

    /* Disable IRQ, set priority and set vector in table */
    NVIC_DisableIRQ(irqn);
    NVIC_SetPriority(irqn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(irqn, (uint32_t)isr);

    return 0;
}

static int
da1469x_find_prescaler(int freq_hz, uint32_t *sys_clk_en)
{
    int base_freq_hz;
    int prev_freq_hz;
    int curr_freq_hz;
    int div;

    /* We can support 1024-32768 Hz from lp_clk or 1-32 MHz from DinV */
    if (!(((freq_hz >= 1024) && (freq_hz <= 32768)) ||
          ((freq_hz >= 1000000) && (freq_hz <= 32000000)))) {
        return -1;
    }

    if (freq_hz < 1000000) {
        base_freq_hz = 32768;
        *sys_clk_en = 0 << TIMER_TIMER_CTRL_REG_TIM_SYS_CLK_EN_Pos;
    } else {
        base_freq_hz = 32000000;
        *sys_clk_en = 1 << TIMER_TIMER_CTRL_REG_TIM_SYS_CLK_EN_Pos;
    }

    prev_freq_hz = base_freq_hz;

    for (div = 0; div < 32; div++) {
        curr_freq_hz = base_freq_hz / (div + 1);

        /*
         * Look for first prescaled frequency which is either equal to or lower
         * than target frequency. Select either current or previous prescaled
         * frequency, depending on which one is closer to target frequency.
         */
        if (curr_freq_hz <= freq_hz) {
            if (freq_hz - curr_freq_hz > prev_freq_hz - freq_hz) {
                div--;
            }
            break;
        }

        prev_freq_hz = curr_freq_hz;
    }

    assert(div < 32);

    return div;
}

int
hal_timer_config(int timer_num, uint32_t freq_hz)
{
    struct da1469x_timer *tmr;
    TIMER_Type *regs;
    uint32_t sys_clk_en;
    int prescaler;

    tmr = da1469x_timer_resolve(timer_num);
    if (!tmr) {
        return SYS_EINVAL;
    }

    regs = tmr->regs;

    prescaler = da1469x_find_prescaler(freq_hz, &sys_clk_en);
    if (prescaler < 0) {
        return SYS_EINVAL;
    }

    assert(prescaler < 32);

    regs->TIMER_CTRL_REG = 0;
    regs->TIMER_PRESCALER_REG = prescaler;
    regs->TIMER_CTRL_REG = TIMER_TIMER_CTRL_REG_TIM_CLK_EN_Msk |
                           sys_clk_en |
                           TIMER_TIMER_CTRL_REG_TIM_FREE_RUN_MODE_EN_Msk |
                           TIMER_TIMER_CTRL_REG_TIM_IRQ_EN_Msk |
                           TIMER_TIMER_CTRL_REG_TIM_EN_Msk;

    NVIC_EnableIRQ(tmr->irqn);

    return 0;
}

int
hal_timer_deinit(int timer_num)
{
    struct da1469x_timer *tmr;
    TIMER_Type *regs;

    tmr = da1469x_timer_resolve(timer_num);
    if (!tmr) {
        return SYS_EINVAL;
    }

    regs = tmr->regs;
    regs->TIMER_CTRL_REG &= ~(TIMER_TIMER_CTRL_REG_TIM_CLK_EN_Msk |
                              TIMER_TIMER_CTRL_REG_TIM_IRQ_EN_Msk |
                              TIMER_TIMER_CTRL_REG_TIM_EN_Msk);

    return 0;
}

uint32_t
hal_timer_get_resolution(int timer_num)
{
    struct da1469x_timer *tmr;
    TIMER_Type *regs;
    uint32_t freq;

    tmr = da1469x_timer_resolve(timer_num);
    if (!tmr) {
        return SYS_EINVAL;
    }

    regs = tmr->regs;

    freq = regs->TIMER_CTRL_REG & TIMER_TIMER_CTRL_REG_TIM_SYS_CLK_EN_Msk ?
           32000000 : 32768;
    freq /= regs->TIMER_PRESCALER_REG + 1;

    return 1000000000 / freq;
}

uint32_t
hal_timer_read(int timer_num)
{
    struct da1469x_timer *tmr;

    tmr = da1469x_timer_resolve(timer_num);
    if (!tmr) {
        return SYS_EINVAL;
    }

    return da1469x_timer_get_value(tmr);
}

int
hal_timer_delay(int timer_num, uint32_t ticks)
{
    struct da1469x_timer *tmr;
    uint32_t until;

    tmr = da1469x_timer_resolve(timer_num);
    if (!tmr) {
        return SYS_EINVAL;
    }

    until = da1469x_timer_get_value(tmr) + ticks;
    while (TICKS_LT(da1469x_timer_get_value(tmr), until)) {
        /* busy loop */
    }

    return 0;
}

int
hal_timer_set_cb(int timer_num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    struct da1469x_timer *tmr;

    tmr = da1469x_timer_resolve(timer_num);
    if (!tmr) {
        return SYS_EINVAL;
    }

    timer->cb_func = cb_func;
    timer->cb_arg = arg;
    timer->bsp_timer = tmr;
    timer->link.tqe_prev = NULL;

    return 0;
}

int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    struct da1469x_timer *tmr;
    uint32_t tick;
    int rc;

    tmr = timer->bsp_timer;

    tick = da1469x_timer_get_value(tmr) + ticks;
    rc = hal_timer_start_at(timer, tick);

    return rc;
}

int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    uint32_t primask;
    struct da1469x_timer *tmr;
    struct hal_timer *cur;

    tmr = timer->bsp_timer;

    timer->expiry = tick;

    __HAL_DISABLE_INTERRUPTS(primask);

    if (TAILQ_EMPTY(&tmr->queue)) {
        TAILQ_INSERT_HEAD(&tmr->queue, timer, link);
    } else {
        TAILQ_FOREACH(cur, &tmr->queue, link) {
            if (TICKS_LT(timer->expiry, cur->expiry)) {
                TAILQ_INSERT_BEFORE(cur, timer, link);
                break;
            }
        }
        if (cur == NULL) {
            TAILQ_INSERT_TAIL(&tmr->queue, timer, link);
        }
    }

    if (timer == TAILQ_FIRST(&tmr->queue)) {
        da1469x_timer_set_trigger(tmr, tick);
    } else {
        da1469x_timer_set_half_time_trigger(tmr);
    }

    __HAL_ENABLE_INTERRUPTS(primask);

    return 0;
}

int
hal_timer_stop(struct hal_timer *timer)
{
    uint32_t primask;
    struct da1469x_timer *tmr;
    int reset;

    /* Item has no pointer to prev if not started (or already stopped) */
    if (timer->link.tqe_prev == NULL) {
        return 0;
    }

    tmr = timer->bsp_timer;

    __HAL_DISABLE_INTERRUPTS(primask);

    /* Need to reset HW timer if this one is first on list */
    if (timer == TAILQ_FIRST(&tmr->queue)) {
        reset = 1;
    }

    TAILQ_REMOVE(&tmr->queue, timer, link);
    timer->link.tqe_prev = NULL;

    if (reset) {
        timer = TAILQ_FIRST(&tmr->queue);
        if (timer != NULL) {
            da1469x_timer_set_trigger(tmr, timer->expiry);
        } else {
            da1469x_timer_set_half_time_trigger(tmr);
        }
    }

    __HAL_ENABLE_INTERRUPTS(primask);

    return 0;
}
