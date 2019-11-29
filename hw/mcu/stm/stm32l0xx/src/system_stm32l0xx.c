/**
  * <h2><center>&copy; Copyright(c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  */

#include <stdint.h>
#include "bsp/stm32l0xx_hal_conf.h"
#include "stm32l0xx.h"
#include "mcu/cmsis_nvic.h"
#include "mcu/stm32_hal.h"

/* This variable is updated in three ways:
  1) by calling CMSIS function SystemCoreClockUpdate()
  2) by calling HAL API function HAL_RCC_GetHCLKFreq()
  3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency
     Note: If you use this function to configure the system clock; then there
           is no need to call the 2 first functions listed above, since SystemCoreClock
           variable is updated automatically.
*/

uint32_t SystemCoreClock;
const uint8_t AHBPrescTable[16] = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U
};
const uint8_t APBPrescTable[8] = {
    0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U
};
const uint8_t PLLMulTable[9] = {
    3U, 4U, 6U, 8U, 12U, 16U, 24U, 32U, 48U
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
void
SystemInit(void)
{
    /* Set MSION bit */
    RCC->CR |= (uint32_t)0x00000100U;

    /*
     * Reset SW[1:0], HPRE[3:0], PPRE1[2:0], PPRE2[2:0], MCOSEL[2:0] and MCOPRE[2:0] bits
     */
    RCC->CFGR &= (uint32_t) 0x88FF400CU;

    /* Reset HSION, HSIDIVEN, HSEON, CSSON and PLLON bits */
    RCC->CR &= (uint32_t)0xFEF6FFF6U;

    /* Reset HSI48ON  bit */
    RCC->CRRCR &= (uint32_t)0xFFFFFFFEU;

    /* Reset HSEBYP bit */
    RCC->CR &= (uint32_t)0xFFFBFFFFU;

    /* Reset PLLSRC, PLLMUL[3:0] and PLLDIV[1:0] bits */
    RCC->CFGR &= (uint32_t)0xFF02FFFFU;

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
  * @brief  Update SystemCoreClock according to Clock Register Values
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
  *           - If SYSCLK source is MSI, SystemCoreClock will contain the MSI
  *             value as defined by the MSI range.
  *
  *           - If SYSCLK source is HSI, SystemCoreClock will contain the HSI_VALUE(*)
  *
  *           - If SYSCLK source is HSE, SystemCoreClock will contain the HSE_VALUE(**)
  *
  *           - If SYSCLK source is PLL, SystemCoreClock will contain the HSE_VALUE(**)
  *             or HSI_VALUE(*) multiplied/divided by the PLL factors.
  *
  *         (*) HSI_VALUE is a constant defined in stm32l0xx_hal.h file (default value
  *             16 MHz) but the real value may vary depending on the variations
  *             in voltage and temperature.
  *
  *         (**) HSE_VALUE is a constant defined in stm32l0xx_hal.h file (default value
  *              8 MHz), user has to ensure that HSE_VALUE is same as the real
  *              frequency of the crystal used. Otherwise, this function may
  *              have wrong result.
  *
  *         - The result of this function could be not correct when using fractional
  *           value for HSE crystal.
  * @param  None
  * @retval None
  */
void
SystemCoreClockUpdate(void)
{
    uint32_t tmp;
    uint32_t pllmul;
    uint32_t plldiv;
    uint32_t pllsource;
    uint32_t msirange;

    /* Get SYSCLK source */
    tmp = RCC->CFGR & RCC_CFGR_SWS;

    switch (tmp) {
    /* MSI used as system clock */
    case 0x00U:
        msirange = (RCC->ICSCR & RCC_ICSCR_MSIRANGE) >> RCC_ICSCR_MSIRANGE_Pos;
        SystemCoreClock = (32768U * (1U << (msirange + 1U)));
        break;

    /* HSI used as system clock */
    case 0x04U:
        if ((RCC->CR & RCC_CR_HSIDIVF) != 0U) {
            SystemCoreClock = HSI_VALUE / 4U;
        } else {
            SystemCoreClock = HSI_VALUE;
        }
        break;

    /* HSE used as system clock */
    case 0x08U:
        SystemCoreClock = HSE_VALUE;
        break;

    /* PLL used as system clock */
    default:
        /*
         * Get PLL clock source and multiplication factor
         */
        pllmul = RCC->CFGR & RCC_CFGR_PLLMUL;
        plldiv = RCC->CFGR & RCC_CFGR_PLLDIV;
        pllmul = PLLMulTable[(pllmul >> RCC_CFGR_PLLMUL_Pos)];
        plldiv = (plldiv >> RCC_CFGR_PLLDIV_Pos) + 1U;

        pllsource = RCC->CFGR & RCC_CFGR_PLLSRC;

        if (pllsource == 0x00U) {
            /* HSI oscillator clock selected as PLL clock entry */
            if ((RCC->CR & RCC_CR_HSIDIVF) != 0U) {
                SystemCoreClock = (((HSI_VALUE / 4U) * pllmul) / plldiv);
            } else {
                SystemCoreClock = (((HSI_VALUE) * pllmul) / plldiv);
            }
        } else {
            /* HSE selected as PLL clock entry */
            SystemCoreClock = (((HSE_VALUE) * pllmul) / plldiv);
        }
        break;
    }

    /*
     * Compute HCLK clock frequency
     */

    /* Get HCLK prescaler */
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos)];

    /* HCLK clock frequency */
    SystemCoreClock >>= tmp;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
