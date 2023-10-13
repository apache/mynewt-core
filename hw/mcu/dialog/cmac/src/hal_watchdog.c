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
#include "syscfg/syscfg.h"
#include "hal/hal_watchdog.h"
#include "CMAC.h"

int
hal_watchdog_init(uint32_t expire_msecs)
{
    return 0;
}

void
hal_watchdog_enable(void)
{
    hal_watchdog_tickle();
}

void
hal_watchdog_disable(void)
{
    GPREG->SET_FREEZE_REG |= GPREG_SET_FREEZE_REG_FRZ_CMAC_WDOG_Msk;
    return;
}

void
hal_watchdog_tickle(void)
{
    uint32_t cnt;

    cnt = MYNEWT_VAL(WATCHDOG_INTERVAL) * 100 / 1024;
    assert(cnt <= CMAC_CM_WDOG_REG_CM_WDOG_CNT_Msk);

    CMAC->CM_WDOG_REG = CMAC_CM_WDOG_REG_SYS2CMAC_WDOG_FREEZE_DIS_Msk |
                        CMAC_CM_WDOG_REG_CM_WDOG_WRITE_VALID_Msk | cnt;
}
