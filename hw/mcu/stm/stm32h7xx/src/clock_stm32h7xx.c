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

#include "stm32h7xx_hal_pwr_ex.h"
#include "stm32h7xx_hal.h"
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

#define PLL_ANY_ON  (MYNEWT_VAL(STM32_CLOCK_PLL1) || MYNEWT_VAL(STM32_CLOCK_PLL2) || \
                     MYNEWT_VAL(STM32_CLOCK_PLL3))

/*
 * PLL1, PLL2 and PLL3 share one source (STM32_CLOCK_PLL_SOURCE),
 * each PLL has its own M divider.
 */
#if PLL_ANY_ON
#if MYNEWT_VAL(STM32_CLOCK_PLL_SOURCE) == RCC_PLLSOURCE_NONE
#error "PLL enabled but STM32_CLOCK_PLL_SOURCE is RCC_PLLSOURCE_NONE"
#endif
#if MYNEWT_VAL(STM32_CLOCK_PLL_SOURCE) == RCC_PLLSOURCE_HSE && !MYNEWT_VAL(STM32_CLOCK_HSE)
#error "PLL source is HSE but STM32_CLOCK_HSE is not enabled"
#endif
#if MYNEWT_VAL(STM32_CLOCK_PLL_SOURCE) == RCC_PLLSOURCE_HSI && !MYNEWT_VAL(STM32_CLOCK_HSI)
#error "PLL source is HSI but STM32_CLOCK_HSI is not enabled"
#endif
#if MYNEWT_VAL(STM32_CLOCK_PLL_SOURCE) == RCC_PLLSOURCE_CSI && !MYNEWT_VAL(STM32_CLOCK_CSI)
#error "PLL source is CSI but STM32_CLOCK_CSI is not enabled"
#endif
#endif

#if MYNEWT_VAL(STM32_CLOCK_SYSCLK_SOURCE) == RCC_SYSCLKSOURCE_PLLCLK && \
    !MYNEWT_VAL(STM32_CLOCK_PLL1)
#error "SYSCLK source is PLL1 but STM32_CLOCK_PLL1 is not enabled"
#endif
#if MYNEWT_VAL(STM32_CLOCK_SYSCLK_SOURCE) == RCC_SYSCLKSOURCE_HSE && \
    !MYNEWT_VAL(STM32_CLOCK_HSE)
#error "SYSCLK source is HSE but STM32_CLOCK_HSE is not enabled"
#endif
#if MYNEWT_VAL(STM32_CLOCK_SYSCLK_SOURCE) == RCC_SYSCLKSOURCE_HSI && \
    !MYNEWT_VAL(STM32_CLOCK_HSI)
#error "SYSCLK source is HSI but STM32_CLOCK_HSI is not enabled"
#endif
#if MYNEWT_VAL(STM32_CLOCK_SYSCLK_SOURCE) == RCC_SYSCLKSOURCE_CSI && \
    !MYNEWT_VAL(STM32_CLOCK_CSI)
#error "SYSCLK source is CSI but STM32_CLOCK_CSI is not enabled"
#endif

#if MYNEWT_VAL(STM32_CLOCK_PLL1)
#if !IS_RCC_PLLM_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL1_PLLM))
#error "PLL1M value is invalid"
#endif
#if !IS_RCC_PLLN_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL1_PLLN))
#error "PLL1N value is invalid"
#endif
#if !IS_RCC_PLLP_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL1_PLLP))
#error "PLL1P value is invalid"
#endif
#if !IS_RCC_PLLQ_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL1_PLLQ))
#error "PLL1Q value is invalid"
#endif
#if !IS_RCC_PLLR_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL1_PLLR))
#error "PLL1R value is invalid"
#endif
#endif

/* PLL2/PLL3 P, Q, R divider 0 means that output is disabled */
#define PLLX_DIV_OK(n, d) (MYNEWT_VAL(STM32_CLOCK_PLL##n##_PLL##d) <= 128)
/* Divider value for disabled output does not matter but must be valid */
#define PLLX_DIV(n, d)    (MYNEWT_VAL(STM32_CLOCK_PLL##n##_PLL##d) ? \
                           MYNEWT_VAL(STM32_CLOCK_PLL##n##_PLL##d) : 1)
/* Mask of outputs to enable (non-zero dividers) */
#define PLLX_OUTPUTS(n)                                                       \
    ((MYNEWT_VAL(STM32_CLOCK_PLL##n##_PLLP) ? RCC_PLL##n##_DIVP : 0) |        \
     (MYNEWT_VAL(STM32_CLOCK_PLL##n##_PLLQ) ? RCC_PLL##n##_DIVQ : 0) |        \
     (MYNEWT_VAL(STM32_CLOCK_PLL##n##_PLLR) ? RCC_PLL##n##_DIVR : 0))

#if MYNEWT_VAL(STM32_CLOCK_PLL2)
#if !IS_RCC_PLL2M_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL2_PLLM))
#error "PLL2M value is invalid"
#endif
#if !IS_RCC_PLL2N_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL2_PLLN))
#error "PLL2N value is invalid"
#endif
#if !PLLX_DIV_OK(2, P) || !PLLX_DIV_OK(2, Q) || !PLLX_DIV_OK(2, R)
#error "PLL2 P, Q or R value is invalid"
#endif
#endif

#if MYNEWT_VAL(STM32_CLOCK_PLL3)
#if !IS_RCC_PLL3M_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL3_PLLM))
#error "PLL3M value is invalid"
#endif
#if !IS_RCC_PLL3N_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL3_PLLN))
#error "PLL3N value is invalid"
#endif
#if !PLLX_DIV_OK(3, P) || !PLLX_DIV_OK(3, Q) || !PLLX_DIV_OK(3, R)
#error "PLL3 P, Q or R value is invalid"
#endif
#endif

/* Kernel clock selections must use enabled PLL outputs */
#if MYNEWT_VAL(STM32_CLOCK_USBSEL) == RCC_USBCLKSOURCE_PLL && !MYNEWT_VAL(STM32_CLOCK_PLL1)
#error "USB clock is PLL1 Q but STM32_CLOCK_PLL1 is not enabled"
#endif
#if MYNEWT_VAL(STM32_CLOCK_USBSEL) == RCC_USBCLKSOURCE_PLL3 && \
    !(MYNEWT_VAL(STM32_CLOCK_PLL3) && MYNEWT_VAL(STM32_CLOCK_PLL3_PLLQ))
#error "USB clock is PLL3 Q but PLL3 or its Q output is not enabled"
#endif
#if MYNEWT_VAL(STM32_CLOCK_FMCSEL) == RCC_FMCCLKSOURCE_PLL && !MYNEWT_VAL(STM32_CLOCK_PLL1)
#error "FMC clock is PLL1 Q but STM32_CLOCK_PLL1 is not enabled"
#endif
#if MYNEWT_VAL(STM32_CLOCK_FMCSEL) == RCC_FMCCLKSOURCE_PLL2 && \
    !(MYNEWT_VAL(STM32_CLOCK_PLL2) && MYNEWT_VAL(STM32_CLOCK_PLL2_PLLR))
#error "FMC clock is PLL2 R but PLL2 or its R output is not enabled"
#endif

/*
 * PLL2 and PLL3 configuration, same steps as HAL RCCEx_PLL2_Config() and
 * RCCEx_PLL3_Config() (static in HAL, used only for peripheral clock
 * selection) but all requested outputs are enabled at once and no HAL tick
 * timeout is used (tick is not running yet).
 * PLL configuration can only be changed when PLL is disabled.
 */
static void
pll2_config(const RCC_PLL2InitTypeDef *pll2, uint32_t outputs)
{
    __HAL_RCC_PLL2_DISABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) != 0U) {
    }

    __HAL_RCC_PLL2_CONFIG(pll2->PLL2M, pll2->PLL2N, pll2->PLL2P, pll2->PLL2Q,
                          pll2->PLL2R);
    __HAL_RCC_PLL2_VCIRANGE(pll2->PLL2RGE);
    __HAL_RCC_PLL2_VCORANGE(pll2->PLL2VCOSEL);
    __HAL_RCC_PLL2FRACN_DISABLE();
    __HAL_RCC_PLL2FRACN_CONFIG(pll2->PLL2FRACN);
    __HAL_RCC_PLL2FRACN_ENABLE();
    __HAL_RCC_PLL2CLKOUT_DISABLE(RCC_PLL2_DIVP | RCC_PLL2_DIVQ | RCC_PLL2_DIVR);
    __HAL_RCC_PLL2CLKOUT_ENABLE(outputs);

    __HAL_RCC_PLL2_ENABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) == 0U) {
    }
}

static void
pll3_config(const RCC_PLL3InitTypeDef *pll3, uint32_t outputs)
{
    __HAL_RCC_PLL3_DISABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY) != 0U) {
    }

    __HAL_RCC_PLL3_CONFIG(pll3->PLL3M, pll3->PLL3N, pll3->PLL3P, pll3->PLL3Q,
                          pll3->PLL3R);
    __HAL_RCC_PLL3_VCIRANGE(pll3->PLL3RGE);
    __HAL_RCC_PLL3_VCORANGE(pll3->PLL3VCOSEL);
    __HAL_RCC_PLL3FRACN_DISABLE();
    __HAL_RCC_PLL3FRACN_CONFIG(pll3->PLL3FRACN);
    __HAL_RCC_PLL3FRACN_ENABLE();
    __HAL_RCC_PLL3CLKOUT_DISABLE(RCC_PLL3_DIVP | RCC_PLL3_DIVQ | RCC_PLL3_DIVR);
    __HAL_RCC_PLL3CLKOUT_ENABLE(outputs);

    __HAL_RCC_PLL3_ENABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY) == 0U) {
    }
}

void
SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc_init = {0};
    RCC_ClkInitTypeDef clk_init = {0};
    HAL_StatusTypeDef status;
    const RCC_PLL2InitTypeDef pll2 = {
        .PLL2M = MYNEWT_VAL(STM32_CLOCK_PLL2_PLLM),
        .PLL2N = MYNEWT_VAL(STM32_CLOCK_PLL2_PLLN),
        .PLL2P = PLLX_DIV(2, P),
        .PLL2Q = PLLX_DIV(2, Q),
        .PLL2R = PLLX_DIV(2, R),
        .PLL2RGE = MYNEWT_VAL(STM32_CLOCK_PLL2_RGE),
        .PLL2VCOSEL = MYNEWT_VAL(STM32_CLOCK_PLL2_VCOSEL),
        .PLL2FRACN = MYNEWT_VAL(STM32_CLOCK_PLL2_PLLFRACN),
    };
    const RCC_PLL3InitTypeDef pll3 = {
        .PLL3M = MYNEWT_VAL(STM32_CLOCK_PLL3_PLLM),
        .PLL3N = MYNEWT_VAL(STM32_CLOCK_PLL3_PLLN),
        .PLL3P = PLLX_DIV(3, P),
        .PLL3Q = PLLX_DIV(3, Q),
        .PLL3R = PLLX_DIV(3, R),
        .PLL3RGE = MYNEWT_VAL(STM32_CLOCK_PLL3_RGE),
        .PLL3VCOSEL = MYNEWT_VAL(STM32_CLOCK_PLL3_VCOSEL),
        .PLL3FRACN = MYNEWT_VAL(STM32_CLOCK_PLL3_PLLFRACN),
    };

    /*
     *  Supply configuration update enable
     */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /*
     * The voltage scaling allows optimizing the power consumption when the
     * device is clocked below the maximum system frequency, to update the
     * voltage scaling value regarding system frequency refer to product
     * datasheet.
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(MYNEWT_VAL(STM32_CLOCK_VOLTAGESCALING_CONFIG));
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
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
    !IS_RCC_HSICALIBRATION_VALUE(MYNEWT_VAL(STM32_CLOCK_HSI_CALIBRATION))
#error "Invalid HSI calibration value"
#endif
#endif

    /*
     * PLL1. When disabled it is left untouched here and turned off at the
     * end, after SYSCLK was switched to its source (it may drive SYSCLK
     * now, i.e. when started by bootloader).
     */
#if MYNEWT_VAL(STM32_CLOCK_PLL1)
    osc_init.PLL.PLLState = RCC_PLL_ON;
    osc_init.PLL.PLLSource = MYNEWT_VAL(STM32_CLOCK_PLL_SOURCE);
    osc_init.PLL.PLLM = MYNEWT_VAL(STM32_CLOCK_PLL1_PLLM);
    osc_init.PLL.PLLN = MYNEWT_VAL(STM32_CLOCK_PLL1_PLLN);
    osc_init.PLL.PLLP = MYNEWT_VAL(STM32_CLOCK_PLL1_PLLP);
    osc_init.PLL.PLLQ = MYNEWT_VAL(STM32_CLOCK_PLL1_PLLQ);
    osc_init.PLL.PLLR = MYNEWT_VAL(STM32_CLOCK_PLL1_PLLR);
    osc_init.PLL.PLLRGE = MYNEWT_VAL(STM32_CLOCK_PLL1_RGE);
    osc_init.PLL.PLLVCOSEL = MYNEWT_VAL(STM32_CLOCK_PLL1_VCOSEL);
    osc_init.PLL.PLLFRACN = MYNEWT_VAL(STM32_CLOCK_PLL1_PLLFRACN);
#else
    osc_init.PLL.PLLState = RCC_PLL_NONE;
#endif

    /*
     * CSI Oscillator (can be used as PLL and SYSCLK clock source).
     */
    if (MYNEWT_VAL(STM32_CLOCK_CSI)) {
        osc_init.OscillatorType |= RCC_OSCILLATORTYPE_CSI;
        osc_init.CSIState = RCC_CSI_ON;
        osc_init.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
    }

    if (MYNEWT_VAL(STM32_CLOCK_HSI48)) {
        osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI48;
        osc_init.HSI48State = RCC_HSI48_ON;
    }

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }

    if (!MYNEWT_VAL(STM32_CLOCK_PLL1) &&
        (MYNEWT_VAL(STM32_CLOCK_PLL2) || MYNEWT_VAL(STM32_CLOCK_PLL3))) {
        /*
         * Without PLL1 source was not set by HAL_RCC_OscConfig(). It can only
         * be changed when all PLLs are disabled.
         */
        __HAL_RCC_PLL_PLLSOURCE_CONFIG(MYNEWT_VAL(STM32_CLOCK_PLL_SOURCE));
    }
    if (MYNEWT_VAL(STM32_CLOCK_PLL2)) {
        pll2_config(&pll2, PLLX_OUTPUTS(2));
    }
    if (MYNEWT_VAL(STM32_CLOCK_PLL3)) {
        pll3_config(&pll3, PLLX_OUTPUTS(3));
    }

    /*
     * Select system clock source and configure the HCLK, PCLK1 and
     * PCLK2 clocks dividers.
     */
    clk_init.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                         RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk_init.SYSCLKSource = MYNEWT_VAL(STM32_CLOCK_SYSCLK_SOURCE);

#if !IS_RCC_HCLK(MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER))
#error "AHB clock divider is invalid"
#endif

#if !IS_RCC_PCLK1(MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER))
#error "APB1 clock divider is invalid"
#endif

#if !IS_RCC_PCLK2(MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER))
#error "APB2 clock divider is invalid"
#endif

    clk_init.AHBCLKDivider = MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER);
    clk_init.APB1CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER);
    clk_init.APB2CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER);
    clk_init.APB3CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB3_DIVIDER);
    clk_init.APB4CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB4_DIVIDER);

#if !IS_FLASH_LATENCY(MYNEWT_VAL(STM32_FLASH_LATENCY))
#error "Flash latency value is invalid"
#endif

    status = HAL_RCC_ClockConfig(&clk_init, MYNEWT_VAL(STM32_FLASH_LATENCY));
    if (status != HAL_OK) {
        assert(0);
    }

#if ((MYNEWT_VAL(STM32_CLOCK_HSI) == 0) || (MYNEWT_VAL(STM32_CLOCK_HSE) == 0) || \
     (MYNEWT_VAL(STM32_CLOCK_PLL1) == 0))
    /*
     * Turn off HSE/HSI oscillator and PLL1; this must be done at the end
     * because SYSCLK source has to be updated first.
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
#if (MYNEWT_VAL(STM32_CLOCK_PLL1) == 0)
    osc_init.PLL.PLLState = RCC_PLL_OFF;
#else
    osc_init.PLL.PLLState = RCC_PLL_NONE;
#endif

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }
#endif

    /*
     * Kernel clock selections. Only the mux is set, selected PLL is
     * configured by its own settings.
     */
    if (MYNEWT_VAL(STM32_CLOCK_USBSEL)) {
        __HAL_RCC_USB_CONFIG(MYNEWT_VAL(STM32_CLOCK_USBSEL));
    }
    if (MYNEWT_VAL(STM32_CLOCK_FMCSEL)) {
        __HAL_RCC_FMC_CONFIG(MYNEWT_VAL(STM32_CLOCK_FMCSEL));
    }
}
#endif
