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

#include <assert.h>

#include "app_util_platform.h"
#include "nrf.h"
#include "nrf_wdt.h"
#include "nrf_drv_wdt.h"

static void
nrf51_hal_wdt_default_handler(void)
{
    assert(0);
}

int
hal_watchdog_init(int expire_secs)
{
    nrf_drv_wdt_config_t cfg;
    int rc;

    cfg.behaviour = NRF_WDT_BEHAVIOUR_RUN_SLEEP;
    cfg.interrupt_priority = WDT_CONFIG_IRQ_PRIORITY;
    cfg.reload_value = (uint32_t) expire_secs;

    rc = nrf_drv_wdt_init(&cfg, nrf51_hal_wdt_default_handler);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

void
hal_watchdog_enable(void)
{
    nrf_drv_wdt_enable();
}

void
hal_watchdog_tickle(void)
{
    nrf_drv_wdt_feed();
}

