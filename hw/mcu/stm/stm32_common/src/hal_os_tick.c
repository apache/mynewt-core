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

#if MYNEWT_VAL(OS_TICKS_USE_RTC)
#include <stm32_common/stm32_hal.h>
#include <datetime/datetime.h>
#endif

/*
 * ST's MCUs seems to have problem with accessing AHB interface from SWD during SLEEP.
 * This makes it almost impossible to use with SEGGER SystemView, therefore when OS_SYSVIEW
 * is defined __WFI will become a loop waiting for pending interrupts.
 */
#if MYNEWT_VAL(OS_SYSVIEW)
#undef __WFI
#define __WFI() do { } while ((SCB->ICSR & (SCB_ICSR_ISRPENDING_Msk | SCB_ICSR_PENDSTSET_Msk)) == 0)
#else
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
#endif

#if MYNEWT_VAL(OS_TICKS_USE_RTC)

#if MYNEWT_VAL(STM32_CLOCK_LSE) && (((32768 / OS_TICKS_PER_SEC) * OS_TICKS_PER_SEC) != 32768)
#error OS_TICKS_PER_SEC should be divisible by power of 2 like 128, 256, 512, 1024 when OS_TICKS_USE_RTC is enabled.
#endif

#ifndef HAL_RTC_MODULE_ENABLED
#error Define HAL_RTC_MODULE_ENABLED in stm32[fl][0-4]xx_hal_conf.h in your bsp
#endif

#define ASYNCH_PREDIV       7
#define SYNCH_PREDIV        (32768 / (ASYNCH_PREDIV + 1) - 1)
#define SUB_SECONDS_BITS    12

#if defined(STM32L0) || defined(STM32F0) || defined(STM32U5)
#define RTC_IRQ RTC_IRQn
#else
#define RTC_IRQ RTC_Alarm_IRQn
#endif

#if defined(STM32L0) || defined(STM32L1)
#define IS_RTC_ENABLED (RCC->CSR & RCC_CSR_RTCEN)
#else
#define IS_RTC_ENABLED (RCC->BDCR & RCC_BDCR_RTCEN)
#endif

_Static_assert(SUB_SECONDS_BITS == __builtin_popcount(SYNCH_PREDIV),
    "SUB_SECONDS_BITS should be number of 1s in SYNCH_PREDIV");

/* RTC time of the last tick. */
static uint32_t last_rtc_time;
static uint32_t sub_seconds_per_tick;
static uint8_t sub_seconds_tick_bits;

/* RTC holds UTC time. */
static RTC_HandleTypeDef rtc = {
    .Instance = RTC,
    .Init = {
        .HourFormat = RTC_HOURFORMAT_24,
        .OutPut = RTC_OUTPUT_DISABLE,
        .OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH,
        .OutPutType = RTC_OUTPUT_TYPE_PUSHPULL,
    }
};

/* RTC AlarmA used for OS ticks. */
static RTC_AlarmTypeDef alarm = {
    .AlarmTime = {
        .Hours = 0,
        .Minutes = 0,
        .Seconds = 0,
        .SubSeconds = 0,
        .TimeFormat = RTC_HOURFORMAT12_AM,
        .SecondFraction = 0,
        .DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
        .StoreOperation = RTC_STOREOPERATION_RESET,
    },
    .AlarmMask = RTC_ALARMMASK_ALL,
    .AlarmSubSecondMask = 0,
    .Alarm = RTC_ALARM_A,
};

static uint32_t
rtc_time_to_sub_seconds(const RTC_TimeTypeDef *time)
{
    uint32_t sub_seconds;

    /* Convert sub second filed to running up sub seconds. */
    sub_seconds = time->SecondFraction - time->SubSeconds;

    sub_seconds += (((time->Hours * 60 + time->Minutes) * 60) + time->Seconds) << SUB_SECONDS_BITS;

    /* Round down to ticks. */
    sub_seconds &= ~((1 << sub_seconds_tick_bits) - 1);

    return sub_seconds;
}

static void
sub_seconds_to_rtc(uint32_t sub_seconds, RTC_TimeTypeDef *time)
{
    time->SubSeconds = time->SecondFraction - (sub_seconds & time->SecondFraction);
    sub_seconds >>= SUB_SECONDS_BITS;
    time->Seconds = sub_seconds % 60;
    sub_seconds /= 60;
    time->Minutes = sub_seconds % 60;
    sub_seconds /= 60;
    time->Hours = sub_seconds % 24;
}

static void
rtc_update_time(void)
{
    int32_t delta;
    uint32_t now;

    HAL_RTC_GetTime(&rtc, &alarm.AlarmTime, RTC_FORMAT_BIN);
    /* Get sub seconds rounded down to tick. */
    now = rtc_time_to_sub_seconds(&alarm.AlarmTime);
    delta = now - last_rtc_time;
    if (delta < 0) {
        /* Day changed, correct delta */
        delta += (24 * 3600) << SUB_SECONDS_BITS;
    }
    alarm.AlarmTime.SubSeconds = alarm.AlarmTime.SecondFraction - (now & alarm.AlarmTime.SecondFraction);
    alarm.AlarmTime.SubSeconds -= sub_seconds_per_tick;
    if ((int32_t)alarm.AlarmTime.SubSeconds < 0) {
        alarm.AlarmTime.SubSeconds += alarm.AlarmTime.SecondFraction + 1;
        alarm.AlarmTime.Seconds++;
        if (alarm.AlarmTime.Seconds >= 60) {
            alarm.AlarmTime.Seconds = 0;
            alarm.AlarmTime.Minutes++;
            if (alarm.AlarmTime.Minutes >= 60) {
                alarm.AlarmTime.Minutes = 0;
                if (alarm.AlarmTime.Hours >= 24) {
                    alarm.AlarmTime.Hours = 0;
                }
            }
        }
    }
    /* Switch to tick timer interrupt by unmasking only sub second alarm bits. */
    alarm.AlarmMask = RTC_ALARMMASK_ALL;
    alarm.AlarmSubSecondMask = sub_seconds_tick_bits << RTC_ALRMASSR_MASKSS_Pos;
    HAL_RTC_SetAlarm_IT(&rtc, &alarm, RTC_FORMAT_BIN);

    last_rtc_time = now;
    os_time_advance(delta >> sub_seconds_tick_bits);
}

void
os_tick_idle(os_time_t ticks)
{
    uint32_t sub_seconds;
    OS_ASSERT_CRITICAL();

    if (ticks > 0) {
        /* Get current time directly to alarm. */
        HAL_RTC_GetTime(&rtc, &alarm.AlarmTime, RTC_FORMAT_BIN);

        alarm.AlarmSubSecondMask = SUB_SECONDS_BITS << RTC_ALRMASSR_MASKSS_Pos;
        if (ticks < OS_TICKS_PER_SEC) {
            /* Convert sub-seconds to up-counting value. */
            sub_seconds = alarm.AlarmTime.SecondFraction - alarm.AlarmTime.SubSeconds;
            /* Round down to tick. */
            sub_seconds &= ~((1 << sub_seconds_tick_bits) - 1);
            /* Add requested number of ticks. */
            sub_seconds += (ticks & (OS_TICKS_PER_SEC - 1)) << sub_seconds_tick_bits;
            /* Remove any overflow which is OK since we are sticking to 1 second limit. */
            sub_seconds &= (1 << SUB_SECONDS_BITS) - 1;
            /* Convert back to down-counting sub-seconds for alarm to use. */
            alarm.AlarmTime.SubSeconds = alarm.AlarmTime.SecondFraction - sub_seconds;
            alarm.AlarmMask = RTC_ALARMMASK_ALL;
        } else {
            sub_seconds = rtc_time_to_sub_seconds(&alarm.AlarmTime);
            sub_seconds += ticks << sub_seconds_tick_bits;
            sub_seconds_to_rtc(sub_seconds, &alarm.AlarmTime);
            alarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
        }
        HAL_RTC_SetAlarm_IT(&rtc, &alarm, RTC_FORMAT_BIN);
    }

    __DSB();
    __WFI();

    if (ticks > 0) {
        rtc_update_time();
    }
}

/* ST HAL interrupt handler calls this function. */
void
HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;
    rtc_update_time();
}

void
RTC_Alarm_IRQHandler(void)
{
    int sr;

    os_trace_isr_enter();

    OS_ENTER_CRITICAL(sr);
    HAL_RTC_AlarmIRQHandler(&rtc);
    OS_EXIT_CRITICAL(sr);

    os_trace_isr_exit();
}

static void
stm32_rtc_os_time_change(const struct os_time_change_info *info, void *arg)
{
    struct clocktime ct;
    RTC_DateTypeDef date;
    uint32_t sub_seconds;
    int sr;

    timeval_to_clocktime(info->tci_cur_tv, NULL, &ct);
    date.Year = ct.year - 2000;
    date.Month = ct.mon;
    date.Date = ct.day;
    /* ST HAL Sunday is 7. */
    date.WeekDay = ct.dow ? ct.dow : 7;
    sub_seconds = ct.usec * 4096U / 1000000U;

    OS_ENTER_CRITICAL(sr);

    alarm.AlarmTime.Hours = ct.hour;
    alarm.AlarmTime.Minutes = ct.min;
    alarm.AlarmTime.Seconds = ct.sec;
    alarm.AlarmTime.SubSeconds = alarm.AlarmTime.SecondFraction;

    HAL_RTC_SetTime(&rtc, &alarm.AlarmTime, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&rtc, &date, RTC_FORMAT_BIN);
    if (sub_seconds) {
        RTC->SHIFTR = RTC_SHIFTR_ADD1S | sub_seconds;
    }
    last_rtc_time = rtc_time_to_sub_seconds(&alarm.AlarmTime);
    alarm.AlarmTime.SubSeconds -= sub_seconds_per_tick;
    alarm.AlarmMask = RTC_ALARMMASK_ALL;
    HAL_RTC_SetAlarm_IT(&rtc, &alarm, RTC_FORMAT_BIN);

    OS_EXIT_CRITICAL(sr);
}

static struct os_time_change_listener rtc_setter = {
    .tcl_fn = stm32_rtc_os_time_change,
};

static void
set_os_datetime_from_rtc(const RTC_TimeTypeDef *time, const RTC_DateTypeDef *date)
{
    struct os_timeval utc;
    struct clocktime ct;
    ct.year = 2000 + date->Year;
    ct.mon = date->Month;
    ct.day = date->Date;
    ct.dow = date->WeekDay == 7 ? 0 : date->WeekDay;
    ct.hour = time->Hours;
    ct.min = time->Minutes;
    ct.sec = time->Seconds;
    ct.usec = ((time->SecondFraction - time->SubSeconds) * 1000000) >> SUB_SECONDS_BITS;
    clocktime_to_timeval(&ct, NULL, &utc);
    os_settimeofday(&utc, NULL);
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t sr;
    RTC_DateTypeDef date = { .Year = 20, .Month = 1, .Date = 1, .WeekDay = RTC_WEEKDAY_WEDNESDAY };
    RTC_TimeTypeDef rtc_time = { 0 };

    RCC_PeriphCLKInitTypeDef clock_init = {
        .PeriphClockSelection = RCC_PERIPHCLK_RTC,
        .RTCClockSelection = RCC_RTCCLKSOURCE_LSE,
    };
    HAL_RCCEx_PeriphCLKConfig(&clock_init);

    /* Set the system tick priority. */
    NVIC_SetPriority(RTC_IRQ, prio);
    NVIC_SetVector(RTC_IRQ, (uint32_t)RTC_Alarm_IRQHandler);

#ifdef __HAL_RCC_RTCAPB_CLK_ENABLE
    __HAL_RCC_RTCAPB_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_RTCAPB_CLKAM_ENABLE
    __HAL_RCC_RTCAPB_CLKAM_ENABLE();
#endif
    /* If RTC is already on get time and date before reinit. */
    if (IS_RTC_ENABLED) {
        HAL_RTC_GetTime(&rtc, &rtc_time, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&rtc, &date, RTC_FORMAT_BIN);
    } else {
        bzero(&rtc_time, sizeof(rtc_time));
        __HAL_RCC_RTC_ENABLE();
    }

    __HAL_DBGMCU_FREEZE_RTC();

#if MYNEWT_VAL(MCU_STM32F0) || MYNEWT_VAL(MCU_STM32U5)
    DBGMCU->CR |= (DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#else
    DBGMCU->CR |= (DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#endif

    OS_ENTER_CRITICAL(sr);

    /* RTCCLK 32768 Hz, ck_apre = 4096 Hz, ck_spre = 1Hz. */
    rtc.Init.AsynchPrediv = ASYNCH_PREDIV;
    rtc.Init.SynchPrediv = SYNCH_PREDIV;
    alarm.AlarmSubSecondMask = SUB_SECONDS_BITS << RTC_ALRMASSR_MASKSS_Pos;
    sub_seconds_per_tick = 32768 / 8 / os_ticks_per_sec;
    sub_seconds_tick_bits = __builtin_popcount(sub_seconds_per_tick - 1);

    HAL_RTC_Init(&rtc);
    HAL_RTCEx_EnableBypassShadow(&rtc);
    HAL_RTC_SetTime(&rtc, &rtc_time, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&rtc, &date, RTC_FORMAT_BIN);
    HAL_RTC_GetTime(&rtc, &rtc_time, RTC_FORMAT_BIN);
    last_rtc_time = rtc_time_to_sub_seconds(&rtc_time);

    alarm.AlarmTime.SubSeconds = rtc.Init.SynchPrediv - sub_seconds_per_tick;

    HAL_RTC_SetAlarm_IT(&rtc, &alarm, RTC_FORMAT_BIN);

    OS_EXIT_CRITICAL(sr);

    /* Set OS date time, then subscribe to changes so initial change will not trigger RTC update. */
    set_os_datetime_from_rtc(&rtc_time, &date);
    os_time_change_listen(&rtc_setter);

    NVIC_EnableIRQ(RTC_IRQ);
}

#else
#if MYNEWT_VAL(STM32_CLOCK_LSE) == 0 || (((32768 / OS_TICKS_PER_SEC) * OS_TICKS_PER_SEC) != 32768) || !defined(STM32F1)

void
os_tick_idle(os_time_t ticks)
{
    OS_ASSERT_CRITICAL();
    __DSB();
    __WFI();
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
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
#if MYNEWT_VAL(MCU_STM32F0) || MYNEWT_VAL(MCU_STM32U5)
    DBGMCU->CR |= (DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#elif MYNEWT_VAL(MCU_STM32H7)
    DBGMCU->CR |= (DBGMCU_CR_DBG_SLEEPD1 | DBGMCU_CR_DBG_STOPD1 | DBGMCU_CR_DBG_STANDBYD1);
#else
    DBGMCU->CR |= (DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#endif
}

#endif
#endif
