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

#include "hal/hal_system.h"
#include "fsl_power.h"

enum hal_reset_reason
hal_reset_cause(void)
{
    static enum hal_reset_reason reason;
    power_device_reset_cause_t reset_cause;
    power_device_boot_mode_t boot_mode;
    uint32_t wakeupio_cause;

    if (reason) {
        return reason;
    }
    POWER_GetWakeUpCause(&reset_cause, &boot_mode, &wakeupio_cause);

    switch (reset_cause) {
    case kRESET_CAUSE_POR:
        reason = HAL_RESET_POR;
        break;
    case kRESET_CAUSE_PADRESET:
        reason = HAL_RESET_PIN;
        break;
    case kRESET_CAUSE_BODRESET:
        reason = HAL_RESET_BROWNOUT;
        break;
    case kRESET_CAUSE_ARMSYSTEMRESET:
    case kRESET_CAUSE_SWRRESET:
        reason = HAL_RESET_SOFT;
        break;
    case kRESET_CAUSE_WDTRESET:
    case kRESET_CAUSE_CDOGRESET:
        reason = HAL_RESET_WATCHDOG;
        break;
    /* Reset causes in DEEP-POWER-DOWN low power mode */
    case kRESET_CAUSE_DPDRESET_WAKEUPIO:
    case kRESET_CAUSE_DPDRESET_RTC:
    case kRESET_CAUSE_DPDRESET_OSTIMER:
    case kRESET_CAUSE_DPDRESET_WAKEUPIO_RTC:
    case kRESET_CAUSE_DPDRESET_WAKEUPIO_OSTIMER:
    case kRESET_CAUSE_DPDRESET_RTC_OSTIMER:
    case kRESET_CAUSE_DPDRESET_WAKEUPIO_RTC_OSTIMER:
        reason = HAL_RESET_SYS_OFF_INT;
        break;
    case kRESET_CAUSE_NOT_RELEVANT:
    case kRESET_CAUSE_NOT_DETERMINISTIC:
        reason = HAL_RESET_OTHER;
    }

    return reason;
}
