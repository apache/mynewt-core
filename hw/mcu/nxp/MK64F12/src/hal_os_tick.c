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
#include <stdio.h>
#include "os/mynewt.h"
#include <hal/hal_os_tick.h>

#include "mcu/cmsis_nvic.h"

#include "fsl_pit.h"

static void nxp_pit0_timer_handler(void)
{
    uint32_t sr;

    OS_ENTER_CRITICAL(sr);

    /* Clear interrupt flag.*/
    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, PIT_TFLG_TIF_MASK);
    os_time_advance(1);

    OS_EXIT_CRITICAL(sr);
}

void os_tick_idle(os_time_t ticks)
{
    OS_ASSERT_CRITICAL();

    __DSB();
    __WFI();
}

void os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    pit_config_t pitConfig;
    uint32_t sr;

    uint32_t ticks_per_ostick = 1000000U / os_ticks_per_sec; /* 1000/s */

    PIT_GetDefaultConfig(&pitConfig);
    pitConfig.enableRunInDebug = true;
    PIT_Init(PIT, &pitConfig);

    /* Clear interrupt flag.*/
    PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, PIT_TFLG_TIF_MASK);

    /* Set timer period for channel 0 */
    PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, USEC_TO_COUNT(ticks_per_ostick, CLOCK_GetFreq(kCLOCK_BusClk)));

    /* Enable timer interrupts for channel 0 */
    PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);

    /* disable interrupts */
    OS_ENTER_CRITICAL(sr);

    /* Set isr in vector table and enable interrupt */
    NVIC_SetPriority(PIT0_IRQn, prio);
    NVIC_SetVector(PIT0_IRQn, (uint32_t)nxp_pit0_timer_handler);
    /* Enable at the NVIC */
    EnableIRQ(PIT0_IRQn);

    /* Start channel 0 */
    PIT_StartTimer(PIT, kPIT_Chnl_0);

    OS_EXIT_CRITICAL(sr);
}
