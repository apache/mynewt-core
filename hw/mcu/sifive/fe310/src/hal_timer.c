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

#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include "os/mynewt.h"
#include <hal/hal_timer.h>
#include <mcu/plic.h>
#include <mcu/fe310_hal.h>
#include <env/freedom-e300-hifive1/platform.h>

#define FE310_HAL_TIMER_MAX     (3)

struct fe310_hal_tmr {
    void *pwm_regs;         /* Pointer to timer registers */
    uint32_t value;         /* Acumulated timer value, incremented on CMP0 */
    uint8_t max_scale;      /* Max value for pwmcfg.pwmscale 7 (for PMW0) or 15 (for PWM1/2) */
    uint8_t pwmxcmp0_int;   /* PWMxCMP0 interrupt number */
    TAILQ_HEAD(hal_timer_qhead, hal_timer) sht_timers;
};

#if MYNEWT_VAL(TIMER_0)
struct fe310_hal_tmr fe310_pwm2 = {
    (uint32_t *) PWM2_CTRL_ADDR, 0, 15, INT_PWM2_BASE
};
#endif
#if MYNEWT_VAL(TIMER_1)
struct fe310_hal_tmr fe310_pwm1 = {
    (uint32_t *) PWM1_CTRL_ADDR, 0, 15, INT_PWM1_BASE
};
#endif
#if MYNEWT_VAL(TIMER_2)
struct fe310_hal_tmr fe310_pwm0 = {
    (uint32_t *) PWM0_CTRL_ADDR, 0, 7, INT_PWM0_BASE
};
#endif

static struct fe310_hal_tmr *fe310_tmr_devs[FE310_HAL_TIMER_MAX] = {
#if MYNEWT_VAL(TIMER_0)
    &fe310_pwm2,
#else
    NULL,
#endif
#if MYNEWT_VAL(TIMER_1)
    &fe310_pwm1,
#else
    NULL,
#endif
#if MYNEWT_VAL(TIMER_2)
    &fe310_pwm0,
#else
    NULL,
#endif
};

static uint8_t pwm_to_timer[FE310_HAL_TIMER_MAX];

static uint32_t
hal_timer_cnt(struct fe310_hal_tmr *tmr)
{
    uint32_t cnt;
    int sr;
    uint32_t regs = (uint32_t) tmr->pwm_regs;

    __HAL_DISABLE_INTERRUPTS(sr);
    cnt = _REG32(regs, PWM_S) + tmr->value;
    /* Check if just overflowed */
    if (_REG32(regs, PWM_CFG) & PWM_CMP0) {
        cnt += _REG32(regs, PWM_CMP0) + 1;
    }
    __HAL_ENABLE_INTERRUPTS(sr);

    return cnt;
}

static void
fe310_tmr_check_first(struct fe310_hal_tmr *tmr)
{
    struct hal_timer *ht;

    ht = TAILQ_FIRST(&tmr->sht_timers);
    if (ht) {
        uint32_t cnt = hal_timer_cnt(tmr);
        int32_t ticks = (int32_t)(ht->expiry - cnt);
        if (ticks < _REG32(tmr->pwm_regs, PWM_CMP0)) {
            _REG32(tmr->pwm_regs, PWM_CMP1) = ticks;
            plic_enable_interrupt(tmr->pwmxcmp0_int + 1);
            return;
        }
    }
    _REG32(tmr->pwm_regs, PWM_CMP1) = _REG32(tmr->pwm_regs, PWM_CMP0);
     /* Disable PWMxCMP1 interrupt, leaving only CMP0 which is used all the time */
    plic_disable_interrupt(tmr->pwmxcmp0_int + 1);
}

/*
 * Call expired timer callbacks
 */
static void
fe310_tmr_cbs(struct fe310_hal_tmr *tmr)
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
    fe310_tmr_check_first(tmr);
}

void
fe310_pwm_cmp0_handler(int num)
{
    int pwm_num = (num - INT_PWM0_BASE) >> 2;
    int timer_num = pwm_to_timer[pwm_num];
    struct fe310_hal_tmr *tmr = fe310_tmr_devs[timer_num];
    /* Turn of CMPxIP */
    _REG32(tmr->pwm_regs, PWM_CFG) &= ~PWM_CFG_CMP0IP;
    tmr->value += _REG32(tmr->pwm_regs, PWM_CMP0) + 1;
    fe310_tmr_cbs(tmr);
}

void
fe310_pwm_cmp1_handler(int num)
{
    int pwm_num = (num - INT_PWM0_BASE) >> 2;
    int timer_num = pwm_to_timer[pwm_num];
    struct fe310_hal_tmr *tmr = fe310_tmr_devs[timer_num];
    /* Turn of CMPxIP */
    _REG32(tmr->pwm_regs, PWM_CFG) &= ~PWM_CFG_CMP1IP;
    fe310_tmr_cbs(tmr);
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
hal_timer_init(int timer_num, void *cfg)
{
    struct fe310_hal_tmr *tmr;

    if (timer_num >= FE310_HAL_TIMER_MAX || !(tmr = fe310_tmr_devs[timer_num]) ||
        (cfg == NULL)) {
        return -1;
    }

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
hal_timer_config(int timer_num, uint32_t freq_hz)
{
    struct fe310_hal_tmr *tmr;
    uint32_t cpu_freq;
    uint32_t div;
    int scale = -1;

    if (timer_num >= FE310_HAL_TIMER_MAX || !(tmr = fe310_tmr_devs[timer_num])) {
        return -1;
    }

    cpu_freq = get_cpu_freq();
    div = cpu_freq / freq_hz;

    if (div > (1 << tmr->max_scale) || div == 0) {
        return -1;
    }

    div += div >> 1;
    while (div) {
        div >>= 1;
        scale++;
    }

    _REG32(tmr->pwm_regs, PWM_CFG) = 0;
    _REG32(tmr->pwm_regs, PWM_COUNT) = 0;
    /* 15 -> 0xFFFF, 7 -> 0xFF*/
    _REG32(tmr->pwm_regs, PWM_CMP0) = (1 << (1 + tmr->max_scale)) - 1;
    _REG32(tmr->pwm_regs, PWM_CMP1) = _REG32(tmr->pwm_regs, PWM_CMP0);
    pwm_to_timer[(tmr->pwmxcmp0_int - INT_PWM0_BASE) >> 2] = (uint8_t) timer_num;
    plic_set_handler(tmr->pwmxcmp0_int, fe310_pwm_cmp0_handler, 3);
    plic_set_handler(tmr->pwmxcmp0_int + 1, fe310_pwm_cmp1_handler, 3);
    _REG32(tmr->pwm_regs, PWM_CFG) = PWM_CFG_ZEROCMP |
                                     PWM_CFG_ENALWAYS | scale;
    plic_enable_interrupt(tmr->pwmxcmp0_int);

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
hal_timer_deinit(int timer_num)
{
    struct fe310_hal_tmr *tmr;
    int sr;

    if (timer_num >= FE310_HAL_TIMER_MAX || !(tmr = fe310_tmr_devs[timer_num])) {
        return -1;
    }

    __HAL_DISABLE_INTERRUPTS(sr);
    _REG32(tmr->pwm_regs, PWM_CFG) = 0;
    plic_disable_interrupt(tmr->pwmxcmp0_int);
    plic_disable_interrupt(tmr->pwmxcmp0_int + 1);
    __HAL_ENABLE_INTERRUPTS(sr);

    return 0;
}

/**
 * hal timer get resolution
 *
 * Get the resolution of the timer. This is the timer period, in nanoseconds
 *
 * @param timer_num
 *
 * @return uint32_t The
 */
uint32_t
hal_timer_get_resolution(int timer_num)
{
    struct fe310_hal_tmr *tmr;
    uint32_t cpu_freq;

    if (timer_num >= FE310_HAL_TIMER_MAX || !(tmr = fe310_tmr_devs[timer_num])) {
        return -1;
    }

    cpu_freq = get_cpu_freq();

    return 1000000000 / (cpu_freq >> (_REG32(tmr->pwm_regs, PWM_CFG) & PWM_CFG_SCALE));
}

/**
 * hal timer read
 *
 * Returns the timer counter.
 *
 * @return uint32_t The timer counter register.
 */
uint32_t
hal_timer_read(int timer_num)
{
    struct fe310_hal_tmr *tmr;

    if (timer_num >= FE310_HAL_TIMER_MAX || !(tmr = fe310_tmr_devs[timer_num])) {
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
hal_timer_delay(int timer_num, uint32_t ticks)
{
    struct fe310_hal_tmr *tmr;
    uint32_t until;

    if (timer_num >= FE310_HAL_TIMER_MAX || !(tmr = fe310_tmr_devs[timer_num])) {
        return -1;
    }

    until = hal_timer_cnt(tmr) + ticks;
    while ((int32_t)(hal_timer_cnt(tmr) - until) <= 0) {
        /* Loop here till finished */
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
hal_timer_set_cb(int timer_num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    struct fe310_hal_tmr *tmr;

    if (timer_num >= FE310_HAL_TIMER_MAX || !(tmr = fe310_tmr_devs[timer_num])) {
        return -1;
    }

    timer->cb_func = cb_func;
    timer->cb_arg = arg;
    timer->link.tqe_prev = NULL;
    timer->bsp_timer = tmr;

    return 0;
}

int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    struct fe310_hal_tmr *tmr;
    uint32_t tick;

    tmr = (struct fe310_hal_tmr *)timer->bsp_timer;

    tick = ticks + hal_timer_cnt(tmr);
    return hal_timer_start_at(timer, tick);
}

int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    struct fe310_hal_tmr *tmr;
    struct hal_timer *ht;
    int sr;
    bool first = true;

    tmr = (struct fe310_hal_tmr *)timer->bsp_timer;

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
            first = false;
        }
        if (!ht) {
            TAILQ_INSERT_TAIL(&tmr->sht_timers, timer, link);
        }
    }

    if (first) {
        fe310_tmr_check_first(tmr);
    }

    __HAL_ENABLE_INTERRUPTS(sr);

    return 0;
}

/**
 * hal timer stop
 *
 * Stop a timer.
 *
 * @param timer
 *
 * @return int
 */
int
hal_timer_stop(struct hal_timer *timer)
{
    struct fe310_hal_tmr *tmr;
    int sr;
    bool recalc = false;

    __HAL_DISABLE_INTERRUPTS(sr);

    tmr = (struct fe310_hal_tmr *)timer->bsp_timer;
    /* Is it still in the list ? */
    if (timer->link.tqe_prev != NULL) {
        /* Recalculation only if removing first timer */
        recalc = (timer == TAILQ_FIRST(&tmr->sht_timers));
        TAILQ_REMOVE(&tmr->sht_timers, timer, link);
        timer->link.tqe_prev = NULL;

        if (recalc) {
            fe310_tmr_check_first(tmr);
        }
    }
    __HAL_ENABLE_INTERRUPTS(sr);

    return 0;
}
