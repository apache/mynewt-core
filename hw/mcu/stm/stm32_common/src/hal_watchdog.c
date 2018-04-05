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

#include "hal/hal_watchdog.h"
#include "mcu/stm32_hal.h"

IWDG_HandleTypeDef g_wdt_cfg;

int
hal_watchdog_init(uint32_t expire_msecs)
{
    uint32_t reload;

    /* Max prescaler is 256 */
    reload = 32768 / 256;
    reload = (reload * expire_msecs) / 1000;

    /* Check to make sure we are not trying a reload value that is too large */
    if (reload > IWDG_RLR_RL) {
        return -1;
    }

    g_wdt_cfg.Instance = IWDG;
    g_wdt_cfg.Init.Prescaler = IWDG_PRESCALER_256;
    g_wdt_cfg.Init.Reload = reload;
    STM32_HAL_WATCHDOG_CUSTOM_INIT(&g_wdt_cfg);

    return 0;
}

void
hal_watchdog_enable(void)
{
    __HAL_DBGMCU_FREEZE_IWDG();
    HAL_IWDG_Init(&g_wdt_cfg);
}

void
hal_watchdog_tickle(void)
{
    HAL_IWDG_Refresh(&g_wdt_cfg);
}
