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

#include <string.h>
#include <assert.h>
#include <os/mynewt.h>
#include <syscfg/syscfg.h>
#include <mcu/stm32_hal.h>
#include <hal/hal_bsp.h>
#include "stm32l1xx_hal_pwr.h"
#include "stm32l1xx_hal_rtc.h"

extern void SystemClock_RestartPLL(void);

extern void hal_rtc_enable_wakeup(uint32_t time_ms);
extern void hal_rtc_disable_wakeup(void);
extern uint32_t hal_rtc_get_elapsed_wakeup_timer(void);
extern void hal_rtc_init(RTC_DateTypeDef *date, RTC_TimeTypeDef *time);
void stm32_tickless_start(uint32_t timeMS);

/* Put MCU  in lowest power stop state, exit only via POR or reset pin */
void hal_mcu_halt() {

    /* all interupts and exceptions off */
    /* PVD off */
    /* power watchdog off */
    /* Be in lowest power mode forever */

    /*start tickless mode forever */
    stm32_tickless_start(0);

    while(1) {

        /*Disables the Power Voltage Detector(PVD) */                
        HAL_PWR_DisablePVD( );
        /* Enable Ultra low power mode */
        HAL_PWREx_EnableUltraLowPower( );
        /* Enable the fast wake up from Ultra low power mode */
        HAL_PWREx_DisableFastWakeUp( );
        /* Enters Stop mode */
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    }
}

void stm32_tick_init(uint32_t os_ticks_per_sec, int prio) {
    /* Even for tickless we use SYSTICK for normal tick.*/
    /*nb of ticks per seconds is hardcoded in HAL_InitTick(..) to have 1ms/tick */
    assert(os_ticks_per_sec == OS_TICKS_PER_SEC);
    
    volatile uint32_t reload_val;

    /*Reload Value = SysTick Counter Clock (Hz) x  Desired Time base (s) */
    reload_val = ((uint64_t)SystemCoreClock / os_ticks_per_sec) - 1;
    /* Set the system time ticker up */
    SysTick->LOAD = reload_val;
    SysTick->VAL = 0;

    /* CLKSOURCE : 1 -> HCLK, 0-> AHB Clock (which is HCLK/8). Use HCLK, as this is the value of SystemCoreClock as used above */
    SysTick->CTRL = ( SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk );  
        
    /* Set the system tick priority */
    NVIC_SetPriority(SysTick_IRQn, prio);

#ifdef RELEASE_BUILD
    HAL_DBGMCU_DisableDBGSleepMode();
    HAL_DBGMCU_DisableDBGStopMode();
    HAL_DBGMCU_DisableDBGStandbyMode();
#else
    /* Keep clocking debug even when CPU is sleeping, stopped or in standby.*/
    HAL_DBGMCU_EnableDBGSleepMode();
    HAL_DBGMCU_EnableDBGStopMode();
    HAL_DBGMCU_EnableDBGStandbyMode();
#endif

#if MYNEWT_VAL(OS_TICKLESS_RTC)

    /* initialise RTC for tickless code if required */
    hal_rtc_init(NULL, NULL);

#endif



}
void stm32_tickless_start(uint32_t timeMS) {

    /* Start RTC alarm for in this amount of time */
    if(timeMS>0){
        hal_rtc_enable_wakeup(timeMS);
    }
    /* Stop SYSTICK */
    NVIC_DisableIRQ(SysTick_IRQn);
    /* Suspend SysTick Interrupt */
    CLEAR_BIT(SysTick->CTRL,SysTick_CTRL_TICKINT_Msk);
}

void stm32_tickless_stop(uint32_t timeMS) {
    
    /* add asleep duration to tick counter : how long we should have slept for minus any remaining time */
    volatile uint32_t asleep_ms =  hal_rtc_get_elapsed_wakeup_timer();
    volatile int asleep_ticks = os_time_ms_to_ticks32(asleep_ms);
    assert(asleep_ticks>=0);
    os_time_advance(asleep_ticks);

    /* disable RTC */
    hal_rtc_disable_wakeup();

    /* reenable SysTick Interrupt */
    NVIC_EnableIRQ(SysTick_IRQn);
    /* reenable SysTick */
    SET_BIT(SysTick->CTRL,SysTick_CTRL_TICKINT_Msk);
    
}

void stm32_power_enter(int power_mode, uint32_t durationMS)
{
    /* if sleep time was less than MIN_TICKS, it is 0. Just do usual WFI and systick will wake us in 1ms */
    if (durationMS==0) {
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
        return;
    }

    if(durationMS >= 32000) {
        /* 32 sec is the largest value of wakeuptimer   */
        /* TODO :  fix this                             */ 
        durationMS = 32000 - 1; 
    }

    /* begin tickless */
#if MYNEWT_VAL(OS_TICKLESS_RTC)
    stm32_tickless_start(durationMS);
#endif

    switch(power_mode) {
        case HAL_BSP_POWER_OFF:
        case HAL_BSP_POWER_DEEP_SLEEP: {
            /*Disables the Power Voltage Detector(PVD) */                
            HAL_PWR_DisablePVD( );
            /* Enable Ultra low power mode */
            HAL_PWREx_EnableUltraLowPower( );
            /* Enable the fast wake up from Ultra low power mode */
            HAL_PWREx_DisableFastWakeUp( );
            /* Enters StandBy mode */
            HAL_PWR_EnterSTANDBYMode();

            SystemClock_RestartPLL();
            break;
        }
        case HAL_BSP_POWER_SLEEP: {

            /*Disables the Power Voltage Detector(PVD) */                
            HAL_PWR_DisablePVD( );
            /* Enable Ultra low power mode */
            HAL_PWREx_EnableUltraLowPower( );
            /* Enable the fast wake up from Ultra low power mode */
            HAL_PWREx_DisableFastWakeUp( );
            /* Enters Stop mode not in PWR_MAINREGULATOR_ON*/
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

            SystemClock_RestartPLL();
            break;
        }
        case HAL_BSP_POWER_WFI: {
            HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

            SystemClock_RestartPLL();            
            break;
        }
        case HAL_BSP_POWER_ON:
        default: {
            
            break;
        }
    }
    
#if MYNEWT_VAL(OS_TICKLESS_RTC)
    /* exit tickless low power mode */
    stm32_tickless_stop(durationMS);
#endif
}
