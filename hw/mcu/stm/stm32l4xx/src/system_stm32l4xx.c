/**
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  */

#include <stdint.h>
#include "bsp/stm32l4xx_hal_conf.h"
#include "stm32l4xx.h"
#include "mcu/cmsis_nvic.h"
#include "mcu/stm32_hal.h"

/* The SystemCoreClock variable is updated in three ways:
  1) by calling CMSIS function SystemCoreClockUpdate()
  2) by calling HAL API function HAL_RCC_GetHCLKFreq()
  3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency
     Note: If you use this function to configure the system clock; then there
           is no need to call the 2 first functions listed above, since SystemCoreClock
           variable is updated automatically.
*/

uint32_t SystemCoreClock;
const uint8_t AHBPrescTable[16] = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U,
};
const uint8_t APBPrescTable[8] = {
    0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U,
};
const uint32_t MSIRangeTable[12] = {
    100000U,   200000U,   400000U,   800000U,  1000000U,  2000000U,
    4000000U, 8000000U, 16000000U, 24000000U, 32000000U, 48000000U,
};

/*
 * XXX BSP specific
 */
void SystemClock_Config(void);

/**
  * @brief  Setup the microcontroller system.
  * @param  None
  * @retval None
  */
void SystemInit(void)
{
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
        /* set CP10 and CP11 Full Access */
        SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    #endif

    /*
     * Reset the RCC clock configuration to the default reset state
     */

    /* Set MSION bit */
    RCC->CR |= RCC_CR_MSION;

    /* Reset CFGR register */
    RCC->CFGR = 0x00000000U;

    /* Reset HSEON, CSSON , HSION, and PLLON bits */
    RCC->CR &= 0xEAF6FFFFU;

    /* Reset PLLCFGR register */
    RCC->PLLCFGR = 0x00001000U;

    /* Reset HSEBYP bit */
    RCC->CR &= 0xFFFBFFFFU;

    /* Disable all interrupts */
    RCC->CIER = 0x00000000U;

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
  *           - If SYSCLK source is MSI, SystemCoreClock will contain the MSI_VALUE(*)
  *
  *           - If SYSCLK source is HSI, SystemCoreClock will contain the HSI_VALUE(**)
  *
  *           - If SYSCLK source is HSE, SystemCoreClock will contain the HSE_VALUE(***)
  *
  *           - If SYSCLK source is PLL, SystemCoreClock will contain the HSE_VALUE(***)
  *             or HSI_VALUE(*) or MSI_VALUE(*) multiplied/divided by the PLL factors.
  *
  *         (*) MSI_VALUE is a constant defined in stm32l4xx_hal.h file (default value
  *             4 MHz) but the real value may vary depending on the variations
  *             in voltage and temperature.
  *
  *         (**) HSI_VALUE is a constant defined in stm32l4xx_hal.h file (default value
  *              16 MHz) but the real value may vary depending on the variations
  *              in voltage and temperature.
  *
  *         (***) HSE_VALUE is a constant defined in stm32l4xx_hal.h file (default value
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
void SystemCoreClockUpdate(void)
{
    uint32_t tmp;
    uint32_t msirange;
    uint32_t pllvco;
    uint32_t pllr;
    uint32_t pllsource;
    uint32_t pllm;

    /*
     * Get MSI Range frequency
     */

    if ((RCC->CR & RCC_CR_MSIRGSEL) == RESET) {
        /* MSISRANGE from RCC_CSR applies */
        msirange = (RCC->CSR & RCC_CSR_MSISRANGE) >> 8U;
    } else {
        /* MSIRANGE from RCC_CR applies */
        msirange = (RCC->CR & RCC_CR_MSIRANGE) >> 4U;
    }

    /* MSI frequency range in HZ */
    msirange = MSIRangeTable[msirange];

    /*
     * Get SYSCLK source
     */
    switch (RCC->CFGR & RCC_CFGR_SWS) {
    /* HSI used as system clock source */
    case 0x04:
        SystemCoreClock = HSI_VALUE;
        break;

    /* HSE used as system clock source */
    case 0x08:
        SystemCoreClock = HSE_VALUE;
        break;

    /* PLL used as system clock source */
    case 0x0C:
        /* PLL_VCO = (HSE_VALUE or HSI_VALUE or MSI_VALUE/ PLLM) * PLLN
         * SYSCLK = PLL_VCO / PLLR
         */
        pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC);
        pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> 4U) + 1U;

        switch (pllsource) {
        /* HSI used as PLL clock source */
        case 0x02:
            pllvco = (HSI_VALUE / pllm);
            break;

        /* HSE used as PLL clock source */
        case 0x03:
            pllvco = (HSE_VALUE / pllm);
            break;

        /* MSI used as PLL clock source */
        default:
            pllvco = (msirange / pllm);
            break;
        }

        pllvco = pllvco * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 8U);
        pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> 25U) + 1U) * 2U;
        SystemCoreClock = pllvco / pllr;
        break;

    default:
        SystemCoreClock = msirange;
        break;
    }

    /*
     * Compute HCLK clock frequency
     */

    /* Get HCLK prescaler */
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4U)];

    /* HCLK clock frequency */
    SystemCoreClock >>= tmp;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
