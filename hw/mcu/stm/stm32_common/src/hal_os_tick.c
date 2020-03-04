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

#include <assert.h>
#include "os/mynewt.h"
#include <hal/hal_os_tick.h>
#include <hal/hal_bsp.h>
#include "bsp/bsp.h"

/*
 * Errata for STM32F405, STM32F407, STM32F415, STM32F417.
 * When WFI instruction is placed at address like 0x080xxxx4
 * (also seen for addresses ending with xxx2). System may
 * crash.
 * __WFI function places WFI instruction at address ending with x0 or x8
 * for affected MCUs.
 */

#if defined(STM32F405xx) || defined(STM32F407xx) || \
    defined(STM32F415xx) || defined(STM32F417xx)
#undef __WFI
__attribute__((aligned(8), naked)) void static
__WFI(void)
{
     __ASM volatile("wfi\n"
                    "bx lr");
}
#endif

/* implementation of different power modes available on STM32 processors */
#ifdef STM32L151xC
extern void stm32_power_enter(int power_mode, uint32_t durationMS);
extern void stm32_tick_init(uint32_t os_ticks_per_sec, int prio);
#else
static void 
stm32_tick_init(uint32_t os_ticks_per_sec, int prio) 
{
    /*nb of ticks per seconds is hardcoded in HAL_InitTick(..) to have 1ms/tick */
    assert(os_ticks_per_sec == OS_TICKS_PER_SEC);
    
    uint32_t reload_val;

    reload_val = ((uint64_t)SystemCoreClock / os_ticks_per_sec) - 1;

    /* Set the system time ticker up */
    SysTick->LOAD = reload_val;
    SysTick->VAL = 0;
    SysTick->CTRL = 0x0007;

    /* Set the system tick priority */
    NVIC_SetPriority(SysTick_IRQn, prio);

    /*
     * Keep clocking debug even when CPU is sleeping, stopped or in standby.
     */

#if !MYNEWT_VAL(MCU_STM32F0)
    DBGMCU->CR |= (DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#else
    DBGMCU->CR |= (DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#endif

}

static void 
stm32_power_enter(int power_mode, uint32_t durationMS)
{
    __DSB();
    __WFI();
}

#endif


void
os_tick_idle(os_time_t ticks)
{
    /* default mode will enter in WFI */
    int power_mode = HAL_BSP_POWER_WFI;

    OS_ASSERT_CRITICAL();

    /* if < MIN_TICKS, then just leave standard SYSTICK and WFI to and wakeup in 1ms */
    if (ticks == 0) { 
        __DSB();
        __WFI();
        return;
    }

    /* Convert to ms */
    volatile uint32_t timeMS = os_time_ticks_to_ms32(ticks);

#if MYNEWT_VAL(BSP_POWER_SETUP)
    /* ask bsp for lowest power mode that is possible */
    power_mode = hal_bsp_power_handler_get_mode(timeMS);

    /* Tell BSP we enter sleep, so it should shut down any board periphs it can for the mode it wants */
    hal_bsp_power_handler_sleep_enter(power_mode);

#endif

    /* setup the appropriate mcu power mode during timeMs asked by os scheduling */
    stm32_power_enter(power_mode, timeMS);

#if MYNEWT_VAL(BSP_POWER_SETUP)
    /* bsp exits the actual power mode */
    hal_bsp_power_handler_sleep_exit(power_mode);
#endif


}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    stm32_tick_init(os_ticks_per_sec, prio);
}
