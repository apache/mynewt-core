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

#include <assert.h>
#include <stm32g4xx_hal_pwr_ex.h>
#include <stm32g4xx_hal_rcc.h>
#include <stm32g4xx_hal.h>

#define IS_RCC_HCLK(__HCLK__) (((__HCLK__) == RCC_SYSCLK_DIV1) || ((__HCLK__) == RCC_SYSCLK_DIV2) || \
                               ((__HCLK__) == RCC_SYSCLK_DIV4) || ((__HCLK__) == RCC_SYSCLK_DIV8) || \
                               ((__HCLK__) == RCC_SYSCLK_DIV16) || ((__HCLK__) == RCC_SYSCLK_DIV64) || \
                               ((__HCLK__) == RCC_SYSCLK_DIV128) || ((__HCLK__) == RCC_SYSCLK_DIV256) || \
                               ((__HCLK__) == RCC_SYSCLK_DIV512))

#define IS_RCC_PCLK(__PCLK__) (((__PCLK__) == RCC_HCLK_DIV1) || ((__PCLK__) == RCC_HCLK_DIV2) || \
                               ((__PCLK__) == RCC_HCLK_DIV4) || ((__PCLK__) == RCC_HCLK_DIV8) || \
                               ((__PCLK__) == RCC_HCLK_DIV16))

/*
 * This allows an user to have a custom clock configuration by zeroing
 * every possible clock source in the syscfg.
 */
#if MYNEWT_VAL(STM32_CLOCK_HSE) || \
    MYNEWT_VAL(STM32_CLOCK_LSE) || MYNEWT_VAL(STM32_CLOCK_HSI) || \
    MYNEWT_VAL(STM32_CLOCK_HSI48) || MYNEWT_VAL(STM32_CLOCK_LSI)

#define TRNG_ENABLED (MYNEWT_VAL(TRNG) != 0)

_Static_assert(MYNEWT_VAL(STM32_CLOCK_HSE) || MYNEWT_VAL(STM32_CLOCK_HSI),
               "HSI and/or HSE must be enabled");

void
SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc_init = {};
    RCC_ClkInitTypeDef clk_init = {};
    HAL_StatusTypeDef status;
#if TRNG_ENABLED
    RCC_PeriphCLKInitTypeDef pclk_init;
#endif

    /*
     * The voltage scaling allows optimizing the power consumption when the
     * device is clocked below the maximum system frequency, to update the
     * voltage scaling value regarding system frequency refer to product
     * datasheet.
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(MYNEWT_VAL(STM32_CLOCK_VOLTAGESCALING_CONFIG));

    osc_init.OscillatorType = RCC_OSCILLATORTYPE_NONE;

    /*
     * LSI is used to clock the independent watchdog and optionally the RTC.
     * It can be disabled per user request, but will be automatically enabled
     * again when the IWDG is started.
     */
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSI;
    if (MYNEWT_VAL(STM32_CLOCK_LSI)) {
        osc_init.LSIState = RCC_LSI_ON;
    } else {
        osc_init.LSIState = RCC_LSI_OFF;
    }

    /*
     * LSE is only used to clock the RTC.
     */
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSE;
    if (MYNEWT_VAL(STM32_CLOCK_LSE)) {
        if (MYNEWT_VAL(STM32_CLOCK_LSE_BYPASS)) {
            osc_init.LSEState = RCC_LSE_BYPASS;
        } else {
            osc_init.LSEState = RCC_LSE_ON;
        }
    } else {
        osc_init.LSEState = RCC_LSE_OFF;
    }

    /*
     * HSE Oscillator (can be used as PLL, SYSCLK and RTC clock source)
     */
    if (MYNEWT_VAL(STM32_CLOCK_HSE)) {
        osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSE;

        if (MYNEWT_VAL(STM32_CLOCK_HSE_BYPASS)) {
            osc_init.HSEState = RCC_HSE_BYPASS;
        } else {
            osc_init.HSEState = RCC_HSE_ON;
        }
    }

    _Static_assert(MYNEWT_VAL(STM32_CLOCK_HSI) || MYNEWT_VAL(STM32_CLOCK_HSI_CALIBRATION) <= 127,
                   "Invalid HSI calibration value");
    /*
     * HSI Oscillator (can be used as PLL and SYSCLK clock source). It is
     * already turned on by default but a new calibration setting might be
     * used. If the user chooses to turn it off, it must be turned off after
     * SYSCLK was updated to use HSE/PLL.
     */
    if (MYNEWT_VAL(STM32_CLOCK_HSI)) {
        osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI;
        osc_init.HSIState = RCC_HSI_ON;
        /* HSI calibration is not optional when HSI is enabled */
        osc_init.HSICalibrationValue = MYNEWT_VAL(STM32_CLOCK_HSI_CALIBRATION);
    }

    /*
     * HSI48 can be used to drive USB/RNG
     */
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI48;
    if (MYNEWT_VAL(STM32_CLOCK_HSI48)) {
        osc_init.HSI48State = RCC_HSI48_ON;
    } else {
        osc_init.HSI48State = RCC_HSI48_OFF;
    }

    _Static_assert(!MYNEWT_VAL(STM32_CLOCK_PLL) || IS_RCC_PLLM_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLM)),
                   "PLLM value is invalid");
    _Static_assert(!MYNEWT_VAL(STM32_CLOCK_PLL) || IS_RCC_PLLN_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLN)),
                   "PLLN value is invalid");
    _Static_assert(!MYNEWT_VAL(STM32_CLOCK_PLL) || IS_RCC_PLLP_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLP)),
                   "PLLP value is invalid");
    _Static_assert(!MYNEWT_VAL(STM32_CLOCK_PLL) || IS_RCC_PLLQ_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLQ)),
                   "PLLQ value is invalid");
    _Static_assert(!MYNEWT_VAL(STM32_CLOCK_PLL) || IS_RCC_PLLR_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLR)),
                   "PLLR value is invalid");
    _Static_assert(MYNEWT_VAL(STM32_CLOCK_PLL) || !MYNEWT_VAL_CHOICE(STM32_CLOCK_SYSCLK, PLL_R),
                   "PLL selected as system clock but not enabled");
    _Static_assert(MYNEWT_VAL(STM32_CLOCK_HSE) || !MYNEWT_VAL_CHOICE(STM32_CLOCK_SYSCLK, HSE),
                   "HSE selected as system clock but not enabled");
    _Static_assert(MYNEWT_VAL(STM32_CLOCK_HSI) || !MYNEWT_VAL_CHOICE(STM32_CLOCK_SYSCLK, HSI),
                   "HSI selected as system clock but not enabled");

    if (MYNEWT_VAL(STM32_CLOCK_PLL)) {
        /*
         * Default to MSI, HSE or HSI48 as PLL source when multiple high-speed
         * sources are enabled.
         */
        osc_init.PLL.PLLState = RCC_PLL_ON;
        if (MYNEWT_VAL(STM32_CLOCK_HSE)) {
            osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
        } else {
            osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSI;
        }

        osc_init.PLL.PLLM = MYNEWT_VAL(STM32_CLOCK_PLL_PLLM);
        osc_init.PLL.PLLN = MYNEWT_VAL(STM32_CLOCK_PLL_PLLN);
        osc_init.PLL.PLLP = MYNEWT_VAL(STM32_CLOCK_PLL_PLLP);
        osc_init.PLL.PLLQ = MYNEWT_VAL(STM32_CLOCK_PLL_PLLQ);
        osc_init.PLL.PLLR = MYNEWT_VAL(STM32_CLOCK_PLL_PLLR);
    }

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }

    clk_init.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    if (MYNEWT_VAL(STM32_CLOCK_PLL) &&
        (MYNEWT_VAL_CHOICE(STM32_CLOCK_SYSCLK, PLL_R) || MYNEWT_VAL_CHOICE(STM32_CLOCK_SYSCLK, auto))) {
        clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    } else if (MYNEWT_VAL(STM32_CLOCK_HSE) &&
               (MYNEWT_VAL_CHOICE(STM32_CLOCK_SYSCLK, HSE) ||
                MYNEWT_VAL_CHOICE(STM32_CLOCK_SYSCLK, auto))) {
        clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    } else {
        clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    }

    _Static_assert(IS_RCC_HCLK(MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER)), "AHB clock divider is invalid");
    _Static_assert(IS_RCC_PCLK(MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER)), "APB1 clock divider is invalid");
    _Static_assert(IS_RCC_PCLK(MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER)), "APB2 clock divider is invalid");

    clk_init.AHBCLKDivider = MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER);
    clk_init.APB1CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER);
    clk_init.APB2CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER);

    _Static_assert(IS_FLASH_LATENCY(MYNEWT_VAL(STM32_FLASH_LATENCY)), "Flash latency value is invalid");

    status = HAL_RCC_ClockConfig(&clk_init, MYNEWT_VAL(STM32_FLASH_LATENCY));
    if (status != HAL_OK) {
        assert(0);
    }

    if ((MYNEWT_VAL(STM32_CLOCK_HSI) == 0) || (MYNEWT_VAL(STM32_CLOCK_HSE) == 0)) {
        /*
         * Turn off HSE/HSI oscillator; this must be done at the end because
         * SYSCLK source has to be updated first.
         */
        osc_init.OscillatorType = RCC_OSCILLATORTYPE_NONE;
        if (MYNEWT_VAL(STM32_CLOCK_HSE) == 0) {
            osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSE;
            osc_init.HSEState = RCC_HSE_OFF;
        }
        if (MYNEWT_VAL(STM32_CLOCK_HSI) == 0) {
            osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI;
            osc_init.HSIState = RCC_HSI_OFF;
        }
    }

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
}
#endif
