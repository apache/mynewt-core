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
#include "mcu/da1469x_clock.h"
#include "mcu/da1469x_hal.h"
#include "mcu/cmsis_nvic.h"
#include "hal/hal_os_tick.h"
#include "hal/hal_system.h"
#include "os/os_trace_api.h"
#include "da1469x_priv.h"
#include "mcu/mcu.h"

struct hal_os_tick {
    uint32_t os_ticks_per_sec;          /* Configured upon init */
    uint32_t cycles_per_ostick;         /* For generating the OS ticks */
    uint32_t cycles_per_256_osticks;    /* For more precise OS Time calculation */
    uint32_t os_tick_residual;
    os_time_t max_idle_ticks;
    uint32_t last_trigger_val;
};

struct hal_os_tick g_hal_os_tick;

/*
 * Implement (x - y) where the range of both 'x' and 'y' is limited to 24-bits.
 *
 * For example:
 *
 * sub24(0, 0xffffff) = 1
 * sub24(0xffffff, 0xfffffe) = 1
 * sub24(0xffffff, 0) = -1
 * sub24(0x7fffff, 0) = 8388607
 * sub24(0x800000, 0) = -8388608
 */
static inline int
sub24(uint32_t x, uint32_t y)
{
    int result;

    assert(x <= 0xffffff);
    assert(y <= 0xffffff);

    result = x - y;
    if (result & 0x800000) {
        return (result | 0xff800000);
    } else {
        return (result & 0x007fffff);
    }
}

static inline uint32_t
hal_os_tick_get_timer_val(void)
{
    return TIMER2->TIMER2_TIMER_VAL_REG;
}

static inline void
hal_os_tick_set_timer_trigger_val(uint32_t trigger_val)
{
    uint32_t timer_val;
    int delta;

    while (1) {
        trigger_val &= 0xffffff;
        TIMER2->TIMER2_RELOAD_REG = trigger_val;
        timer_val = hal_os_tick_get_timer_val();

        /* XXX Not sure what happens if we set TIMER2_RELOAD_REG to the same
         *     value as TIMER2_TIMER_VAL_REG, so just in case this does not
         *     trigger interrupt let's just move to next tick.
         */
        delta = sub24(trigger_val, timer_val);
        if (delta > 0) {
            break;
        }

        trigger_val += g_hal_os_tick.cycles_per_ostick;
    }
}

static void
hal_os_tick_handler(void)
{
    uint32_t primask;
    uint32_t timer_val;
    uint32_t delta_x256;
    uint32_t ticks;

    __HAL_DISABLE_INTERRUPTS(primask);

    /* Calculate elapsed cycles of timer & record its current value */
    timer_val = hal_os_tick_get_timer_val();
    delta_x256 = ((timer_val - g_hal_os_tick.last_trigger_val) & 0xffffff) << 8;
    g_hal_os_tick.last_trigger_val = timer_val;

    /* Clear timer interrupt */
    TIMER2->TIMER2_CLEAR_IRQ_REG = 1;

    /* Re-arm timer for the next OS tick */
    hal_os_tick_set_timer_trigger_val(timer_val + g_hal_os_tick.cycles_per_ostick);

    /* Update OS Time */
    ticks = delta_x256 / g_hal_os_tick.cycles_per_256_osticks;
    g_hal_os_tick.os_tick_residual += delta_x256 % g_hal_os_tick.cycles_per_256_osticks;
    ticks += g_hal_os_tick.os_tick_residual / g_hal_os_tick.cycles_per_256_osticks;
    g_hal_os_tick.os_tick_residual %= g_hal_os_tick.cycles_per_256_osticks;
    os_time_advance(ticks);

    __HAL_ENABLE_INTERRUPTS(primask);
}

static void
hal_os_tick_timer2_isr(void)
{
    os_trace_isr_enter();

    hal_os_tick_handler();

    os_trace_isr_exit();
}

void
hal_os_tick_calc_params(uint32_t cycles_per_sec)
{
    /* Upon imit, `os_ticks_per_sec` becomes available only after clock setup - skip for now */
    if (g_hal_os_tick.os_ticks_per_sec == 0) {
        return;
    }

    g_hal_os_tick.cycles_per_256_osticks = (cycles_per_sec << 8) / g_hal_os_tick.os_ticks_per_sec;
    g_hal_os_tick.cycles_per_ostick = g_hal_os_tick.cycles_per_256_osticks >> 8;
    g_hal_os_tick.max_idle_ticks = (1UL << 22) / g_hal_os_tick.cycles_per_ostick;
}

void
os_tick_idle(os_time_t ticks)
{
    uint32_t new_trigger_val;

    OS_ASSERT_CRITICAL();

    if (ticks > 0) {
        if (ticks > g_hal_os_tick.max_idle_ticks) {
            ticks = g_hal_os_tick.max_idle_ticks;
        }

        new_trigger_val = g_hal_os_tick.last_trigger_val +
                          (ticks * g_hal_os_tick.cycles_per_ostick);

        hal_os_tick_set_timer_trigger_val(new_trigger_val);
    }

    da1469x_sleep(ticks);

    if (ticks > 0) {
        hal_os_tick_handler();
    }
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t primask;

    g_hal_os_tick.os_ticks_per_sec = os_ticks_per_sec;
    g_hal_os_tick.last_trigger_val = 0;
    g_hal_os_tick.os_tick_residual = 0;
    hal_os_tick_calc_params(da1469x_clock_lp_freq_get());

    TIMER2->TIMER2_CTRL_REG = 0;
    TIMER2->TIMER2_PRESCALER_REG = 0;
    TIMER2->TIMER2_CTRL_REG |= TIMER2_TIMER2_CTRL_REG_TIM_CLK_EN_Msk;
    TIMER2->TIMER2_CTRL_REG |= (TIMER2_TIMER2_CTRL_REG_TIM_FREE_RUN_MODE_EN_Msk |
                                TIMER2_TIMER2_CTRL_REG_TIM_IRQ_EN_Msk |
                                TIMER2_TIMER2_CTRL_REG_TIM_EN_Msk);

    __HAL_DISABLE_INTERRUPTS(primask);

    NVIC_SetPriority(TIMER2_IRQn, prio);
    NVIC_SetVector(TIMER2_IRQn, (uint32_t)hal_os_tick_timer2_isr);
    NVIC_EnableIRQ(TIMER2_IRQn);

    __HAL_ENABLE_INTERRUPTS(primask);
}
