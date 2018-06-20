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

#include "os/mynewt.h"

#include <assert.h>
#include <inttypes.h>
#include "stm32f7xx_hal_rcc.h"

static uint32_t
stm32_hal_timer_abp_clk_div(uint32_t divider)
{
    switch (divider) {
    case RCC_HCLK_DIV1:
        return 1;
    case RCC_HCLK_DIV2:
        return 2;
    case RCC_HCLK_DIV4:
        return 4;
    case RCC_HCLK_DIV8:
        return 8;
    case RCC_HCLK_DIV16:
        return 16;
    }
    return 0;
}

uint32_t
stm32_hal_timer_get_freq(void *timx)
{
    uintptr_t regs = (uintptr_t)timx;
    RCC_ClkInitTypeDef clocks;
    uint32_t flashLatency;
    uint32_t freq = 0;
    uint32_t div = 0;

    HAL_RCC_GetClockConfig(&clocks, &flashLatency);

    switch (regs) {
#ifdef TIM1
    case (uintptr_t)TIM1:
#endif
#ifdef TIM8
    case (uintptr_t)TIM8:
#endif
#ifdef TIM9
    case (uintptr_t)TIM9:
#endif
#ifdef TIM10
    case (uintptr_t)TIM10:
#endif
#ifdef TIM11
    case (uintptr_t)TIM11:
#endif
#ifdef TIM15
    case (uintptr_t)TIM15:
#endif
#ifdef TIM16
    case (uintptr_t)TIM16:
#endif
#ifdef TIM17
    case (uintptr_t)TIM17:
#endif
#ifdef TIM20
    case (uintptr_t)TIM20:
#endif
        freq = HAL_RCC_GetPCLK2Freq();
        div  = stm32_hal_timer_abp_clk_div(clocks.APB2CLKDivider);
        break;

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
        freq = HAL_RCC_GetPCLK1Freq();
        div  = stm32_hal_timer_abp_clk_div(clocks.APB1CLKDivider);
        break;

    default:
        assert(0);
        return 0;
    }

    if (RCC_TIMPRES_ACTIVATED == READ_BIT(RCC->DCKCFGR1, RCC_DCKCFGR1_TIMPRE)) {
        if (div > 2) {
            return freq * 4;
        }
        return freq * div;
    }

    if (div > 1) {
        return freq * 2;
    }
    return freq;
}
