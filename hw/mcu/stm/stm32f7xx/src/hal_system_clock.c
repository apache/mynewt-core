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

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_rcc.h"
#include <assert.h>
#include <hal/hal_system.h>
#include <mcu/cortex_m7.h>

void
hal_system_clock_start(void)
{
#if MYNEWT_VAL(MCU_SYSCLK_PLL_HSI)
    RCC_OscInitTypeDef osc;
    RCC_ClkInitTypeDef clk;

    /*
     * CLK_IN       = HSI                   ... 16MHz
     * PLL_CLK_OUT  = CLK_IN / PLLM * PLLN  ... 436MHz
     * PLLCLK       = PLL_CLK_OUT / PLLP    ... SYSCLK
     * PLL48CLK     = PLL_CLK_OUT / PLLQ    ... USB clock
     * PLLDSICLK    = PLL_CLK_OUT / PLLR    ... DSI host
     */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_NONE;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 16;
    osc.PLL.PLLN            = 432;  
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = 9;
    osc.PLL.PLLR            = 7;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    assert(HAL_OK == HAL_RCC_OscConfig(&osc));

    clk.ClockType       = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider   = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider  = RCC_HCLK_DIV4;
    clk.APB2CLKDivider  = RCC_HCLK_DIV2;
    assert(HAL_OK == HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_7));
#endif
}
