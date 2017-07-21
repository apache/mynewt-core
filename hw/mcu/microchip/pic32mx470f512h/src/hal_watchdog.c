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

#include "hal/hal_watchdog.h"
#include <xc.h>

int
hal_watchdog_init(uint32_t expire_msecs)
{
    /*
     * Cannot change watchdog prescaler at runtime.
     * Only check if the watchdog timer is greater than expire_msecs.
     */
    uint32_t wdt_period = 1;
    wdt_period <<= (WDTCON & _WDTCON_SWDTPS_MASK) >> _WDTCON_SWDTPS_POSITION;

    return wdt_period < expire_msecs ? -1 : 0;
}

void
hal_watchdog_enable(void)
{
    WDTCONSET = _WDTCON_ON_MASK;
}

void
hal_watchdog_tickle(void)
{
    WDTCONSET = _WDTCON_WDTCLR_MASK;
}

