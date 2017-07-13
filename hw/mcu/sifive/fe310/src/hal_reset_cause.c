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

#include <hal/hal_system.h>
#include <env/freedom-e300-hifive1/platform.h>

enum hal_reset_reason
hal_reset_cause(void)
{
    enum hal_reset_reason reason;

    switch (AON_REG(AON_PMUCAUSE) & (0x300)) {
    default:
    case AON_RESETCAUSE_POWERON:
        reason = HAL_RESET_POR;
        break;
    case AON_RESETCAUSE_EXTERNAL:
        reason = HAL_RESET_PIN;
        break;
    case AON_RESETCAUSE_WATCHDOG:
        reason = HAL_RESET_WATCHDOG;
        break;
    }
    return reason;
}
