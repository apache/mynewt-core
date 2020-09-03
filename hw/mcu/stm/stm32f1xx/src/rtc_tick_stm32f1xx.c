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
#include <stm32_common/stm32_hal.h>

#include <Drivers/STM32F1xx_HAL_Driver/Inc/stm32f1xx_ll_rtc.h>

/* RTC tick only if LSE is enabled and OS_TICKS_PER_SEC can work with 32768 oscillator */
#if MYNEWT_VAL(STM32_CLOCK_LSE) && (((32768 / OS_TICKS_PER_SEC) * OS_TICKS_PER_SEC) == 32768)

/*
 * ST's MCUs seems to have problem with accessing AHB interface from SWD during SLEEP.
 * This makes it almost impossible to use with SEGGER SystemView, therefore when OS_SYSVIEW
 * is defined __WFI will become a loop waiting for pending interrupts.
 */
#if MYNEWT_VAL(OS_SYSVIEW)
#undef __WFI
#define __WFI() do { } while ((SCB->ICSR & (SCB_ICSR_ISRPENDING_Msk | SCB_ICSR_PENDSTSET_Msk)) == 0)
#endif

struct hal_os_tick {
    uint32_t rtc_cnt;
};

struct hal_os_tick g_hal_os_tick;

static void
stm32_os_tick_update_rtc(void)
{
    int ticks;
    uint32_t rtc_cnt;

    /* Clear all flags. */
    RTC->CRL = 0;
    /* Read current value of counter. */
    rtc_cnt = (RTC->CNTH << 16u) | RTC->CNTL;
    if (RTC->CRL & RTC_CRL_SECF_Msk) {
        /* Read again in case it was just incremented. */
        rtc_cnt = (RTC->CNTH << 16u) | RTC->CNTL;
    }
    ticks = rtc_cnt - g_hal_os_tick.rtc_cnt;
    g_hal_os_tick.rtc_cnt = rtc_cnt;

    os_time_advance(ticks);
}

static void
stm32_os_tick_rtc_handler(void)
{
    os_trace_isr_enter();
    stm32_os_tick_update_rtc();
    os_trace_isr_exit();
}

void
os_tick_idle(os_time_t ticks)
{
    uint32_t rtc_cnt;
    OS_ASSERT_CRITICAL();

    if (ticks > 1) {
        while (!LL_RTC_IsActiveFlag_RTOF(RTC));
        LL_RTC_ClearFlag_SEC(RTC);
        rtc_cnt = (RTC->CNTH << 16u) + RTC->CNTL;
        if (LL_RTC_IsActiveFlag_SEC(RTC)) {
            rtc_cnt = (RTC->CNTH << 16u) + RTC->CNTL;
            ticks--;
        }
        rtc_cnt += ticks - 1;
        /* All flags cleared, disable write protection */
        RTC->CRL = RTC_CRL_CNF_Msk;

        LL_RTC_ALARM_Set(RTC, rtc_cnt);
        /* Enable alarm, disable tick interrupt */
        RTC->CRH = RTC_CRH_ALRIE_Msk;
        LL_RTC_EnableWriteProtection(RTC);
    } else {
        /* Disable alarm, enable tick interrupt. */
        RTC->CRH = RTC_CRH_SECIE_Msk;
    }

    __DSB();
    __WFI();

    if (ticks > 0) {
        /*
         * Update OS time before anything else when coming out of
         * the tickless regime.
         */
        stm32_os_tick_update_rtc();
    }
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t sr;
    RCC_PeriphCLKInitTypeDef clock_init = {
        .PeriphClockSelection = RCC_PERIPHCLK_RTC,
        .RTCClockSelection = RCC_RTCCLKSOURCE_LSE,
    };

    HAL_RCCEx_PeriphCLKConfig(&clock_init);
    __HAL_RCC_RTC_ENABLE();

    OS_ENTER_CRITICAL(sr);
    /* RTCCLK = 32768 Hz. */
    /* Disable interrupts. */
    RTC->CRH = 0;
    /* Enter configuration mode, clear all flags. */
    RTC->CRL = RTC_CRL_CNF_Msk;
    /* TR_CLK = OS_TICKS_PER_SEC Hz. */
    LL_RTC_SetAsynchPrescaler(RTC, ((32768 / OS_TICKS_PER_SEC) - 1));
    LL_RTC_TIME_Set(RTC, 0);
    LL_RTC_EnableIT_SEC(RTC);
    /* Exit configuration mode. */
    RTC->CRL = 0;

    /* Set the system tick priority. */
    NVIC_SetPriority(RTC_IRQn, prio);
    NVIC_SetVector(RTC_IRQn, (uint32_t)stm32_os_tick_rtc_handler);
    NVIC_EnableIRQ(RTC_IRQn);

    OS_EXIT_CRITICAL(sr);
}

#endif
