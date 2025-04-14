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
#include <stdint.h>
#include "mcu/nrf54l_hal.h"
#include "nrfx.h"

static uint8_t nrf54l_clock_hfxo_refcnt;

/**
 * Request HFXO clock be turned on. Note that each request must have a
 * corresponding release.
 *
 * @return int 0: hfxo was already on. 1: hfxo was turned on.
 */
int
nrf54l_clock_hfxo_request(void)
{
    int started;
    uint32_t ctx;

    started = 0;
    __HAL_DISABLE_INTERRUPTS(ctx);
    assert(nrf54l_clock_hfxo_refcnt < 0xff);
    if (nrf54l_clock_hfxo_refcnt == 0) {
        if ((NRF_CLOCK->XO.STAT & CLOCK_XO_STAT_STATE_Msk) !=
            (CLOCK_XO_STAT_STATE_Running << CLOCK_XO_STAT_STATE_Pos)) {
            NRF_CLOCK->EVENTS_XOSTARTED = 0;
            NRF_CLOCK->TASKS_XOSTART = 1;
            while (1) {
                if ((NRF_CLOCK->EVENTS_XOSTARTED != 0)) {
                    break;
                }
            }
        }
        started = 1;
    }
    ++nrf54l_clock_hfxo_refcnt;
    __HAL_ENABLE_INTERRUPTS(ctx);

    return started;
}

/**
 * Release the HFXO. This means that the caller no longer needs the HFXO to be
 * turned on. Each call to release should have been preceeded by a corresponding
 * call to request the HFXO
 *
 *
 * @return int 0: HFXO not stopped by this call (others using it) 1: HFXO
 *         stopped.
 */
int
nrf54l_clock_hfxo_release(void)
{
    int stopped;
    uint32_t ctx;

    stopped = 0;
    __HAL_DISABLE_INTERRUPTS(ctx);
    assert(nrf54l_clock_hfxo_refcnt != 0);
    --nrf54l_clock_hfxo_refcnt;
    if (nrf54l_clock_hfxo_refcnt == 0) {
        NRF_CLOCK_S->TASKS_XOSTOP = 1;
        stopped = 1;
    }
    __HAL_ENABLE_INTERRUPTS(ctx);

    return stopped;
}
