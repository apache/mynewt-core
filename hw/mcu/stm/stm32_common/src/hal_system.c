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

#include "mcu/stm32_hal.h"
#include "os/mynewt.h"
#include "hal/hal_system.h"

void
hal_system_reset(void)
{
    while (1) {
        if (hal_debugger_connected()) {
            /*
             * If debugger is attached, breakpoint here.
             */
            asm("bkpt");
        }
        NVIC_SystemReset();
    }
}

int
hal_debugger_connected(void)
{
    return CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
}

uint32_t
HAL_GetTick(void)
{
    return os_time_get();
}

HAL_StatusTypeDef
HAL_InitTick (uint32_t TickPriority)
{
    return HAL_OK;
}
