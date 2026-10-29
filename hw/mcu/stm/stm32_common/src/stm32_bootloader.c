/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdint.h>
#include <mcu/stm32_hal.h>
#include <mcu/mcu.h>
#include <os/util.h>

#if defined(STM32F4)
/*
 * stm32f4xx_hal_rcc.c has empty weak HAL_RCC_DeInit(), working one is in
 * stm32f4xx_hal_rcc_ex.c. Reference function from that file so it gets linked,
 * otherwise system bootloader starts with PLL still running and detects wrong
 * HSE frequency (USB DFU does not work).
 */
static void (*const rcc_ex_ref)(RCC_PeriphCLKInitTypeDef *) __attribute__((used)) =
    HAL_RCCEx_GetPeriphCLKConfig;
#endif

void
stm32_start_bootloader(void)
{
    const uint32_t *system_memory = (const uint32_t *)STM32_SYSTEM_MEMORY;
    int i;

    __disable_irq();

    /* Interrupts enabled by application must not reach system bootloader */
    for (i = 0; i < ARRAY_SIZE(NVIC->ICER); ++i) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    SCB_DisableDCache();
#endif
#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    SCB_DisableICache();
#endif

    HAL_RCC_DeInit();
    /*
     * Reset peripherals, this also disconnects USB so host can see
     * device re-enumerate when system bootloader starts its own USB DFU.
     */
    HAL_DeInit();

#ifdef __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH
#ifdef __HAL_RCC_SYSCFG_CLK_ENABLE
    __HAL_RCC_SYSCFG_CLK_ENABLE();
#endif
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
#endif
#ifdef SCB_VTOR_TBLOFF_Msk
    SCB->VTOR = STM32_SYSTEM_MEMORY;
#endif

#if (defined(__ARM_ARCH_8M_MAIN__) && (__ARM_ARCH_8M_MAIN__ == 1)) || \
    (defined(__ARM_ARCH_8M_BASE__) && (__ARM_ARCH_8M_BASE__ == 1))
    /* System memory stack is below limit set for application stack */
    __set_MSPLIM(0);
    __set_PSPLIM(0);
#endif
    __DSB();
    __ISB();
    __enable_irq();

    /*
     * This may be called from task (PSP in use), switch to MSP (CONTROL = 0)
     * in assembly so no stack access happens between stack switch and jump.
     */
    __asm volatile (
        "msr msp, %0\n"
        "msr control, %2\n"
        "isb\n"
        "bx  %1\n"
        :
        : "r" (system_memory[0]), "r" (system_memory[1]), "r" (0)
        : "memory");
}
