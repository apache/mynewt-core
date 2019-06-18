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

#include "stm32f4xx_hal_pwr_ex.h"
#include "stm32f4xx_hal.h"
#include <assert.h>

#if (MYNEWT_VAL(STM32_CLOCK_HSE) != 0) + \
    (MYNEWT_VAL(STM32_CLOCK_LSE) != 0) + \
    (MYNEWT_VAL(STM32_CLOCK_HSI) != 0) + \
    (MYNEWT_VAL(STM32_CLOCK_LSI) != 0) > 1
#error "Only one of HSE/LSE/HSI/LSI source can be enabled"
#endif

#if MYNEWT_VAL(STM32_CLOCK_HSE) || MYNEWT_VAL(STM32_CLOCK_LSE) || \
    MYNEWT_VAL(STM32_CLOCK_HSI) || MYNEWT_VAL(STM32_CLOCK_LSI)
void
SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc_init;
    RCC_ClkInitTypeDef clk_init;
    HAL_StatusTypeDef status;

    /*
     * Enable Power Control clock
     */
    __HAL_RCC_PWR_CLK_ENABLE();

    /*
     * The voltage scaling allows optimizing the power consumption when the
     * device is clocked below the maximum system frequency, to update the
     * voltage scaling value regarding system frequency refer to product
     * datasheet.
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(MYNEWT_VAL(STM32_CLOCK_VOLTAGESCALING_CONFIG));

#if (MYNEWT_VAL(STM32_CLOCK_HSE) != 0)

    /*
     * Enable HSE Oscillator and activate PLL with HSE as source
     */
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;

#if (MYNEWT_VAL(STM32_CLOCK_HSE_BYPASS) != 0)
    osc_init.HSEState = RCC_HSE_BYPASS;
#else
    osc_init.HSEState = RCC_HSE_ON;
#endif

    osc_init.PLL.PLLState = RCC_PLL_ON;
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;

#elif (MYNEWT_VAL(STM32_CLOCK_LSE) != 0)
#error "Using LSE as clock source is not implemented at this moment"

#elif (MYNEWT_VAL(STM32_CLOCK_HSI) != 0)
#error "Using HSI as clock source is not implemented at this moment"

#elif (MYNEWT_VAL(STM32_CLOCK_LSI) != 0)
#error "Using LSI as clock source is not implemented at this moment"

#endif

#if !IS_RCC_PLLM_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLM))
#error "PLLM value is invalid"
#endif

#if !IS_RCC_PLLN_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLN))
#error "PLLN value is invalid"
#endif

#if !IS_RCC_PLLP_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLP))
#error "PLLP value is invalid"
#endif

#if !IS_RCC_PLLQ_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLQ))
#error "PLLQ value is invalid"
#endif

    osc_init.PLL.PLLM = MYNEWT_VAL(STM32_CLOCK_PLL_PLLM);
    osc_init.PLL.PLLN = MYNEWT_VAL(STM32_CLOCK_PLL_PLLN);
    osc_init.PLL.PLLP = MYNEWT_VAL(STM32_CLOCK_PLL_PLLP);
    osc_init.PLL.PLLQ = MYNEWT_VAL(STM32_CLOCK_PLL_PLLQ);

#if (MYNEWT_VAL(STM32_CLOCK_PLL_PLLR) != 0)

#if !IS_RCC_PLLR_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLR))
#error "PLLR value is invalid"
#endif

    osc_init.PLL.PLLR = MYNEWT_VAL(STM32_CLOCK_PLL_PLLR);

#endif /* MYNEWT_VAL(STM32_CLOCK_PLL_PLLR) != 0 */

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
        while (1) { }
    }

#if (MYNEWT_VAL(STM32_CLOCK_HSE) != 0) || (MYNEWT_VAL(STM32_CLOCK_HSI) != 0)
#if MYNEWT_VAL(STM32_CLOCK_ENABLE_OVERDRIVE)
    /*
     * Activate the Over-Drive mode
     */
    status = HAL_PWREx_EnableOverDrive();
    if (status != HAL_OK) {
        assert(0);
        while (1) { }
    }
#endif /* MYNEWT_VAL(STM32_CLOCK_ENABLE_OVERDRIVE) */
#endif

    /*
     * Select PLL as system clock source and configure the HCLK, PCLK1 and
     * PCLK2 clocks dividers
     */
    clk_init.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;

#if !IS_RCC_HCLK(MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER))
#error "AHB clock divider is invalid"
#endif

#if !IS_RCC_PCLK(MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER))
#error "APB1 clock divider is invalid"
#endif

#if !IS_RCC_PCLK(MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER))
#error "APB2 clock divider is invalid"
#endif

    clk_init.AHBCLKDivider = MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER);
    clk_init.APB1CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER);
    clk_init.APB2CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER);

#if !IS_FLASH_LATENCY(MYNEWT_VAL(STM32_FLASH_LATENCY))
#error "Flash latency value is invalid"
#endif

    status = HAL_RCC_ClockConfig(&clk_init, MYNEWT_VAL(STM32_FLASH_LATENCY));
    if (status != HAL_OK) {
        assert(0);
        while (1) { }
    }


#if PREFETCH_ENABLE
#if defined(STM32F405xx) || defined(STM32F415xx) || defined(STM32F407xx) || \
    defined(STM32F417xx)
    /* RevA (prefetch must be off) or RevZ (prefetch can be on/off) */
    if (HAL_GetREVID() == 0x1001) {
        __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    }
#else
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
#endif
#endif

#if INSTRUCTION_CACHE_ENABLE
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
#endif

#if DATA_CACHE_ENABLE
    __HAL_FLASH_DATA_CACHE_ENABLE();
#endif
}
#endif
