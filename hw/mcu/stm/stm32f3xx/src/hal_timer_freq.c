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

#include <inttypes.h>
#include <assert.h>

#include "os/mynewt.h"
#include "stm32f3xx_hal_rcc.h"

static uint32_t
stm32_hal_timer_get_freq_ahb1(RCC_ClkInitTypeDef *clk) {
    uint32_t freq = HAL_RCC_GetPCLK1Freq();

    if (clk->APB1CLKDivider != RCC_HCLK_DIV1) {
        return 2 * freq;
    }

    return freq;
}

static uint32_t
stm32_hal_timer_get_freq_ahb2(RCC_ClkInitTypeDef *clk, bool pll2source)
{
    uint32_t freq = HAL_RCC_GetPCLK2Freq();

    if (pll2source && RCC_HCLK_DIV1 == clk->APB2CLKDivider && RCC_SYSCLK_DIV1 == clk->AHBCLKDivider) {
        return 2 * freq;
    }

    if (clk->APB2CLKDivider != RCC_HCLK_DIV1) {
        return 2 * freq;
    }

    return freq;
}

uint32_t
stm32_hal_timer_get_freq(void *timx)
{
    uintptr_t regs = (uintptr_t)timx;
    RCC_ClkInitTypeDef clk;
    uint32_t flashLatency;

    HAL_RCC_GetClockConfig(&clk, &flashLatency);

    switch (regs) {
#ifdef TIM1
    case (uintptr_t)TIM1:
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM1_SOURCE());
#endif
#ifdef TIM8
    case (uintptr_t)TIM8:
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM8_SOURCE());
#endif
#ifdef TIM9
    case (uintptr_t)TIM9:
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM9_SOURCE());
#endif
#ifdef TIM10
    case (uintptr_t)TIM10:
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM10_SOURCE());
#endif
#ifdef TIM11
    case (uintptr_t)TIM11:
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM11_SOURCE());
#endif
#ifdef TIM15
    case (uintptr_t)TIM15:
#   ifdef __HAL_RCC_GET_TIM15_SOURCE
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM15_SOURCE());
#   else
        return stm32_hal_timer_get_freq_ahb2(&clk, false);
#   endif
#endif
#ifdef TIM16
    case (uintptr_t)TIM16:
#   ifdef __HAL_RCC_GET_TIM16_SOURCE
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM16_SOURCE());
#   else
        return stm32_hal_timer_get_freq_ahb2(&clk, false);
#   endif
#endif
#ifdef TIM17
    case (uintptr_t)TIM17:
#   ifdef __HAL_RCC_GET_TIM17_SOURCE
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM17_SOURCE());
#   else
        return stm32_hal_timer_get_freq_ahb2(&clk, false);
#   endif
#endif
#ifdef TIM20
    case (uintptr_t)TIM20:
#   ifdef __HAL_RCC_GET_TIM20_SOURCE
        return stm32_hal_timer_get_freq_ahb2(&clk, __HAL_RCC_GET_TIM20_SOURCE());
#   else
        return stm32_hal_timer_get_freq_ahb2(&clk, false);
#   endif
#endif
        /* AHB1 timers ... */
#ifdef TIM2
    case (uintptr_t)TIM2:
#endif
#ifdef TIM3
    case (uintptr_t)TIM3:
#endif
#ifdef TIM4
    case (uintptr_t)TIM4:
#endif
#ifdef TIM5
    case (uintptr_t)TIM5:
#endif
#ifdef TIM6
    case (uintptr_t)TIM6:
#endif
#ifdef TIM7
    case (uintptr_t)TIM7:
#endif

#ifdef TIM12
    case (uintptr_t)TIM12:
#endif
#ifdef TIM13
    case (uintptr_t)TIM13:
#endif
#ifdef TIM14
    case (uintptr_t)TIM14:
#endif
        return stm32_hal_timer_get_freq_ahb1(&clk);
    }

    assert(0);
    return 0;
}

