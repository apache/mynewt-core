/*
 * <h2><center>&copy; COPYRIGHT 2016 STMicroelectronics</center></h2>
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_rcc.h"
#include <assert.h>

/*
 * This allows an user to have a custom clock configuration by zeroing
 * every possible clock source in the syscfg.
 */
#if MYNEWT_VAL(STM32_CLOCK_HSE) || MYNEWT_VAL(STM32_CLOCK_LSE) || \
    MYNEWT_VAL(STM32_CLOCK_HSI) || MYNEWT_VAL(STM32_CLOCK_LSI)

/*
 * HSI is turned on by default, but can be turned off and use HSE instead.
 */
#if (((MYNEWT_VAL(STM32_CLOCK_HSE) != 0) + (MYNEWT_VAL(STM32_CLOCK_HSI) != 0)) < 1)
#error "At least one of HSE or HSI clock source must be enabled"
#endif

#define BUSY_LOOP(cond)                                                       \
    while (cond) {                                                            \
    }

_Static_assert(IS_RCC_PLL_MUL(MYNEWT_VAL(STM32_CLOCK_PLL_MUL)), "Invalid PLL MUL");
_Static_assert(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE != LL_RCC_SYS_CLKSOURCE_PLL ||
                   MYNEWT_VAL_STM32_CLOCK_PLL,
               "PLL not enable and selected as system clock");

void
SystemClock_Config(void)
{
    __HAL_FLASH_SET_LATENCY(MYNEWT_VAL(STM32_FLASH_LATENCY));

    if (MYNEWT_VAL(STM32_CLOCK_LSI)) {
        LL_RCC_LSI_Enable();
    }
    if (MYNEWT_VAL(STM32_CLOCK_LSE_BYPASS)) {
        LL_RCC_LSE_EnableBypass();
    }
    if (MYNEWT_VAL(STM32_CLOCK_LSE)) {
        LL_RCC_LSE_Enable();
    }
    if (MYNEWT_VAL(STM32_CLOCK_HSE_BYPASS)) {
        LL_RCC_HSE_EnableBypass();
    }
    if (MYNEWT_VAL(STM32_CLOCK_HSE)) {
        LL_RCC_HSE_Enable();
    }

    LL_RCC_SetAHBPrescaler(MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER));
    LL_RCC_SetAPB1Prescaler(MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER));
    LL_RCC_SetAPB2Prescaler(MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER));

    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_PLL_SOURCE == LL_RCC_PLLSOURCE_HSE &&
              !LL_RCC_HSE_IsReady());
    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_PLL_SOURCE == LL_RCC_PLLSOURCE_HSI_DIV_2 &&
              !LL_RCC_HSI_IsReady());
    if (MYNEWT_VAL_STM32_CLOCK_PLL) {
        LL_RCC_PLL_ConfigDomain_SYS(MYNEWT_VAL_STM32_CLOCK_PLL_SOURCE |
                                        MYNEWT_VAL_STM32_CLOCK_PREDIV,
                                    MYNEWT_VAL_STM32_CLOCK_PLL_MUL);
        LL_RCC_PLL_Enable();
    }
    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE == LL_RCC_PLLSOURCE_HSI_DIV_2 &&
              !LL_RCC_HSI_IsReady());
    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE == LL_RCC_SYS_CLKSOURCE_HSE &&
              !LL_RCC_HSE_IsReady());
    BUSY_LOOP(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE == LL_RCC_SYS_CLKSOURCE_PLL &&
              !LL_RCC_PLL_IsReady());
    LL_RCC_SetSysClkSource(MYNEWT_VAL_STM32_CLOCK_SYSCLK_SOURCE);

    if (MYNEWT_VAL_STM32_CLOCK_PLL_SOURCE == LL_RCC_PLLSOURCE_HSI_DIV_2) {
        LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_PLL);
    }
    if (MYNEWT_VAL_STM32_CLOCK_PLL_SOURCE == LL_RCC_PLLSOURCE_HSE) {
        LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_PLL_DIV_1_5);
    }
}

#endif
