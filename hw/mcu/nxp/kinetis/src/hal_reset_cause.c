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
#include "syscfg/syscfg.h"
#include <fsl_device_registers.h>
#include "fsl_rcm.h"

enum hal_reset_reason
hal_reset_cause(void)
{
    static enum hal_reset_reason reason;
    uint32_t reg;

    if (reason) {
        return reason;
    }
    reg = (uint32_t) RCM;

    if (reg & (kRCM_SourceWdog | kRCM_SourceLockup)) {
        reason = HAL_RESET_WATCHDOG;
    } else if (reg & kRCM_SourceSw) {
        reason = HAL_RESET_SOFT;
    } else if (reg & kRCM_SourcePin) {
        reason = HAL_RESET_PIN;
    } else if (reg & kRCM_SourcePor) {
        reason = HAL_RESET_POR;
    } else if (reg & kRCM_SourceWakeup) {
        reason = HAL_RESET_SYS_OFF_INT;
    } else if (reg & kRCM_SourceLvd) {
        reason = HAL_RESET_BROWNOUT;
    } else {
        reason = HAL_RESET_OTHER;
    }

    return reason;
}
