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
#include "hal/hal_system.h"
#include "mcu/cmsis_nvic.h"

void
hal_system_init(void)
{
}

void
hal_system_reset(void)
{
    /*
     * Do not reset here since it will make M0 and M33 out of sync. Instead just
     * signal FW error and let M33 do it.
     */

    __disable_irq();

    if (hal_debugger_connected()) {
        asm("bkpt");
    }

    CMAC->CM_EXC_STAT_REG = CMAC_CM_EXC_STAT_REG_EXC_FW_ERROR_Msk;

    for (;;);
}

int
hal_debugger_connected(void)
{
    return CMAC->CM_STAT_REG & CMAC_CM_STAT_REG_SWD_ATTACHED_Msk;
}

void
hal_system_clock_start(void)
{
}
