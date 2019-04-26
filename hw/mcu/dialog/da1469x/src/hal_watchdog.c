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
#include "hal/hal_watchdog.h"
#include "DA1469xAB.h"

static uint32_t g_hal_watchdog_reload_val;

int
hal_watchdog_init(uint32_t expire_msecs)
{
    SYS_WDOG->WATCHDOG_CTRL_REG = SYS_WDOG_WATCHDOG_CTRL_REG_WDOG_FREEZE_EN_Msk;
    GPREG->SET_FREEZE_REG |= GPREG_SET_FREEZE_REG_FRZ_SYS_WDOG_Msk;

    g_hal_watchdog_reload_val = expire_msecs / 10;
    assert((g_hal_watchdog_reload_val & 0xFFFFC000) == 0);

    while (SYS_WDOG->WATCHDOG_CTRL_REG & SYS_WDOG_WATCHDOG_CTRL_REG_WRITE_BUSY_Msk) {
        /* wait */
    }
    SYS_WDOG->WATCHDOG_REG = g_hal_watchdog_reload_val;
    
    return 0;
}

void
hal_watchdog_enable(void)
{
    GPREG->RESET_FREEZE_REG |= GPREG_RESET_FREEZE_REG_FRZ_SYS_WDOG_Msk;
}

void
hal_watchdog_tickle(void)
{
    while (SYS_WDOG->WATCHDOG_CTRL_REG & SYS_WDOG_WATCHDOG_CTRL_REG_WRITE_BUSY_Msk) {
        /* wait */
    }
    SYS_WDOG->WATCHDOG_REG = g_hal_watchdog_reload_val;
}
