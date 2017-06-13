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

#include <xc.h>
#include "hal/hal_system.h"

enum hal_reset_reason
hal_reset_cause(void)
{
    static enum hal_reset_reason reason;

    if (reason) {
        return reason;
    }

    if (RCON & _RCON_WDTO_MASK) {
        reason = HAL_RESET_WATCHDOG;
    } else if (RCON & _RCON_SWR_MASK) {
        reason = HAL_RESET_SOFT;
    } else if (RCON & _RCON_EXTR_MASK) {
        reason = HAL_RESET_PIN;
    } else if (RCON & _RCON_POR_MASK) {
        reason = HAL_RESET_POR;
    } else if (RCON & _RCON_BOR_MASK) {
        reason = HAL_RESET_BROWNOUT;
    }

    RCONCLR = _RCON_EXTR_MASK | _RCON_SWR_MASK | _RCON_WDTO_MASK |
              _RCON_BOR_MASK | _RCON_POR_MASK;

    return reason;
}