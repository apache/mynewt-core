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
#include "fsl_clock.h"

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
    uint32_t sr;

    /* Disable interrupts */
    OS_ENTER_CRITICAL(sr);

    /* Disable SysTick timer */
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    /* Initialize Reload value */
    SysTick->LOAD = SystemCoreClock / os_ticks_per_sec;
    SysTick->VAL = 0UL;
    /* Set clock source to processor clock */
    SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
    /* Enable SysTick IRQ */
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;

    /* Set isr in vector table and enable interrupt */
    NVIC_SetPriority(SysTick_IRQn, prio);
    /* Enable at the NVIC */
    EnableIRQ(SysTick_IRQn);

    /* Enable SysTick timer */
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

    OS_EXIT_CRITICAL(sr);
}
