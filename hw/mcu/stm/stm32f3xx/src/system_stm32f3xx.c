/**
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
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
  *
  ******************************************************************************
  */

#include "bsp/stm32f3xx_hal_conf.h"
#include "stm32f3xx.h"
#include "mcu/cmsis_nvic.h"

/*
 * This variable is updated in three ways:
 * 1) by calling CMSIS function SystemCoreClockUpdate()
 * 2) by calling HAL API function HAL_RCC_GetHCLKFreq()
 * 3) each time HAL_RCC_ClockConfig() is called to configure the system clock
 *    frequency
 * Note: If you use this function to configure the system clock; then there
 * is no need to call the 2 first functions listed above, since SystemCoreClock
 * variable is updated automatically.
 */
uint32_t SystemCoreClock;
const uint8_t AHBPrescTable[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9
};
const uint8_t APBPrescTable[8] = {
    0, 0, 0, 0, 1, 2, 3, 4
};

/*
 * XXX BSP specific
 */
void SystemClock_Config(void);

/**
  * @brief  Setup the microcontroller system
  *         Initialize the FPU setting, vector table location and the PLL configuration is reset.
  * @param  None
  * @retval None
  */
void SystemInit(void)
{
    /*
     * FPU settings
     */

#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    /* set CP10 and CP11 Full Access */
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
#endif

    /*
     * Reset the RCC clock configuration to the default reset state
     */

    /* Set HSION bit */
    RCC->CR |= (uint32_t)0x00000001;

    /* Reset CFGR register */
    RCC->CFGR &= 0xF87FC00C;

    /* Reset HSEON, CSSON and PLLON bits */
    RCC->CR &= (uint32_t)0xFEF6FFFF;

    /* Reset HSEBYP bit */
    RCC->CR &= (uint32_t)0xFFFBFFFF;

    /* Reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE bits */
    RCC->CFGR &= (uint32_t)0xFF80FFFF;

    /* Reset PREDIV1[3:0] bits */
    RCC->CFGR2 &= (uint32_t)0xFFFFFFF0;

    /* Reset USARTSW[1:0], I2CSW and TIMs bits */
    RCC->CFGR3 &= (uint32_t)0xFF00FCCC;

    /* Disable all interrupts */
    RCC->CIR = 0x00000000;

    /* Configure System Clock */
    SystemClock_Config();

    /* Update SystemCoreClock global variable */
    SystemCoreClockUpdate();

    /* Relocate the vector table */
    NVIC_Relocate();
}

/**
   * @brief  Update SystemCoreClock variable according to Clock Register Values.
  *         The SystemCoreClock variable contains the core clock (HCLK), it can
  *         be used by the user application to setup the SysTick timer or configure
  *         other parameters.
  *
  * @note   Each time the core clock (HCLK) changes, this function must be called
  *         to update SystemCoreClock variable value. Otherwise, any configuration
  *         based on this variable will be incorrect.
  *
  * @note   - The system frequency computed by this function is not the real
  *           frequency in the chip. It is calculated based on the predefined
  *           constant and the selected clock source:
  *
  *           - If SYSCLK source is HSI, SystemCoreClock will contain the HSI_VALUE(*)
  *
  *           - If SYSCLK source is HSE, SystemCoreClock will contain the HSE_VALUE(**)
  *
  *           - If SYSCLK source is PLL, SystemCoreClock will contain the HSE_VALUE(**)
  *             or HSI_VALUE(*) multiplied/divided by the PLL factors.
  *
  *         (*) HSI_VALUE is a constant defined in stm32f3xx_hal.h file (default value
  *             8 MHz) but the real value may vary depending on the variations
  *             in voltage and temperature.
  *
  *         (**) HSE_VALUE is a constant defined in stm32f3xx_hal.h file (default value
  *              8 MHz), user has to ensure that HSE_VALUE is same as the real
  *              frequency of the crystal used. Otherwise, this function may
  *              have wrong result.
  *
  *         - The result of this function could be not correct when using fractional
  *           value for HSE crystal.
  *
  * @param  None
  * @retval None
  */
void SystemCoreClockUpdate (void)
{
    uint32_t tmp;
    uint32_t pllmull;
    uint32_t pllsource;
    uint32_t predivfactor;

    /*
     * Get SYSCLK source
     */

    tmp = RCC->CFGR & RCC_CFGR_SWS;
    switch (tmp) {
    /* HSE used as system clock */
    case RCC_CFGR_SWS_HSE:
        SystemCoreClock = HSE_VALUE;
        break;

    /* PLL used as system clock */
    case RCC_CFGR_SWS_PLL:
        /*
         * Get PLL clock source and multiplication factor
         */
        pllmull = RCC->CFGR & RCC_CFGR_PLLMUL;
        pllsource = RCC->CFGR & RCC_CFGR_PLLSRC;
        pllmull = (pllmull >> 18) + 2;

#if defined(STM32F302xE) || defined(STM32F303xE) || defined(STM32F398xx)
        predivfactor = (RCC->CFGR2 & RCC_CFGR2_PREDIV) + 1;
        if (pllsource == RCC_CFGR_PLLSRC_HSE_PREDIV) {
            /* HSE oscillator clock selected as PREDIV1 clock entry */
            SystemCoreClock = (HSE_VALUE / predivfactor) * pllmull;
        } else {
            /* HSI oscillator clock selected as PREDIV1 clock entry */
            SystemCoreClock = (HSI_VALUE / predivfactor) * pllmull;
        }
#else
        if (pllsource == RCC_CFGR_PLLSRC_HSI_DIV2) {
            /* HSI oscillator clock divided by 2 selected as PLL clock entry */
            SystemCoreClock = (HSI_VALUE >> 1) * pllmull;
        } else {
            predivfactor = (RCC->CFGR2 & RCC_CFGR2_PREDIV) + 1;
            /* HSE oscillator clock selected as PREDIV1 clock entry */
            SystemCoreClock = (HSE_VALUE / predivfactor) * pllmull;
        }
#endif
        break;

    /* HSI used as system clock */
    default:
      SystemCoreClock = HSI_VALUE;
      break;
    }

    /*
     * Compute HCLK clock frequency
     */

    /* Get HCLK prescaler */
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];

    /* HCLK clock frequency */
    SystemCoreClock >>= tmp;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
