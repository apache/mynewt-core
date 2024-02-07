/**
 ******************************************************************************
 * @file    system_stm32h7xx.c
 * @author  MCD Application Team
 * @brief   CMSIS Cortex-Mx Device Peripheral Access Layer System Source File.
 *
 *   This file provides two functions and one global variable to be called from
 *   user application:
 *      - SystemInit(): This function is called at startup just after reset and
 *                      before branch to main program. This call is made inside
 *                      the "startup_stm32h7xx.s" file.
 *
 *      - SystemCoreClock variable: Contains the core clock, it can be used
 *                                  by the user application to setup the SysTick
 *                                  timer or configure other parameters.
 *
 *      - SystemCoreClockUpdate(): Updates the variable SystemCoreClock and must
 *                                 be called whenever the core clock is changed
 *                                 during program execution.
 *
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2017 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/** @addtogroup CMSIS
 * @{
 */

/** @addtogroup stm32h7xx_system
 * @{
 */

/** @addtogroup STM32H7xx_System_Private_Includes
 * @{
 */

#include "bsp/stm32h7xx_hal_conf.h"
#include "stm32h7xx.h"
#include "mcu/cmsis_nvic.h"

/* This variable is updated in three ways:
    1) by calling CMSIS function SystemCoreClockUpdate()
    2) by calling HAL API function HAL_RCC_GetHCLKFreq()
    3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency
       Note: If you use this function to configure the system clock; then there
             is no need to call the 2 first functions listed above, since SystemCoreClock
             variable is updated automatically.
 */
uint32_t SystemCoreClock = 64000000;
uint32_t SystemD2Clock = 64000000;
const uint8_t D1CorePrescTable[16] = {0, 0, 0, 0, 1, 2, 3, 4, 1, 2, 3, 4, 6, 7, 8, 9};
/*
 * XXX BSP specific
 */
void SystemClock_Config(void);

/**
 * @brief  Setup the microcontroller system
 *         Initialize the Embedded Flash Interface, the PLL and update the
 *         SystemFrequency variable.
 * @param  None
 * @retval None
 */
void
SystemInit(void)
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
    RCC->CR |= RCC_CR_HSION;

    /* Reset CFGR register */
    RCC->CFGR = 0x00000000;

    /* Reset HSEON, HSECSSON, CSION, HSI48ON, CSIKERON, PLL1ON, PLL2ON and PLL3ON bits */
    RCC->CR &= 0xEAF6ED7FU;

#if defined(D3_SRAM_BASE)
    /* Reset D1CFGR register */
    RCC->D1CFGR = 0x00000000;
    /* Reset D2CFGR register */
    RCC->D2CFGR = 0x00000000;
    /* Reset D3CFGR register */
    RCC->D3CFGR = 0x00000000;
#else
    /* Reset CDCFGR1 register */
    RCC->CDCFGR1 = 0x00000000;
    /* Reset CDCFGR2 register */
    RCC->CDCFGR2 = 0x00000000;
    /* Reset SRDCFGR register */
    RCC->SRDCFGR = 0x00000000;
#endif

    /* Reset PLLCKSELR register */
    RCC->PLLCKSELR = 0x02020200;

    /* Reset PLLCFGR register */
    RCC->PLLCFGR = 0x01FF0000;
    /* Reset PLL1DIVR register */
    RCC->PLL1DIVR = 0x01010280;
    /* Reset PLL1FRACR register */
    RCC->PLL1FRACR = 0x00000000;

    /* Reset PLL2DIVR register */
    RCC->PLL2DIVR = 0x01010280;

    /* Reset PLL2FRACR register */

    RCC->PLL2FRACR = 0x00000000;
    /* Reset PLL3DIVR register */
    RCC->PLL3DIVR = 0x01010280;

    /* Reset PLL3FRACR register */
    RCC->PLL3FRACR = 0x00000000;

    /* Reset HSEBYP bit */
    RCC->CR &= 0xFFFBFFFFU;
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
 *         (*) HSI_VALUE is a constant defined in stm32f7xx_hal_conf.h file (default value
 *             16 MHz) but the real value may vary depending on the variations
 *             in voltage and temperature.
 *
 *         (**) HSE_VALUE is a constant defined in stm32f7xx_hal_conf.h file (default value
 *              25 MHz), user has to ensure that HSE_VALUE is same as the real
 *              frequency of the crystal used. Otherwise, this function may
 *              have wrong result.
 *
 *         - The result of this function could be not correct when using fractional
 *           value for HSE crystal.
 */
void
SystemCoreClockUpdate(void)
{
    uint32_t pllp, pllsource, pllm, pllfracen, hsivalue, tmp;
    uint32_t common_system_clock;
    float_t fracn1, pllvco;


    /* Get SYSCLK source -------------------------------------------------------*/

    switch (RCC->CFGR & RCC_CFGR_SWS) {
    case RCC_CFGR_SWS_HSI:  /* HSI used as system clock source */
        common_system_clock = (uint32_t) (HSI_VALUE >> ((RCC->CR & RCC_CR_HSIDIV)>> 3));
        break;

    case RCC_CFGR_SWS_CSI:  /* CSI used as system clock  source */
        common_system_clock = CSI_VALUE;
        break;

    case RCC_CFGR_SWS_HSE:  /* HSE used as system clock  source */
        common_system_clock = HSE_VALUE;
        break;

    case RCC_CFGR_SWS_PLL1:  /* PLL1 used as system clock  source */

        /* PLL_VCO = (HSE_VALUE or HSI_VALUE or CSI_VALUE/ PLLM) * PLLN
           SYSCLK = PLL_VCO / PLLR
         */
        pllsource = (RCC->PLLCKSELR & RCC_PLLCKSELR_PLLSRC);
        pllm = ((RCC->PLLCKSELR & RCC_PLLCKSELR_DIVM1)>> 4);
        pllfracen = ((RCC->PLLCFGR & RCC_PLLCFGR_PLL1FRACEN)>>RCC_PLLCFGR_PLL1FRACEN_Pos);
        fracn1 = (float_t)(uint32_t)(pllfracen* ((RCC->PLL1FRACR & RCC_PLL1FRACR_FRACN1)>> 3));

        if (pllm != 0U) {
            switch (pllsource) {
            case RCC_PLLCKSELR_PLLSRC_HSI:  /* HSI used as PLL clock source */

                hsivalue = (HSI_VALUE >> ((RCC->CR & RCC_CR_HSIDIV)>> 3));
                pllvco = ((float_t)hsivalue / (float_t)pllm) *
                         ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_N1) + (fracn1/(float_t)0x2000) +
                          (float_t)1);
                break;

            case RCC_PLLCKSELR_PLLSRC_CSI:  /* CSI used as PLL clock source */
                pllvco = ((float_t)CSI_VALUE / (float_t)pllm) *
                         ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_N1) + (fracn1/(float_t)0x2000) +
                          (float_t)1);
                break;

            case RCC_PLLCKSELR_PLLSRC_HSE:  /* HSE used as PLL clock source */
                pllvco = ((float_t)HSE_VALUE / (float_t)pllm) *
                         ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_N1) + (fracn1/(float_t)0x2000) +
                          (float_t)1);
                break;

            default:
                hsivalue = (HSI_VALUE >> ((RCC->CR & RCC_CR_HSIDIV)>> 3));
                pllvco = ((float_t)hsivalue / (float_t)pllm) *
                         ((float_t)(uint32_t)(RCC->PLL1DIVR & RCC_PLL1DIVR_N1) + (fracn1/(float_t)0x2000) +
                          (float_t)1);
                break;
            }
            pllp = (((RCC->PLL1DIVR & RCC_PLL1DIVR_P1) >>9) + 1U);
            common_system_clock = (uint32_t)(float_t)(pllvco/(float_t)pllp);
        } else {
            common_system_clock = 0U;
        }
        break;

    default:
        common_system_clock = (uint32_t) (HSI_VALUE >> ((RCC->CR & RCC_CR_HSIDIV)>> 3));
        break;
    }

    /* Compute SystemClock frequency --------------------------------------------------*/
#if defined (RCC_D1CFGR_D1CPRE)
    tmp = D1CorePrescTable[(RCC->D1CFGR & RCC_D1CFGR_D1CPRE)>> RCC_D1CFGR_D1CPRE_Pos];

    /* common_system_clock frequency : CM7 CPU frequency  */
    common_system_clock >>= tmp;

    /* SystemD2Clock frequency : CM4 CPU, AXI and AHBs Clock frequency  */
    SystemD2Clock =
        (common_system_clock >> ((D1CorePrescTable[(RCC->D1CFGR & RCC_D1CFGR_HPRE)>> RCC_D1CFGR_HPRE_Pos]) & 0x1FU));

#else
    tmp = D1CorePrescTable[(RCC->CDCFGR1 & RCC_CDCFGR1_CDCPRE)>> RCC_CDCFGR1_CDCPRE_Pos];

    /* common_system_clock frequency : CM7 CPU frequency  */
    common_system_clock >>= tmp;

    /* SystemD2Clock frequency : AXI and AHBs Clock frequency  */
    SystemD2Clock = (common_system_clock >>
                     ((D1CorePrescTable[(RCC->CDCFGR1 & RCC_CDCFGR1_HPRE)>> RCC_CDCFGR1_HPRE_Pos]) & 0x1FU));

#endif

#if defined(DUAL_CORE) && defined(CORE_CM4)
    SystemCoreClock = SystemD2Clock;
#else
    SystemCoreClock = common_system_clock;
#endif /* DUAL_CORE && CORE_CM4 */
}

/*!< Uncomment the following line if you need to use external SDRAM mounted
     on DK as data memory  */
/* #define DATA_IN_ExtSDRAM */

#if defined (DATA_IN_ExtSDRAM)
/**
 * @brief  Setup the external memory controller.
 *         Called in startup_stm32f7xx.s before jump to main.
 *         This function configures the external memories (SDRAM)
 *         This SDRAM will be used as program data memory (including heap and stack).
 * @param  None
 * @retval None
 */
void
SystemInit_ExtMemCtl(void)
{
    register uint32_t tmpreg = 0, timeout = 0xFFFF;
    register __IO uint32_t index;

    /* Enable GPIOC, GPIOD, GPIOE, GPIOF, GPIOG and GPIOH interface
       clock */
    RCC->AHB1ENR |= 0x000000FC;

    /* Connect PCx pins to FMC Alternate function */
    GPIOC->AFR[0] = 0x0000C000;
    GPIOC->AFR[1] = 0x00000000;
    /* Configure PCx pins in Alternate function mode */
    GPIOC->MODER = 0x00000080;
    /* Configure PCx pins speed to 50 MHz */
    GPIOC->OSPEEDR = 0x00000080;
    /* Configure PCx pins Output type to push-pull */
    GPIOC->OTYPER = 0x00000000;
    /* No pull-up, pull-down for PCx pins */
    GPIOC->PUPDR = 0x00000040;

    /* Connect PDx pins to FMC Alternate function */
    GPIOD->AFR[0] = 0x000000CC;
    GPIOD->AFR[1] = 0xCC000CCC;
    /* Configure PDx pins in Alternate function mode */
    GPIOD->MODER = 0xA02A000A;
    /* Configure PDx pins speed to 50 MHz */
    GPIOD->OSPEEDR = 0xA02A000A;
    /* Configure PDx pins Output type to push-pull */
    GPIOD->OTYPER = 0x00000000;
    /* No pull-up, pull-down for PDx pins */
    GPIOD->PUPDR = 0x50150005;

    /* Connect PEx pins to FMC Alternate function */
    GPIOE->AFR[0] = 0xC00000CC;
    GPIOE->AFR[1] = 0xCCCCCCCC;
    /* Configure PEx pins in Alternate function mode */
    GPIOE->MODER = 0xAAAA800A;
    /* Configure PEx pins speed to 50 MHz */
    GPIOE->OSPEEDR = 0xAAAA800A;
    /* Configure PEx pins Output type to push-pull */
    GPIOE->OTYPER = 0x00000000;
    /* No pull-up, pull-down for PEx pins */
    GPIOE->PUPDR = 0x55554005;

    /* Connect PFx pins to FMC Alternate function */
    GPIOF->AFR[0] = 0x00CCCCCC;
    GPIOF->AFR[1] = 0xCCCCC000;
    /* Configure PFx pins in Alternate function mode */
    GPIOF->MODER = 0xAA800AAA;
    /* Configure PFx pins speed to 50 MHz */
    GPIOF->OSPEEDR = 0xAA800AAA;
    /* Configure PFx pins Output type to push-pull */
    GPIOF->OTYPER = 0x00000000;
    /* No pull-up, pull-down for PFx pins */
    GPIOF->PUPDR = 0x55400555;

    /* Connect PGx pins to FMC Alternate function */
    GPIOG->AFR[0] = 0x00CC00CC;
    GPIOG->AFR[1] = 0xC000000C;
    /* Configure PGx pins in Alternate function mode */
    GPIOG->MODER = 0x80020A0A;
    /* Configure PGx pins speed to 50 MHz */
    GPIOG->OSPEEDR = 0x80020A0A;
    /* Configure PGx pins Output type to push-pull */
    GPIOG->OTYPER = 0x00000000;
    /* No pull-up, pull-down for PGx pins */
    GPIOG->PUPDR = 0x40010505;

    /* Connect PHx pins to FMC Alternate function */
    GPIOH->AFR[0] = 0x00C0C000;
    GPIOH->AFR[1] = 0x00000000;
    /* Configure PHx pins in Alternate function mode */
    GPIOH->MODER = 0x00000880;
    /* Configure PHx pins speed to 50 MHz */
    GPIOH->OSPEEDR = 0x00000880;
    /* Configure PHx pins Output type to push-pull */
    GPIOH->OTYPER = 0x00000000;
    /* No pull-up, pull-down for PHx pins */
    GPIOH->PUPDR = 0x00000440;

    /* Enable the FMC interface clock */
    RCC->AHB3ENR |= 0x00000001;

    /* Configure and enable SDRAM bank1 */
    FMC_Bank5_6->SDCR[0] = 0x00001954;
    FMC_Bank5_6->SDTR[0] = 0x01115351;

    /* SDRAM initialization sequence */
    /* Clock enable command */
    FMC_Bank5_6->SDCMR = 0x00000011;
    tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
    while ((tmpreg != 0) && (timeout-- > 0)) {
        tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
    }

    /* Delay */
    for (index = 0; index < 1000; index++) {
        ;
    }

    /* PALL command */
    FMC_Bank5_6->SDCMR = 0x00000012;
    timeout = 0xFFFF;
    while ((tmpreg != 0) && (timeout-- > 0)) {
        tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
    }

    /* Auto refresh command */
    FMC_Bank5_6->SDCMR = 0x000000F3;
    timeout = 0xFFFF;
    while ((tmpreg != 0) && (timeout-- > 0)) {
        tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
    }

    /* MRD register program */
    FMC_Bank5_6->SDCMR = 0x00044014;
    timeout = 0xFFFF;
    while ((tmpreg != 0) && (timeout-- > 0)) {
        tmpreg = FMC_Bank5_6->SDSR & 0x00000020;
    }

    /* Set refresh count */
    tmpreg = FMC_Bank5_6->SDRTR;
    FMC_Bank5_6->SDRTR = (tmpreg | (0x0000050C<<1));

    /* Disable write protection */
    tmpreg = FMC_Bank5_6->SDCR[0];
    FMC_Bank5_6->SDCR[0] = (tmpreg & 0xFFFFFDFF);

    /*
     * Disable the FMC bank1 (enabled after reset).
     * This, prevents CPU speculation access on this bank which blocks the use of FMC during
     * 24us. During this time the others FMC master (such as LTDC) cannot use it!
     */
    FMC_Bank1->BTCR[0] = 0x000030d2;
}
#endif /* DATA_IN_ExtSDRAM */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
