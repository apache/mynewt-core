/*
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 */

#include <stdint.h>
#include "bsp/stm32wbxx_hal_conf.h"
#include "stm32wbxx.h"
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
const uint32_t AHBPrescTable[16] = {
    1UL, 3UL, 5UL, 1UL, 1UL, 6UL, 10UL, 32UL, 2UL, 4UL, 8UL, 16UL, 64UL,
    128UL, 256UL, 512UL,
};
const uint32_t APBPrescTable[8] = {
    0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL,
};
/* 0UL values are incorrect cases */
const uint32_t MSIRangeTable[16] = {
    100000UL, 200000UL, 400000UL, 800000UL, 1000000UL, 2000000UL,
    4000000UL, 8000000UL, 16000000UL, 24000000UL, 32000000UL, 48000000UL,
    0UL, 0UL, 0UL, 0UL,
};
const uint32_t SmpsPrescalerTable[4][6] = {
    { 1UL,3UL,2UL,2UL,1UL,2UL, },
    { 2UL,6UL,4UL,3UL,2UL,4UL, },
    { 4UL,12UL,8UL,6UL,4UL,8UL, },
    { 4UL,12UL,8UL,6UL,4UL,8UL, },
};

/*
 * XXX BSP specific
 */
void SystemClock_Config(void);

/**
  * @brief  Setup the microcontroller system.
  *         Initialize the default HSI clock source, vector table location and the PLL configuration is reset.
  * @param  None
  * @retval None
  */
void SystemInit(void)
{
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
        /* Set CP10 and CP11 Full Access */
        SCB->CPACR |= ((3UL << (10UL*2UL))|(3UL << (11UL*2UL)));
    #endif

    /*
     * Reset the RCC clock configuration to the default reset state
     */

    /* Set MSION bit */
    RCC->CR |= RCC_CR_MSION;

    /* Reset CFGR register */
    RCC->CFGR = 0x00070000U;

    /* Reset PLLSAI1ON, PLLON, HSECSSON, HSEON, HSION, and MSIPLLON bits */
    RCC->CR &= (uint32_t)0xFAF6FEFBU;

    /* Reset LSI1 and LSI2 bits */
    RCC->CSR &= (uint32_t)0xFFFFFFFAU;

    /* Reset HSI48ON  bit */
    RCC->CRRCR &= (uint32_t)0xFFFFFFFEU;

    /* Reset PLLCFGR register */
    RCC->PLLCFGR = 0x22041000U;

    /* Reset PLLSAI1CFGR register */
    RCC->PLLSAI1CFGR = 0x22041000U;

    /* Reset HSEBYP bit */
    RCC->CR &= 0xFFFBFFFFU;

    /* Disable all interrupts */
    RCC->CIER = 0x00000000;

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
  *         (*) MSI_VALUE is a constant defined in stm32wbxx_hal.h file (default value
  *             4 MHz) but the real value may vary depending on the variations
  *             in voltage and temperature.
  *
  *         (**) HSI_VALUE is a constant defined in stm32wbxx_hal_conf.h file (default value
  *              16 MHz) but the real value may vary depending on the variations
  *              in voltage and temperature.
  *
  *         (***) HSE_VALUE is a constant defined in stm32wbxx_hal_conf.h file (default value
  *              32 MHz), user has to ensure that HSE_VALUE is same as the real
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

    /* MSI frequency range in Hz */
    msirange = MSIRangeTable[(RCC->CR & RCC_CR_MSIRANGE) >> RCC_CR_MSIRANGE_Pos];

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

    /* PLL used as system clock  source */
    case 0x0C:
        /* PLL_VCO = (HSE_VALUE or HSI_VALUE or MSI_VALUE/ PLLM) * PLLN
         * SYSCLK = PLL_VCO / PLLR
         */
        pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC);
        pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1UL;

        if (pllsource == 0x02UL) {
            /* HSI used as PLL clock source */
            pllvco = (HSI_VALUE / pllm);
        } else if (pllsource == 0x03UL) {
            /* HSE used as PLL clock source */
            pllvco = (HSE_VALUE / pllm);
        } else {
            /* MSI used as PLL clock source */
            pllvco = (msirange / pllm);
        }

        pllvco = pllvco * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos);
        pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1UL);
        SystemCoreClock = pllvco/pllr;
        break;

    /* MSI used as system clock source */
    default:
        SystemCoreClock = msirange;
        break;
    }

    /*
     * Compute HCLK clock frequency
     */

    /* Get HCLK1 prescaler */
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos)];

    /* HCLK clock frequency */
    SystemCoreClock = SystemCoreClock / tmp;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
