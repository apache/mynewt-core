/**
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

#include <stdint.h>
#include "os/mynewt.h"
#include "mcu/cortex_m4.h"
#include "hal/hal_system.h"
#include "am_mcu_apollo.h"

void
hal_system_init(void)
{
    NVIC_Relocate();
}

void
hal_system_reset(void)
{

#if MYNEWT_VAL(HAL_SYSTEM_RESET_CB)
    hal_system_reset_cb();
#endif

    while (1) {
        HAL_DEBUG_BREAK();
        NVIC_SystemReset();
    }
}

enum hal_reset_reason
hal_reset_cause(void)
{
    static enum hal_reset_reason reason;
    uint32_t reg;

    if (reason) {
        return reason;
    }

    /* Read STAT register */
    reg = RSTGEN->STAT;

    if (reg & RSTGEN_STAT_PORSTAT_Msk) {
        reason = HAL_RESET_POR;
    } else if (reg & RSTGEN_STAT_WDRSTAT_Msk) {
        reason = HAL_RESET_WATCHDOG;
    } else if (reg & RSTGEN_STAT_SWRSTAT_Msk) {
        reason = HAL_RESET_SOFT;
    } else if (reg & RSTGEN_STAT_EXRSTAT_Msk) {
        reason = HAL_RESET_PIN;
    } else if (reg & RSTGEN_STAT_BORSTAT_Msk) {
        reason = HAL_RESET_BROWNOUT;
    } else {
        reason = HAL_RESET_OTHER;
    }

    return reason;
}

int
hal_debugger_connected(void)
{
    return CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
}
