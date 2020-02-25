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

    /*
     * Configure HSI as PLL source; this avoids an error reconfiguring the
     * PLL when it is already set as the system clock source.
     */
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc_init.HSIState = RCC_HSI_ON;
    osc_init.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc_init.PLL.PLLState = RCC_PLL_NONE;

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }

    osc_init.OscillatorType = RCC_OSCILLATORTYPE_NONE;

    /*
     * LSI is used to clock the independent watchdog and optionally the RTC.
     * It can be disabled per user request, but is automatically enabled again
     * when the IWDG is started.
     *
     * XXX currently the watchdog is not optional, so there's no point in
     * disabling LSI through syscfg.
     */
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSI;
#if MYNEWT_VAL(STM32_CLOCK_LSI)
    osc_init.LSIState = RCC_LSI_ON;
#else
    osc_init.LSIState = RCC_LSI_OFF;
#endif

    /*
     * LSE is only used to clock the RTC.
     */
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSE;
#if (MYNEWT_VAL(STM32_CLOCK_LSE) == 0)
    osc_init.LSEState = RCC_LSE_OFF;
#elif MYNEWT_VAL(STM32_CLOCK_LSE_BYPASS)
    osc_init.LSEState = RCC_LSE_BYPASS;
#else
    osc_init.LSEState = RCC_LSE_ON;
#endif

    /*
     * HSE Oscillator (can be used as PLL, SYSCLK and RTC clock source)
     */
#if MYNEWT_VAL(STM32_CLOCK_HSE)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSE;
#if MYNEWT_VAL(STM32_CLOCK_HSE_BYPASS)
    osc_init.HSEState = RCC_HSE_BYPASS;
#else
    osc_init.HSEState = RCC_HSE_ON;
#endif
#endif

    /*
     * HSI Oscillator (can be used as PLL and SYSCLK clock source). It is
     * already turned on by default but a new calibration setting might be
     * used. If the user chooses to turn it off, it must be turned off after
     * SYSCLK was updated to use HSE/PLL.
     */
#if MYNEWT_VAL(STM32_CLOCK_HSI)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI;
    osc_init.HSIState = RCC_HSI_ON;
    /* HSI calibration is not optional when HSI is enabled */
    osc_init.HSICalibrationValue = MYNEWT_VAL(STM32_CLOCK_HSI_CALIBRATION);

#if MYNEWT_VAL(STM32_CLOCK_HSI) && \
        !IS_RCC_CALIBRATION_VALUE(MYNEWT_VAL(STM32_CLOCK_HSI_CALIBRATION))
#error "Invalid HSI calibration value"
#endif
#endif

    /*
     * Default to HSE as source, when both HSE and HSI are enabled.
     *
     * TODO: option to let the PLL turned off could be added, because
     * both HSI and HSE can be used as SYSCLK source.
     */
    osc_init.PLL.PLLState = RCC_PLL_ON;
#if MYNEWT_VAL(STM32_CLOCK_HSE)
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
#else
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSI;
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

#if MYNEWT_VAL(STM32_CLOCK_PLL_PLLR)

#if !IS_RCC_PLLR_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLR))
#error "PLLR value is invalid"
#endif

    osc_init.PLL.PLLR = MYNEWT_VAL(STM32_CLOCK_PLL_PLLR);

#endif /* MYNEWT_VAL(STM32_CLOCK_PLL_PLLR) */

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }

#if MYNEWT_VAL(STM32_CLOCK_ENABLE_OVERDRIVE)
    /*
     * Activate the Over-Drive mode
     */
    status = HAL_PWREx_EnableOverDrive();
    if (status != HAL_OK) {
        assert(0);
    }
#endif

    /*
     * Select PLL as system clock source and configure the HCLK, PCLK1 and
     * PCLK2 clocks dividers. HSI and HSE are also valid system clock sources,
     * although there is no much point in supporting them now.
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
    }

#if ((MYNEWT_VAL(STM32_CLOCK_HSI) == 0) || (MYNEWT_VAL(STM32_CLOCK_HSE) == 0))
    /*
     * Turn off HSE/HSI oscillator; this must be done at the end because
     * SYSCLK source has to be updated first.
     */
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_NONE;
#if (MYNEWT_VAL(STM32_CLOCK_HSE) == 0)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSE;
    osc_init.HSEState = RCC_HSE_OFF;
#endif
#if (MYNEWT_VAL(STM32_CLOCK_HSI) == 0)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI;
    osc_init.HSIState = RCC_HSI_OFF;
#endif
    osc_init.PLL.PLLState = RCC_PLL_NONE;

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }
#endif

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
