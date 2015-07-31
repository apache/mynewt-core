/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "stm32f4xx/stm32f4xx.h"
#include "stm32f4xx/system_stm32f4xx.h"

/* 
 * Exception priorities. The higher the number, the lower the priority. A
 * higher priority exception will interrupt a lower priority exception.
 *                                                                    
 * NOTE: This processor supports 4 priority bits.
 */
#define PEND_SV_PRIO    (0x0F)
#define SYSTICK_PRIO    (0x0E)

/* Make the SVC instruction highest priority */
#define SVC_PRIO        (0)

/**
 * os bsp systick init
 *  
 * Initializes systick for the MCU 
 */
void
os_bsp_systick_init(uint32_t os_tick_usecs)
{
    uint32_t reload_val;

    reload_val = (((uint64_t)SystemCoreClock * os_tick_usecs) / 1000000) - 1;

    /* Set the system time ticker up */
    SysTick->LOAD = reload_val;
    SysTick->VAL = 0;
    SysTick->CTRL = 0x0007;

    /* Set the system tick priority */
    NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIO);
}

void
os_bsp_init(void)
{
    /* Set the PendSV interrupt exception priority to the lowest priority */
    NVIC_SetPriority(PendSV_IRQn, PEND_SV_PRIO);

    /* Set the SVC interrupt to priority 0 (highest configurable) */
    NVIC_SetPriority(SVCall_IRQn, SVC_PRIO);
}

void
os_bsp_ctx_sw(void)
{
    /* Set PendSV interrupt pending bit to force context switch */
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}


