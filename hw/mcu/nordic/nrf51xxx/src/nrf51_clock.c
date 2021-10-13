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
#include "mcu/nrf51_hal.h"
#include "nrfx.h"

static uint8_t nrf51_clock_hfxo_refcnt;

/**
 * Request HFXO clock be turned on. Note that each request must have a
 * corresponding release.
 *
 * @return int 0: hfxo was already on. 1: hfxo was turned on.
 */
int
nrf51_clock_hfxo_request(void)
{
    int started;
    uint32_t ctx;

#if MYNEWT_VAL_CHOICE(MCU_HFCLK_SOURCE, HFINT)
    /* Cannot enable/disable hfxo if it is not present */
    assert(0);
#endif

    started = 0;
    __HAL_DISABLE_INTERRUPTS(ctx);
    assert(nrf51_clock_hfxo_refcnt < 0xff);
    if (nrf51_clock_hfxo_refcnt == 0) {
        /* Check the current STATE and SRC of HFCLK */
        if ((NRF_CLOCK->HFCLKSTAT &
             (CLOCK_HFCLKSTAT_SRC_Msk | CLOCK_HFCLKSTAT_STATE_Msk)) !=
            (CLOCK_HFCLKSTAT_SRC_Xtal << CLOCK_HFCLKSTAT_SRC_Pos |
             CLOCK_HFCLKSTAT_STATE_Running << CLOCK_HFCLKSTAT_STATE_Pos)) {
            NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
            NRF_CLOCK->TASKS_HFCLKSTART = 1;
            while (!NRF_CLOCK->EVENTS_HFCLKSTARTED) {
            }
        }
        started = 1;
    }
    ++nrf51_clock_hfxo_refcnt;
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
nrf51_clock_hfxo_release(void)
{
    int stopped;
    uint32_t ctx;

    stopped = 0;
    __HAL_DISABLE_INTERRUPTS(ctx);
    assert(nrf51_clock_hfxo_refcnt != 0);
    --nrf51_clock_hfxo_refcnt;
    if (nrf51_clock_hfxo_refcnt == 0) {
        NRF_CLOCK->TASKS_HFCLKSTOP = 1;
        stopped = 1;
    }
    __HAL_ENABLE_INTERRUPTS(ctx);

    return stopped;
}
