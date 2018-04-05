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
#include <hal/hal_system.h>
#include <mcu/cortex_m4.h>
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_rcc.h"

void
hal_system_clock_start(void)
{
#if MYNEWT_VAL(MCU_SYSCLK_PLL_HSI)
    RCC_OscInitTypeDef osc;
    RCC_ClkInitTypeDef clk;

    osc.OscillatorType      = RCC_OSCILLATORTYPE_NONE;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
#if defined(RCC_CFGR_PLLSRC_HSI_PREDIV)
    osc.PLL.PREDIV          = RCC_PREDIV_DIV2;
#endif
    osc.PLL.PLLMUL          = RCC_PLL_MUL16;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    assert(HAL_OK == HAL_RCC_OscConfig(&osc));

    clk.ClockType       = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider   = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider  = RCC_HCLK_DIV2;
    clk.APB2CLKDivider  = RCC_HCLK_DIV1;
    assert(HAL_OK == HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2));
#endif
}
