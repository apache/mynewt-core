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

#include "stm32wbxx_hal_pwr_ex.h"
#include "stm32wbxx_hal_rcc.h"
#include "stm32wbxx_hal.h"
#include <assert.h>

/*
 * This allows an user to have a custom clock configuration by zeroing
 * every possible clock source in the syscfg.
 */
#if MYNEWT_VAL(STM32_CLOCK_MSI) || MYNEWT_VAL(STM32_CLOCK_HSE) || \
    MYNEWT_VAL(STM32_CLOCK_LSE) || MYNEWT_VAL(STM32_CLOCK_HSI) || \
    MYNEWT_VAL(STM32_CLOCK_HSI48) || MYNEWT_VAL(STM32_CLOCK_LSI1) || \
    MYNEWT_VAL(STM32_CLOCK_LSI2)

#define TRNG_ENABLED (MYNEWT_VAL(TRNG) != 0)

/*
 * HSI is turned on by default, but can be turned off and use HSE/HSI48 instead.
 */
#if (((MYNEWT_VAL(STM32_CLOCK_MSI) != 0) + \
      (MYNEWT_VAL(STM32_CLOCK_HSE) != 0) + \
      (MYNEWT_VAL(STM32_CLOCK_HSI) != 0) + \
      (MYNEWT_VAL(STM32_CLOCK_HSI48) != 0)) < 1)
#error "At least one of MSI, HSE, HSI or HSI48 clock sources must be enabled"
#endif

void
SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc_init;
    RCC_ClkInitTypeDef clk_init;
    HAL_StatusTypeDef status;
#if TRNG_ENABLED
    RCC_PeriphCLKInitTypeDef  pclk_init;
#endif

    /*
     * The voltage scaling allows optimizing the power consumption when the
     * device is clocked below the maximum system frequency, to update the
     * voltage scaling value regarding system frequency refer to product
     * datasheet.
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(MYNEWT_VAL(STM32_CLOCK_VOLTAGESCALING_CONFIG));

    osc_init.OscillatorType = RCC_OSCILLATORTYPE_NONE;

    /*
     * LSI1/2 is used to clock the independent watchdog and optionally the RTC.
     * LSI2 can also be used for auto-wakeup from the RF system. When LSI2 is
     * enabled LSI1 is automatically enabled no matter which config was set.
     *
     * Both can be disabled per user request, but LSI1 is automatically enabled
     * again when the IWDG is started.
     *
     * XXX currently the watchdog is not optional, so there's no point in
     * disabling LSI1 through syscfg.
     */
#if (MYNEWT_VAL(STM32_CLOCK_LSI1) == 0) && (MYNEWT_VAL(STM32_CLOCK_LSI2) == 0)

    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSI1 | RCC_OSCILLATORTYPE_LSI2;
    osc_init.LSIState = RCC_LSI_OFF;

#else

    osc_init.LSIState = RCC_LSI_ON;

#if MYNEWT_VAL(STM32_CLOCK_LSI1)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSI1;
#endif

#if MYNEWT_VAL(STM32_CLOCK_LSI2)

#if !IS_RCC_LSI2_CALIBRATION_VALUE(MYNEWT_VAL(STM32_CLOCK_LSI2_CALIBRATION))
#error "Invalid LSI2 calibration value"
#endif

    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSI2;
    osc_init.LSI2CalibrationValue = MYNEWT_VAL(STM32_CLOCK_LSI2_CALIBRATION);
#endif /* MYNEWT_VAL(STM32_CLOCK_LSI2) */

#endif /* (MYNEWT_VAL(STM32_CLOCK_LSI1) == 0) && (MYNEWT_VAL(STM32_CLOCK_LSI2) == 0) */

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
     * MSI Oscillator
     */
#if MYNEWT_VAL(STM32_CLOCK_MSI)

#if (MYNEWT_VAL(STM32_CLOCK_MSI_CALIBRATION) > 255)
#error "Invalid MSI calibration value"
#endif
#if !IS_RCC_MSI_CLOCK_RANGE(MYNEWT_VAL(STM32_CLOCK_MSI_CLOCK_RANGE))
#error "Invalid MSI clock range"
#endif

    /* NOTE: MSI can't be disabled if it's the current PLL or SYSCLK source;
     * leave it untouched in those cases, and disable later after a new source
     * has been configured.
     */
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_MSI;
    osc_init.MSIState = RCC_MSI_ON;
    osc_init.MSICalibrationValue = MYNEWT_VAL(STM32_CLOCK_MSI_CALIBRATION);
    osc_init.MSIClockRange = MYNEWT_VAL(STM32_CLOCK_MSI_CLOCK_RANGE);
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
#if MYNEWT_VAL(STM32_CLOCK_HSEPRE)
    __HAL_RCC_HSE_DIV2_ENABLE();
#else
    __HAL_RCC_HSE_DIV2_DISABLE();
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

#if (MYNEWT_VAL(STM32_CLOCK_HSI_CALIBRATION) > 127)
#error "Invalid HSI calibration value"
#endif
#endif

    /*
     * Can be used to drive USB and the TRNG
     */
#if MYNEWT_VAL(STM32_CLOCK_HSI48)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI48;
    osc_init.HSI48State = RCC_HSI48_ON;
#endif

    /*
     * Default to MSI, HSE or HSI48 as PLL source when multiple high-speed
     * sources are enabled.
     */
    osc_init.PLL.PLLState = RCC_PLL_ON;
#if MYNEWT_VAL(STM32_CLOCK_MSI)
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_MSI;
#elif MYNEWT_VAL(STM32_CLOCK_HSE)
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

#if !IS_RCC_PLLR_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLR))
#error "PLLR value is invalid"
#endif

    osc_init.PLL.PLLM = MYNEWT_VAL(STM32_CLOCK_PLL_PLLM);
    osc_init.PLL.PLLN = MYNEWT_VAL(STM32_CLOCK_PLL_PLLN);
    osc_init.PLL.PLLP = MYNEWT_VAL(STM32_CLOCK_PLL_PLLP);
    osc_init.PLL.PLLQ = MYNEWT_VAL(STM32_CLOCK_PLL_PLLQ);
    osc_init.PLL.PLLR = MYNEWT_VAL(STM32_CLOCK_PLL_PLLR);

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }

    /*
     * Select PLL as system clock source and configure the HCLK*, PCLK* and
     * SYSCLK clocks dividers. HSI, HSE and MSI are also valid system clock
     * sources, although there is no much point in supporting them now.
     */
    clk_init.ClockType = RCC_CLOCKTYPE_HCLK4 | RCC_CLOCKTYPE_HCLK2 |
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;
    clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;

#if !IS_RCC_HCLKx(MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER))
#error "AHB clock divider is invalid"
#endif

#if !IS_RCC_PCLKx(MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER))
#error "APB1 clock divider is invalid"
#endif

#if !IS_RCC_PCLKx(MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER))
#error "APB2 clock divider is invalid"
#endif

#if !IS_RCC_HCLKx(MYNEWT_VAL(STM32_CLOCK_AHBCLK2_DIVIDER))
#error "HCLK2 divider is invalid"
#endif

#if !IS_RCC_HCLKx(MYNEWT_VAL(STM32_CLOCK_AHBCLK4_DIVIDER))
#error "HCLK4 divider is invalid"
#endif

    clk_init.AHBCLKDivider = MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER);
    clk_init.APB1CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER);
    clk_init.APB2CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER);
    clk_init.AHBCLK2Divider = MYNEWT_VAL(STM32_CLOCK_AHBCLK2_DIVIDER);
    clk_init.AHBCLK4Divider = MYNEWT_VAL(STM32_CLOCK_AHBCLK4_DIVIDER);

#if !IS_FLASH_LATENCY(MYNEWT_VAL(STM32_FLASH_LATENCY))
#error "Flash latency value is invalid"
#endif

    status = HAL_RCC_ClockConfig(&clk_init, MYNEWT_VAL(STM32_FLASH_LATENCY));
    if (status != HAL_OK) {
        assert(0);
    }

#if ((MYNEWT_VAL(STM32_CLOCK_HSI) == 0) || (MYNEWT_VAL(STM32_CLOCK_HSE) == 0) || \
     (MYNEWT_VAL(STM32_CLOCK_HSI48) == 0) || (MYNEWT_VAL(STM32_CLOCK_MSI) == 0))
    /*
     * Turn off HSE/HSI/HSI48 oscillator; this must be done at the end because
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
#if (MYNEWT_VAL(STM32_CLOCK_HSI48) == 0)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI48;
    osc_init.HSI48State = RCC_HSI48_OFF;
#endif
#if (MYNEWT_VAL(STM32_CLOCK_MSI) == 0)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_MSI;
    osc_init.MSIState = RCC_MSI_OFF;
#endif

#endif

    osc_init.PLL.PLLState = RCC_PLL_NONE;

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }

#if TRNG_ENABLED
    pclk_init.PeriphClockSelection = RCC_PERIPHCLK_RNG;
    /* Other clock sources are possible, but since right now we always
     * configure the PLL, this should be ok
     */
    pclk_init.RngClockSelection = RCC_RNGCLKSOURCE_PLL;
    status = HAL_RCCEx_PeriphCLKConfig(&pclk_init);
    if (status != HAL_OK) {
        assert(0);
    }
#endif

#if PREFETCH_ENABLE
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
#endif

#if INSTRUCTION_CACHE_ENABLE
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
#endif

#if DATA_CACHE_ENABLE
    __HAL_FLASH_DATA_CACHE_ENABLE();
#endif
}
#endif
