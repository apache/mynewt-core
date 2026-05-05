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

#include <os/mynewt.h>
#include <assert.h>
#include <stm32h5xx_hal.h>
#include <stm32h5xx_hal_pwr_ex.h>
#include <stm32h5xx_ll_rcc.h>
#include <stm32h5xx_ll_pwr.h>

/*
 * This allows an user to have a custom clock configuration by zeroing
 * every possible clock source in the syscfg.
 */
#if MYNEWT_VAL_STM32_CLOCK_HSE || MYNEWT_VAL_STM32_CLOCK_LSE ||               \
    MYNEWT_VAL_STM32_CLOCK_HSI || MYNEWT_VAL_STM32_CLOCK_LSI

/*
 * HSI is turned on by default, but can be turned off and use HSE instead.
 */
_Static_assert((MYNEWT_VAL_STM32_CLOCK_HSE != 0) + (MYNEWT_VAL_STM32_CLOCK_HSI != 0) >= 1,
               "At least one of HSE or HSI clock source must be enabled");

#define PLL_ON(n)       (MYNEWT_VAL_STM32_CLOCK_##n != 0)
#define PLL_OFF(n)      (MYNEWT_VAL_STM32_CLOCK_##n == 0)
#define PLL_VAL(n, val) MYNEWT_VAL_STM32_CLOCK_##n##_##val

_Static_assert(PLL_OFF(PLL1) || IS_RCC_PLLM_VALUE(PLL_VAL(PLL1, PLLM)),
               "PLLM value is invalid");
_Static_assert(PLL_OFF(PLL1) || IS_RCC_PLLN_VALUE(PLL_VAL(PLL1, PLLN)),
               "PLLN value is invalid");
_Static_assert(PLL_OFF(PLL1) || IS_RCC_PLLP_VALUE(PLL_VAL(PLL1, PLLP)),
               "PLLP value is invalid");
_Static_assert(PLL_OFF(PLL1) || IS_RCC_PLLQ_VALUE(PLL_VAL(PLL1, PLLQ)),
               "PLLQ value is invalid");
_Static_assert(PLL_OFF(PLL1) || IS_RCC_PLLR_VALUE(PLL_VAL(PLL1, PLLR)),
               "PLLR value is invalid");

_Static_assert(PLL_OFF(PLL2) || IS_RCC_PLLM_VALUE(PLL_VAL(PLL2, PLLM)),
               "PLLM value is invalid");
_Static_assert(PLL_OFF(PLL2) || IS_RCC_PLLN_VALUE(PLL_VAL(PLL2, PLLN)),
               "PLLN value is invalid");
_Static_assert(PLL_OFF(PLL2) || IS_RCC_PLLP_VALUE(PLL_VAL(PLL2, PLLP)),
               "PLLP value is invalid");
_Static_assert(PLL_OFF(PLL2) || IS_RCC_PLLQ_VALUE(PLL_VAL(PLL2, PLLQ)),
               "PLLQ value is invalid");
_Static_assert(PLL_OFF(PLL2) || IS_RCC_PLLR_VALUE(PLL_VAL(PLL2, PLLR)),
               "PLLR value is invalid");

_Static_assert(PLL_OFF(PLL3) || IS_RCC_PLLM_VALUE(PLL_VAL(PLL3, PLLM)),
               "PLLM value is invalid");
_Static_assert(PLL_OFF(PLL3) || IS_RCC_PLLN_VALUE(PLL_VAL(PLL3, PLLN)),
               "PLLN value is invalid");
_Static_assert(PLL_OFF(PLL3) || IS_RCC_PLLP_VALUE(PLL_VAL(PLL3, PLLP)),
               "PLLP value is invalid");
_Static_assert(PLL_OFF(PLL3) || IS_RCC_PLLQ_VALUE(PLL_VAL(PLL3, PLLQ)),
               "PLLQ value is invalid");
_Static_assert(PLL_OFF(PLL3) || IS_RCC_PLLR_VALUE(PLL_VAL(PLL3, PLLR)),
               "PLLR value is invalid");

_Static_assert(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE != LL_RCC_SYS_CLKSOURCE_PLL1 ||
                   PLL_ON(PLL1),
               "PLL1 selected as system clock but no enabled");

#define BUSY_LOOP(cond)                                                       \
    while (cond) {                                                            \
    }

void
SystemClock_Config(void)
{
    LL_PWR_SetRegulVoltageScaling(MYNEWT_VAL_STM32_CLOCK_VOLTAGESCALING_CONFIG);
    BUSY_LOOP(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY));

    __HAL_FLASH_SET_LATENCY(MYNEWT_VAL_STM32_FLASH_LATENCY);

    if (MYNEWT_VAL_STM32_CLOCK_LSI) {
        LL_RCC_LSI_Enable();
    }
    if (MYNEWT_VAL_STM32_CLOCK_LSE_BYPASS && !LL_RCC_LSE_IsReady()) {
        LL_PWR_EnableBkUpAccess();
        LL_RCC_LSE_EnableBypass();
    }
    if (MYNEWT_VAL_STM32_CLOCK_LSE && !LL_RCC_LSE_IsReady()) {
        LL_PWR_EnableBkUpAccess();
        LL_RCC_LSE_Enable();
    }
    if (MYNEWT_VAL_STM32_CLOCK_HSE_BYPASS) {
        LL_RCC_HSE_EnableBypass();
    }
    if (MYNEWT_VAL_STM32_CLOCK_HSE) {
        LL_RCC_HSE_Enable();
    }

    if (MYNEWT_VAL_STM32_CLOCK_HSI48) {
        LL_RCC_HSI48_Enable();
    }

    LL_RCC_SetAHBPrescaler(MYNEWT_VAL_STM32_CLOCK_AHB_DIVIDER);
    LL_RCC_SetAPB1Prescaler(MYNEWT_VAL_STM32_CLOCK_APB1_DIVIDER);
    LL_RCC_SetAPB2Prescaler(MYNEWT_VAL_STM32_CLOCK_APB2_DIVIDER);
    LL_RCC_SetAPB3Prescaler(MYNEWT_VAL_STM32_CLOCK_APB3_DIVIDER);

    if (MYNEWT_VAL_STM32_CLOCK_USBSEL) {
        LL_RCC_SetUSBClockSource(MYNEWT_VAL_STM32_CLOCK_USBSEL);
    }

    BUSY_LOOP(PLL_VAL(PLL1, SOURCE) == LL_RCC_PLL1SOURCE_HSE && !LL_RCC_HSE_IsReady());
    BUSY_LOOP(PLL_VAL(PLL1, SOURCE) == LL_RCC_PLL1SOURCE_HSI && !LL_RCC_HSI_IsReady());
    BUSY_LOOP(PLL_VAL(PLL1, SOURCE) == LL_RCC_PLL1SOURCE_CSI && !LL_RCC_CSI_IsReady());
    if (PLL_ON(PLL1)) {
        LL_RCC_PLL1_SetSource(MYNEWT_VAL_STM32_CLOCK_PLL1_SOURCE);
        LL_RCC_PLL1_SetM(MYNEWT_VAL_STM32_CLOCK_PLL1_PLLM);
        LL_RCC_PLL1_SetN(MYNEWT_VAL_STM32_CLOCK_PLL1_PLLN);
        LL_RCC_PLL1_SetP(MYNEWT_VAL_STM32_CLOCK_PLL1_PLLP);
        LL_RCC_PLL1_SetQ(MYNEWT_VAL_STM32_CLOCK_PLL1_PLLQ);
        LL_RCC_PLL1_SetR(MYNEWT_VAL_STM32_CLOCK_PLL1_PLLR);
        if (MYNEWT_VAL_STM32_CLOCK_PLL1_PLLFRACN) {
            LL_RCC_PLL1_SetFRACN(MYNEWT_VAL_STM32_CLOCK_PLL1_PLLFRACN);
        }
        LL_RCC_PLL1_SetVCOInputRange(MYNEWT_VAL_STM32_CLOCK_PLL1_RGE);
        LL_RCC_PLL1_Enable();
        LL_RCC_PLL1P_Enable();
        LL_RCC_PLL1Q_Enable();
    }

    BUSY_LOOP(PLL_VAL(PLL2, SOURCE) == LL_RCC_PLL1SOURCE_HSE && !LL_RCC_HSE_IsReady());
    BUSY_LOOP(PLL_VAL(PLL2, SOURCE) == LL_RCC_PLL1SOURCE_HSI && !LL_RCC_HSI_IsReady());
    BUSY_LOOP(PLL_VAL(PLL2, SOURCE) == LL_RCC_PLL1SOURCE_CSI && !LL_RCC_CSI_IsReady());
    if (PLL_ON(PLL2)) {
        LL_RCC_PLL2_SetSource(MYNEWT_VAL_STM32_CLOCK_PLL2_SOURCE);
        LL_RCC_PLL2_SetM(MYNEWT_VAL_STM32_CLOCK_PLL2_PLLM);
        LL_RCC_PLL2_SetN(MYNEWT_VAL_STM32_CLOCK_PLL2_PLLN);
        LL_RCC_PLL2_SetP(MYNEWT_VAL_STM32_CLOCK_PLL2_PLLP);
        LL_RCC_PLL2_SetQ(MYNEWT_VAL_STM32_CLOCK_PLL2_PLLQ);
        LL_RCC_PLL2_SetR(MYNEWT_VAL_STM32_CLOCK_PLL2_PLLR);
        if (MYNEWT_VAL_STM32_CLOCK_PLL2_PLLFRACN) {
            LL_RCC_PLL2_SetFRACN(MYNEWT_VAL_STM32_CLOCK_PLL2_PLLFRACN);
        }
        LL_RCC_PLL2_SetVCOInputRange(MYNEWT_VAL_STM32_CLOCK_PLL2_RGE);
        LL_RCC_PLL2_Enable();
        LL_RCC_PLL2P_Enable();
        LL_RCC_PLL2Q_Enable();
    }

    BUSY_LOOP(PLL_VAL(PLL3, SOURCE) == LL_RCC_PLL1SOURCE_HSE && !LL_RCC_HSE_IsReady());
    BUSY_LOOP(PLL_VAL(PLL3, SOURCE) == LL_RCC_PLL1SOURCE_HSI && !LL_RCC_HSI_IsReady());
    BUSY_LOOP(PLL_VAL(PLL3, SOURCE) == LL_RCC_PLL1SOURCE_CSI && !LL_RCC_CSI_IsReady());
    if (PLL_ON(PLL3)) {
        LL_RCC_PLL3_SetSource(MYNEWT_VAL_STM32_CLOCK_PLL3_SOURCE);
        LL_RCC_PLL3_SetM(MYNEWT_VAL_STM32_CLOCK_PLL3_PLLM);
        LL_RCC_PLL3_SetN(MYNEWT_VAL_STM32_CLOCK_PLL3_PLLN);
        LL_RCC_PLL3_SetP(MYNEWT_VAL_STM32_CLOCK_PLL3_PLLP);
        LL_RCC_PLL3_SetQ(MYNEWT_VAL_STM32_CLOCK_PLL3_PLLQ);
        LL_RCC_PLL3_SetR(MYNEWT_VAL_STM32_CLOCK_PLL3_PLLR);
        if (MYNEWT_VAL_STM32_CLOCK_PLL3_PLLFRACN) {
            LL_RCC_PLL3_SetFRACN(MYNEWT_VAL_STM32_CLOCK_PLL3_PLLFRACN);
        }
        LL_RCC_PLL3_SetVCOInputRange(MYNEWT_VAL_STM32_CLOCK_PLL3_RGE);
        LL_RCC_PLL3_Enable();
        LL_RCC_PLL3P_Enable();
        LL_RCC_PLL3Q_Enable();
    }
    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE == LL_RCC_SYS_CLKSOURCE_HSI &&
              !LL_RCC_HSI_IsReady());
    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE == LL_RCC_SYS_CLKSOURCE_HSE &&
              !LL_RCC_HSE_IsReady());
    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE == LL_RCC_SYS_CLKSOURCE_CSI &&
              !LL_RCC_CSI_IsReady());
    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE == LL_RCC_SYS_CLKSOURCE_PLL1 &&
              !LL_RCC_PLL1_IsReady());
    LL_RCC_SetSysClkSource(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE);
}

#endif
