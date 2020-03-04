/**
 * Copyright 2019 Wyres
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at
 *    http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, 
 * software distributed under the License is distributed on 
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, 
 * either express or implied. See the License for the specific 
 * language governing permissions and limitations under the License.
 */

/**
 * Utilities for rtc management
 */

#include <assert.h>
#include "os/mynewt.h"

#include "hal/hal_gpio.h"
#include "stm32l1xx_hal_rcc.h"
#include "stm32l1xx_hal_rtc.h"
#include "stm32l1xx_hal_pwr.h"
#include "bsp.h"
#include "limits.h"

/*Follower technic is a way to calcultate the amount of time elapsed during wakeup timer        */
/*Follower needs a resolution similar to wakeup timer resolution.                               */
/*RTC_SSR is the subsecond downcounter used for calendar block and clocked by LSE subdivided    */
/*by asynchronous prescaler                                                                     */

/* Asynchronous prediv to get 1kHz about (close to SysTick frequency). Then ck_apre = 1024Hz    */
#define DIVIDED_FOLLOWER_FREQUENCY                  1024
#define FOLLOWER_PRESCALER_A                        (LSE_VALUE / DIVIDED_FOLLOWER_FREQUENCY)
#define PREDIV_A                                    (FOLLOWER_PRESCALER_A - 1)

/* Synchronous prediv :                                                                         */
/*    The ck_apre clock is used to clock the binary RTC_SSR subseconds downcounter. When it     */
/*    reaches 0, RTC_SSR is reloaded with the content of PREDIV_S. RTC_SSR is available in      */
/*    in Cat.2, Cat.3, Cat.4, Cat.5 and Cat.6 devices only                                      */
#define FOLLOWER_PRESCALER_S                        32768
#define PREDIV_S                                    (FOLLOWER_PRESCALER_S - 1)



/*!
 * \brief Days, Hours, Minutes and seconds
 */
#define DAYS_IN_LEAP_YEAR                           (( uint32_t )  366U)
#define DAYS_IN_YEAR                                (( uint32_t )  365U)
#define SECONDS_IN_1DAY                             (( uint32_t )86400U)
#define SECONDS_IN_1HOUR                            (( uint32_t ) 3600U)
#define SECONDS_IN_1MINUTE                          (( uint32_t )   60U)
#define MINUTES_IN_1HOUR                            (( uint32_t )   60U)
#define HOURS_IN_1DAY                               (( uint32_t )   24U)

/*!
 * \brief Correction factors
 */
#define  DAYS_IN_MONTH_CORRECTION_NORM              (( uint32_t )0x99AAA0)
#define  DAYS_IN_MONTH_CORRECTION_LEAP              (( uint32_t )0x445550)

/*!
 * \brief Calculates ceiling( X / N )
 */
#define DIVC(X, N)                                (((X) + (N) -1) / (N))


#define RTC_CLOCK_PRESCALER                         16.0f
#define DIVIDED_RTC_FREQUENCY                       (((float)LSE_VALUE)/RTC_CLOCK_PRESCALER)
#define DIVIDED_RTC_PERIOD                          (1.0f/DIVIDED_RTC_FREQUENCY)
#define MIN_RTC_PERIOD_SEC                          (1.0f*DIVIDED_RTC_PERIOD)
#define MAX_RTC_PERIOD_SEC                          (65535.0f*DIVIDED_RTC_PERIOD)
#define MIN_RTC_PERIOD_MSEC                         (1000.0f*MIN_RTC_PERIOD_SEC)
#define MAX_RTC_PERIOD_MSEC                         (1000.0f*MAX_RTC_PERIOD_SEC)




/*!
 * RTC timer context 
 */
typedef struct {
    uint32_t Time;                  /* Reference time­ */
    RTC_TimeTypeDef CalendarTime;   /* Reference time in calendar format */
    RTC_DateTypeDef CalendarDate;   /* Reference date in calendar format */
}RtcTimerContext_t;


/*!
 * \brief RTC Handle
 */
static RTC_HandleTypeDef RtcHandle = {
    .Instance = NULL,
    .Init = { 
        .HourFormat = 0,
        .AsynchPrediv = 0,
        .SynchPrediv = 0,
        .OutPut = 0,
        .OutPutPolarity = 0,
        .OutPutType = 0
    },
    .Lock = HAL_UNLOCKED,
    .State = HAL_RTC_STATE_RESET
};

static struct {
    uint32_t clk_srce_sel;
    uint16_t autoreload_timer;
    uint16_t follower_counter_start;
} WakeUpTimer_Settings;



/**
  * @brief  This function handles  WAKE UP TIMER  interrupt request.
  * @retval None
  */
void 
RTC_WKUP_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&RtcHandle);
}


#ifdef RTC_ALARM_TEST
static RTC_AlarmTypeDef RtcAlarm;
#endif

/**
  * @brief  This function handles  ALARM (A&B)  interrupt request.
  * @retval None
  */
void 
RTC_Alarm_IRQHandler(void)
{
    HAL_RTC_AlarmIRQHandler(&RtcHandle);
}

/**
  * @brief  Alarm A callback.
  * @param  hrtc: pointer to a RTC_HandleTypeDef structure that contains
  *                the configuration information for RTC.
  * @retval None
  */
void 
HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{

}


void 
hal_rtc_init(RTC_DateTypeDef *date, RTC_TimeTypeDef *time)
{
    int rc;

    NVIC_DisableIRQ(RTC_WKUP_IRQn);
    NVIC_DisableIRQ(RTC_Alarm_IRQn);
    NVIC_DisableIRQ(TAMPER_STAMP_IRQn);
        
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    __HAL_RCC_RTC_ENABLE( );

    PeriphClkInit.PeriphClockSelection |= RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        assert(0);
    }

    RtcHandle.Instance = RTC;
    RtcHandle.Init.HourFormat = RTC_HOURFORMAT_24;
    RtcHandle.Init.AsynchPrediv = PREDIV_A;
    RtcHandle.Init.SynchPrediv = PREDIV_S;
    RtcHandle.Init.OutPut = RTC_OUTPUT_DISABLE;
    RtcHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    RtcHandle.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    
    rc = HAL_RTC_Init(&RtcHandle);
    assert(rc == 0);

    if (date == NULL) {
        RTC_DateTypeDef default_date = {
            .Year = 0,
            .Month = RTC_MONTH_JANUARY,
            .Date = 1,
            .WeekDay = RTC_WEEKDAY_MONDAY,
        };

        date = &default_date;
    }

    if (time==NULL) {
        RTC_TimeTypeDef default_time = {
            .Hours = 0,
            .Minutes = 0,
            .Seconds = 0,
            .SubSeconds = 0,
            .TimeFormat = 0,
            .StoreOperation = RTC_STOREOPERATION_RESET,
            .DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
        };
        
        time = &default_time;
    }

    HAL_RTC_SetDate(&RtcHandle, date, RTC_FORMAT_BIN);
    HAL_RTC_SetTime(&RtcHandle, time, RTC_FORMAT_BIN);

    // Enable Direct Read of the calendar registers (not through Shadow registers)
    HAL_RTCEx_DisableBypassShadow(&RtcHandle);
    
#ifdef RTC_ALARM_TEST   
    //setup alarm
    RtcAlarm.AlarmTime.Hours = 0;
    RtcAlarm.AlarmTime.Minutes = 0;
    RtcAlarm.AlarmTime.Seconds = 1;
    RtcAlarm.AlarmTime.SubSeconds = 0;
    RtcAlarm.AlarmTime.TimeFormat = RTC_HOURFORMAT12_AM;
    RtcAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    RtcAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
    RtcAlarm.AlarmMask = RTC_ALARMMASK_SECONDS;
    RtcAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
    RtcAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
    RtcAlarm.AlarmDateWeekDay = 1;
    RtcAlarm.Alarm = RTC_ALARM_A;

    NVIC_SetPriority(RTC_Alarm_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(RTC_Alarm_IRQn, (uint32_t)RTC_Alarm_IRQHandler);
    NVIC_EnableIRQ(RTC_Alarm_IRQn);

    rc = HAL_RTC_SetAlarm_IT(&RtcHandle, &RtcAlarm, FORMAT_BIN);
#else

    HAL_RTC_DeactivateAlarm(&RtcHandle, RTC_ALARM_A);
    HAL_RTC_DeactivateAlarm(&RtcHandle, RTC_ALARM_B);
    NVIC_DisableIRQ(RTC_Alarm_IRQn);
#endif

    /*Prepare WakeUp capabilities */
    HAL_RTCEx_DeactivateWakeUpTimer(&RtcHandle);
    /*RTC WAKEUP used as tickless may have the same priority than Systick */
    NVIC_SetPriority(RTC_WKUP_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    /* Note : IRQ handler is not configured into hal. Do it here.*/
    NVIC_SetVector(RTC_WKUP_IRQn, (uint32_t)RTC_WKUP_IRQHandler);
    /*Enable IRQ now forever */
    NVIC_EnableIRQ(RTC_WKUP_IRQn);
    
    /*Initialises start value of the follower*/
    WakeUpTimer_Settings.follower_counter_start = 
        (FOLLOWER_PRESCALER_S - (uint32_t)((RtcHandle.Instance->SSR) & RTC_SSR_SS));

    assert(rc == 0);
    
}


void 
hal_rtc_enable_wakeup(uint32_t time_ms)
{    
    int rc;

    /* WARNING : works only with time_ms =< 32 s (MAX_RTC_PERIOD_MSEC)
                (due to the follower which is currently 
                unable to setup lower resolution)
       
       TODO : improve setup of follower's resolution 

       NOTE : for time_ms > 32 follower could be used as is by using 
              ALARM features (assuming RTC clocking very different of 1Hz)
    */


    /*
     * The wakeup timer clock input can be:
     * • RTC clock (RTCCLK) divided by 2, 4, 8, or 16.
     * When RTCCLK is LSE(32.768kHz), this allows configuring the wakeup interrupt period
     * from 122 µs to 32 s, with a resolution down to 61µs
     * • ck_spre (usually 1 Hz internal clock)
     * When ck_spre frequency is 1Hz, this allows achieving a wakeup time from 1 s to
     * around 36 hours with one-second resolution. This large programmable time range is
     * divided in 2 parts:
     * – from 1s to 18 hours when WUCKSEL [2:1] = 10
     * – and from around 18h to 36h when WUCKSEL[2:1] = 11
     */
    

    if (time_ms < (uint32_t)(MAX_RTC_PERIOD_MSEC)) {
        /* 0 < time_ms < 32 sec */
        WakeUpTimer_Settings.clk_srce_sel = RTC_WAKEUPCLOCK_RTCCLK_DIV16;
        WakeUpTimer_Settings.autoreload_timer = (uint32_t)(time_ms / (MIN_RTC_PERIOD_MSEC));
    } else if (time_ms < (uint32_t)(18 * 60 * 60 * 1000)) {   
        /* 32 sec < time_ms < 18h */
        WakeUpTimer_Settings.clk_srce_sel = RTC_WAKEUPCLOCK_CK_SPRE_16BITS;
        /* load counter */
        WakeUpTimer_Settings.autoreload_timer = (time_ms / 1000);
        /* do little adjustement */
        WakeUpTimer_Settings.autoreload_timer -= 1;
    } else {
        /* 18h < time_ms < 36h */
        /*
         * TODO : setup wakeup with ALARM feature instead of WAKEUP     
         */
        WakeUpTimer_Settings.clk_srce_sel = RTC_WAKEUPCLOCK_CK_SPRE_17BITS;
        WakeUpTimer_Settings.autoreload_timer = (time_ms / 1000);

    }

    /* Setting the Wake up time */
    rc = HAL_RTCEx_SetWakeUpTimer_IT(&RtcHandle, (uint32_t)WakeUpTimer_Settings.autoreload_timer, 
                                     WakeUpTimer_Settings.clk_srce_sel);
    assert (rc == HAL_OK);

    WakeUpTimer_Settings.follower_counter_start = 
        (FOLLOWER_PRESCALER_S - (uint32_t)((RtcHandle.Instance->SSR) & RTC_SSR_SS));
}

uint32_t 
hal_rtc_get_elapsed_wakeup_timer(void)
{
    volatile uint16_t time_ms, follower_counter_elapsed;
    
    /*RTC_SSR is a downcounter*/
    follower_counter_elapsed = (FOLLOWER_PRESCALER_S - (uint16_t)((RtcHandle.Instance->SSR) & RTC_SSR_SS));

    /*elapsed counts is the difference between saved start counter and actual counter */
    follower_counter_elapsed -= WakeUpTimer_Settings.follower_counter_start;

    /*when stop-time wrap around the counter then elapsed time is more than expected */
    if (follower_counter_elapsed > FOLLOWER_PRESCALER_S) {
        follower_counter_elapsed -= (((1<<(CHAR_BIT * sizeof(follower_counter_elapsed)))-1) - FOLLOWER_PRESCALER_S);
    }

    /*convert it into ms*/
    time_ms = ((follower_counter_elapsed * 1000) / DIVIDED_FOLLOWER_FREQUENCY);
    
    return (uint32_t)time_ms;

}

void 
hal_rtc_disable_wakeup(void)
{
    HAL_RTCEx_DeactivateWakeUpTimer(&RtcHandle);
    
    /* In order to take into account the pending IRQ and clear      */
    /* wakeup flags (EXTI->PR & RTC_EXTI_LINE_WAKEUPTIMER_EVENT)    */    
    /* after reentring in critical region : DO NOT DISABLE IRQ !!!  */
    /* IRQ will remain enabled forever                              */
    /* NVIC_DisableIRQ(RTC_WKUP_IRQn); */
}
