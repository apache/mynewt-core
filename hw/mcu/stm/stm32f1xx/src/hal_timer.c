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

#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "os/mynewt.h"

#include <mcu/cmsis_nvic.h>
#include <hal/hal_timer.h>

#if 0

#include "stm32l1xx.h"
#include "stm32l1xx_hal_dma.h"
#include "stm32l1xx_hal_tim.h"
#include "stm32l1xx_hal_rcc.h"

#include "mcu/stm32l1xx_mynewt_hal.h"

#define STM32L1_HAL_TIMER_MAX     (2)

struct stm32l1_hal_tmr {
    TIM_TypeDef *sht_regs;	/* Pointer to timer registers */
    uint32_t sht_oflow;		/* 16 bits of overflow to make timer 32bits */
    TAILQ_HEAD(hal_timer_qhead, hal_timer) sht_timers;
};

#if MYNEWT_VAL(TIMER_0)
struct stm32l1_hal_tmr stm32l1_tmr0;
#endif
#if MYNEWT_VAL(TIMER_1)
struct stm32l1_hal_tmr stm32l1_tmr1;
#endif

static struct stm32l1_hal_tmr *stm32l1_tmr_devs[STM32L1_HAL_TIMER_MAX] = {
#if MYNEWT_VAL(TIMER_0)
    &stm32l1_tmr0,
#else
    NULL,
#endif
#if MYNEWT_VAL(TIMER_1)
    &stm32l1_tmr1,
#else
    NULL,
#endif
};

static uint32_t hal_timer_cnt(struct stm32l1_hal_tmr *tmr);

#if (MYNEWT_VAL(TIMER_0) || MYNEWT_VAL(TIMER_1))
/*
 * Call expired timer callbacks, and reprogram timer with new expiry time.
 */
static void
stm32l1_tmr_cbs(struct stm32l1_hal_tmr *tmr)
{
    uint32_t cnt;
    struct hal_timer *ht;

    while ((ht = TAILQ_FIRST(&tmr->sht_timers)) != NULL) {
        cnt = hal_timer_cnt(tmr);
        if (((int32_t)(cnt - ht->expiry)) >= 0) {
            TAILQ_REMOVE(&tmr->sht_timers, ht, link);
            ht->link.tqe_prev = NULL;
            ht->cb_func(ht->cb_arg);
        } else {
            break;
        }
    }
    ht = TAILQ_FIRST(&tmr->sht_timers);
    if (ht) {
        tmr->sht_regs->CCR1 = ht->expiry;
    } else {
        TIM_CCxChannelCmd(tmr->sht_regs, TIM_CHANNEL_1, TIM_CCx_DISABLE);
        tmr->sht_regs->DIER &= ~TIM_DIER_CC1IE;
    }
}

/*
 * timer irq handler
 *
 * Generic HAL timer irq handler.
 *
 * @param tmr
 */
static void
stm32l1_tmr_irq(struct stm32l1_hal_tmr *tmr)
{
    uint32_t sr;
    uint32_t clr = 0;

    sr = tmr->sht_regs->SR;
    if (sr & TIM_SR_UIF) {
        /*
         * Overflow interrupt
         */
        tmr->sht_oflow += 0x10000;
        clr |= TIM_SR_UIF;
    }
    if (sr & TIM_SR_CC1IF) {
        /*
         * Capture event
         */
        clr |= TIM_SR_CC1IF;
        stm32l1_tmr_cbs(tmr);
    }

    tmr->sht_regs->SR = ~clr;
}
#endif

#if MYNEWT_VAL(TIMER_0)
static void
stm32l1_tmr0_irq(void)
{
    stm32l1_tmr_irq(&stm32l1_tmr0);
}
#endif

#if MYNEWT_VAL(TIMER_1)
static void
stm32l1_tmr1_irq(void)
{
    stm32l1_tmr_irq(&stm32l1_tmr1);
}
#endif

static void
stm32l1_tmr_reg_irq(IRQn_Type irqn, uint32_t func)
{
    NVIC_SetPriority(irqn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(irqn, func);
    NVIC_EnableIRQ(irqn);
}

static uint32_t
stm32l1_base_freq(TIM_TypeDef *regs)
{
    RCC_ClkInitTypeDef clocks;
    uint32_t fl;
    uint32_t freq;

    HAL_RCC_GetClockConfig(&clocks, &fl);

    /*
     * Assuming RCC_DCKCFGR->TIMPRE is 0.
     * There's just APB2 timers here.
     */
    switch ((uint32_t)regs) {
#ifdef TIM1
    case (uint32_t)TIM1:
#endif
#ifdef TIM8
    case (uint32_t)TIM8:
#endif
#ifdef TIM9
    case (uint32_t)TIM9:
#endif
#ifdef TIM10
    case (uint32_t)TIM10:
#endif
#ifdef TIM11
    case (uint32_t)TIM11:
#endif
        freq = HAL_RCC_GetPCLK2Freq();
        if (clocks.APB2CLKDivider) {
            freq *= 2;
        }
        break;
    default:
        return 0;
    }
    return freq;
}

static void
stm32l1_hw_setup(int num, TIM_TypeDef *regs)
{
    uint32_t func;

    switch (num) {
#if MYNEWT_VAL(TIMER_0)
    case 0:
        func = (uint32_t)stm32l1_tmr0_irq;
        break;
#endif
#if MYNEWT_VAL(TIMER_1)
    case 1:
        func = (uint32_t)stm32l1_tmr1_irq;
        break;
#endif
    default:
        assert(0);
        return;
    }

#ifdef TIM1
    if (regs == TIM1) {
        stm32l1_tmr_reg_irq(TIM1_CC_IRQn, func);
        stm32l1_tmr_reg_irq(TIM1_UP_TIM10_IRQn, func);
        __HAL_RCC_TIM1_CLK_ENABLE();
    }
#endif
#ifdef TIM8
    if (regs == TIM8) {
        stm32l1_tmr_reg_irq(TIM8_CC_IRQn, func);
        stm32l1_tmr_reg_irq(TIM8_UP_TIM13_IRQn, func);
        __HAL_RCC_TIM8_CLK_ENABLE();
    }
#endif
#ifdef TIM9
    if (regs == TIM9) {
        stm32l1_tmr_reg_irq(TIM1_BRK_TIM9_IRQn, func);
        __HAL_RCC_TIM9_CLK_ENABLE();
    }
#endif
#ifdef TIM10
    if (regs == TIM10) {
        stm32l1_tmr_reg_irq(TIM1_UP_TIM10_IRQn, func);
        __HAL_RCC_TIM10_CLK_ENABLE();
    }
#endif
#ifdef TIM11
    if (regs == TIM11) {
        stm32l1_tmr_reg_irq(TIM1_TRG_COM_TIM11_IRQn, func);
        __HAL_RCC_TIM11_CLK_ENABLE();
    }
#endif
}

static void
stm32l1_hw_setdown(TIM_TypeDef *regs)
{
#ifdef TIM1
    if (regs == TIM1) {
        __HAL_RCC_TIM1_CLK_DISABLE();
    }
#endif
#ifdef TIM8
    if (regs == TIM8) {
        __HAL_RCC_TIM8_CLK_DISABLE();
    }
#endif
#ifdef TIM9
    if (regs == TIM9) {
        __HAL_RCC_TIM9_CLK_DISABLE();
    }
#endif
#ifdef TIM10
    if (regs == TIM10) {
        __HAL_RCC_TIM10_CLK_DISABLE();
    }
#endif
#ifdef TIM11
    if (regs == TIM11) {
        __HAL_RCC_TIM11_CLK_DISABLE();
    }
#endif
}

/**
 * hal timer init
 *
 * Initialize platform specific timer items
 *
 * @param timer_num     Timer number to initialize
 * @param cfg           Pointer to platform specific configuration
 *
 * @return int          0: success; error code otherwise
 */
int
hal_timer_init(int num, void *cfg)
{
    struct stm32l1_hal_tmr *tmr;

    if (num >= STM32L1_HAL_TIMER_MAX || !(tmr = stm32l1_tmr_devs[num]) ||
        (cfg == NULL)) {
        return -1;
    }

    tmr->sht_regs = (TIM_TypeDef *)cfg;

    if (!IS_TIM_CC1_INSTANCE(tmr->sht_regs)) {
        return -1;
    }

    stm32l1_hw_setup(num, tmr->sht_regs);

    /*
     * Stop the timers at debugger. XXX Which TIM?
     */
    DBGMCU->APB1FZ |= 0x1ff; /* TIM2 - TIM7, TIM12-TIM14 */
    DBGMCU->APB2FZ |= 0x70003; /* TIM1, TIM8-TIM11 */

    return 0;
}

/**
 * hal timer config
 *
 * Configure a timer to run at the desired frequency. This starts the timer.
 *
 * @param timer_num
 * @param freq_hz
 *
 * @return int
 */
int
hal_timer_config(int num, uint32_t freq_hz)
{
    struct stm32l1_hal_tmr *tmr;
    TIM_Base_InitTypeDef init;
    uint32_t prescaler;

    if (num >= STM32L1_HAL_TIMER_MAX || !(tmr = stm32l1_tmr_devs[num])) {
        return -1;
    }
    if (!IS_TIM_CC1_INSTANCE(tmr->sht_regs)) {
        return -1;
    }

    prescaler = stm32l1_base_freq(tmr->sht_regs) / freq_hz;
    if (prescaler > 0xffff) {
        return -1;
    }

    memset(&init, 0, sizeof(init));
    init.Period = 0xffff;
    init.Prescaler = prescaler;
    init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    init.CounterMode = TIM_COUNTERMODE_UP;

    /*
     * Set up to count overflow interrupts.
     */
    tmr->sht_regs->CR1 = TIM_CR1_URS;
    tmr->sht_regs->DIER = TIM_DIER_UIE;

    TIM_Base_SetConfig(tmr->sht_regs, &init);

    tmr->sht_regs->SR = 0;
    tmr->sht_regs->CR1 |= TIM_CR1_CEN;

    return 0;
}

/**
 * hal timer deinit
 *
 * De-initialize a HW timer.
 *
 * @param timer_num
 *
 * @return int
 */
int
hal_timer_deinit(int num)
{
    struct stm32l1_hal_tmr *tmr;
    int sr;

    if (num >= STM32L1_HAL_TIMER_MAX || !(tmr = stm32l1_tmr_devs[num])) {
        return -1;
    }

    __HAL_DISABLE_INTERRUPTS(sr);
    /*
     * Turn off CC1, and then turn off the timer.
     */
    tmr->sht_regs->CR1 &= ~TIM_CR1_CEN;
    tmr->sht_regs->DIER &= ~TIM_DIER_CC1IE;
    TIM_CCxChannelCmd(tmr->sht_regs, TIM_CHANNEL_1, TIM_CCx_DISABLE);
    __HAL_ENABLE_INTERRUPTS(sr);
    stm32l1_hw_setdown(tmr->sht_regs);

    return 0;
}

/**
 * hal timer get resolution
 *
 * Get the resolution of the timer. This is the timer period, in nanoseconds
 *
 * @param timer_num
 *
 * @return uint32_t
 */
uint32_t
hal_timer_get_resolution(int num)
{
    struct stm32l1_hal_tmr *tmr;

    if (num >= STM32L1_HAL_TIMER_MAX || !(tmr = stm32l1_tmr_devs[num])) {
        return -1;
    }
    return (1000000000 / (SystemCoreClock / tmr->sht_regs->PSC));
}

static uint32_t
hal_timer_cnt(struct stm32l1_hal_tmr *tmr)
{
    uint32_t cnt;
    int sr;

    __HAL_DISABLE_INTERRUPTS(sr);
    cnt = tmr->sht_oflow + tmr->sht_regs->CNT;
    if (tmr->sht_regs->SR & TIM_SR_UIF) {
        /*
         * Just overflowed
         */
        cnt = tmr->sht_oflow + tmr->sht_regs->CNT + 0x10000;
    }
    __HAL_ENABLE_INTERRUPTS(sr);

    return cnt;
}

/**
 * hal timer read
 *
 * Returns the timer counter. NOTE: if the timer is a 16-bit timer, only
 * the lower 16 bits are valid. If the timer is a 64-bit timer, only the
 * low 32-bits are returned.
 *
 * @return uint32_t The timer counter register.
 */
uint32_t
hal_timer_read(int num)
{
    struct stm32l1_hal_tmr *tmr;

    if (num >= STM32L1_HAL_TIMER_MAX || !(tmr = stm32l1_tmr_devs[num])) {
        return -1;
    }
    return hal_timer_cnt(tmr);
}

/**
 * hal timer delay
 *
 * Blocking delay for n ticks
 *
 * @param timer_num
 * @param ticks
 *
 * @return int 0 on success; error code otherwise.
 */
int
hal_timer_delay(int num, uint32_t ticks)
{
    struct stm32l1_hal_tmr *tmr;
    uint32_t until;

    if (num >= STM32L1_HAL_TIMER_MAX || !(tmr = stm32l1_tmr_devs[num])) {
        return -1;
    }

    until = hal_timer_cnt(tmr) + ticks;
    while ((int32_t)(hal_timer_cnt(tmr) - until) <= 0) {
        ;
    }
    return 0;
}

/**
 *
 * Initialize the HAL timer structure with the callback and the callback
 * argument. Also initializes the HW specific timer pointer.
 *
 * @param cb_func
 *
 * @return int
 */
int
hal_timer_set_cb(int num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    struct stm32l1_hal_tmr *tmr;

    if (num >= STM32L1_HAL_TIMER_MAX || !(tmr = stm32l1_tmr_devs[num])) {
        return -1;
    }
    timer->cb_func = cb_func;
    timer->cb_arg = arg;
    timer->bsp_timer = tmr;
    timer->link.tqe_prev = NULL;

    return 0;
}

/**
 * hal_timer_start()
 *
 * Start a timer. Timer fires 'ticks' ticks from now.
 *
 * @param timer
 * @param ticks
 *
 * @return int
 */
int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    struct stm32l1_hal_tmr *tmr;
    uint32_t tick;

    tmr = (struct stm32l1_hal_tmr *)timer->bsp_timer;

    tick = ticks + hal_timer_cnt(tmr);
    return hal_timer_start_at(timer, tick);
}

/**
 * hal_timer_start_at()
 *
 * Start a timer. Timer fires at tick 'tick'.
 *
 * @param timer
 * @param tick
 *
 * @return int
 */
int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    struct stm32l1_hal_tmr *tmr;
    struct hal_timer *ht;
    int sr;

    tmr = (struct stm32l1_hal_tmr *)timer->bsp_timer;

    timer->expiry = tick;

    __HAL_DISABLE_INTERRUPTS(sr);

    if (TAILQ_EMPTY(&tmr->sht_timers)) {
        TAILQ_INSERT_HEAD(&tmr->sht_timers, timer, link);
    } else {
        TAILQ_FOREACH(ht, &tmr->sht_timers, link) {
            if ((int32_t)(timer->expiry - ht->expiry) < 0) {
                TAILQ_INSERT_BEFORE(ht, timer, link);
                break;
            }
        }
        if (!ht) {
            TAILQ_INSERT_TAIL(&tmr->sht_timers, timer, link);
        }
    }

    if ((int32_t)(tick - hal_timer_cnt(tmr)) <= 0) {
        /*
         * Event in the past (should be the case if it was just inserted).
         */
        tmr->sht_regs->EGR |= TIM_EGR_CC1G;
        tmr->sht_regs->DIER |= TIM_DIER_CC1IE;
    } else {
        if (timer == TAILQ_FIRST(&tmr->sht_timers)) {
            TIM_CCxChannelCmd(tmr->sht_regs, TIM_CHANNEL_1, TIM_CCx_ENABLE);
            tmr->sht_regs->CCR1 = timer->expiry;
            tmr->sht_regs->DIER |= TIM_DIER_CC1IE;
        }
    }
    __HAL_ENABLE_INTERRUPTS(sr);

    return 0;
}

/**
 * hal_timer_stop()
 *
 * Cancels the timer.
 *
 * @param timer
 *
 * @return int
 */
int
hal_timer_stop(struct hal_timer *timer)
{
    struct stm32l1_hal_tmr *tmr;
    struct hal_timer *ht;
    int sr;
    int reset_ocmp;

    __HAL_DISABLE_INTERRUPTS(sr);

    tmr = (struct stm32l1_hal_tmr *)timer->bsp_timer;
    if (timer->link.tqe_prev != NULL) {
        reset_ocmp = 0;
        if (timer == TAILQ_FIRST(&tmr->sht_timers)) {
            /* If first on queue, we will need to reset OCMP */
            ht = TAILQ_NEXT(timer, link);
            reset_ocmp = 1;
        }
        TAILQ_REMOVE(&tmr->sht_timers, timer, link);
        timer->link.tqe_prev = NULL;
        if (reset_ocmp) {
            if (ht) {
                tmr->sht_regs->CCR1 = ht->expiry;
            } else {
                TIM_CCxChannelCmd(tmr->sht_regs, TIM_CHANNEL_1,
                  TIM_CCx_DISABLE);
                tmr->sht_regs->DIER &= ~TIM_DIER_CC1IE;
            }
        }
    }
    __HAL_ENABLE_INTERRUPTS(sr);

    return 0;
}
#endif
