/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mcu/cortex_m0.h>
#include "hal/hal_system.h"
#include <stdlib.h>

void
hal_system_reset(void)
{
    while (1) {
        if (hal_debugger_connected()) {
            /*
             * If debugger is attached, breakpoint here.
             */
            __asm__("bkpt");
        }

        /* Cortex-M0+ Core Debug Registers (DCB registers, SHCSR, and DFSR)
           are only accessible over DAP and not via processor. Therefore
           they are not covered by the Cortex-M0 header file. */

        NVIC_SystemReset();
    }
}

int
hal_debugger_connected(void)
{
    return DSU->STATUSB.reg & DSU_STATUSB_DBGPRES;
}

uint32_t
HAL_GetTick(void)
{
    return 0;
}

int
HAL_InitTick (uint32_t TickPriority)
{
    return 0;
}
