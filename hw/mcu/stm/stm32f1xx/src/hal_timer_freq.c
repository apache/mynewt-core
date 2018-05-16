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

#include <hal/hal_timer.h>
#include "mcu/stm32_hal.h"
#include "stm32_common/stm32_hal.h"


/*
 * Generic implementation for determining the frequency
 * of a timer.
 */

uint32_t
stm32_hal_timer_get_freq(void *regs)
{
    RCC_ClkInitTypeDef clocks;
    uint32_t fl;
    uint32_t freq;

    HAL_RCC_GetClockConfig(&clocks, &fl);

    /*
     * Assuming RCC_DCKCFGR->TIMPRE is 0.
     * There's just APB2 timers here.
     */
    switch ((uintptr_t)regs) {
#ifdef TIM1
    case (uintptr_t)TIM1:
#endif
#ifdef TIM8
    case (uintptr_t)TIM8:
#endif
#ifdef TIM9
    case (uuintptr_t)TIM9:
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
        freq = HAL_RCC_GetPCLK2Freq();
        if (clocks.APB2CLKDivider) {
            freq *= 2;
        }
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
        freq = HAL_RCC_GetPCLK1Freq();
        if (clocks.APB1CLKDivider) {
            freq *= 2;
        }
        break;
    default:
        return 0;
    }
    return freq;
}
