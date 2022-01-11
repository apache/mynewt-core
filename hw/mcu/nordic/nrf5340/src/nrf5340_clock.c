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
#include "mcu/nrf5340_hal.h"
#include "nrfx.h"
#include "nrf_clock.h"

static uint8_t nrf5340_clock_hfxo_refcnt;
static uint8_t nrf5340_clock_hfclk192m_refcnt;

int
nrf5340_clock_hfxo_request(void)
{
    int started;
    uint32_t ctx;

    started = 0;
    __HAL_DISABLE_INTERRUPTS(ctx);
    assert(nrf5340_clock_hfxo_refcnt < 0xff);
    if (nrf5340_clock_hfxo_refcnt == 0) {
        nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTART);
        started = 1;
    }
    ++nrf5340_clock_hfxo_refcnt;
    __HAL_ENABLE_INTERRUPTS(ctx);

    return started;
}

int
nrf5340_clock_hfxo_release(void)
{
    int stopped;
    uint32_t ctx;

    stopped = 0;
    __HAL_DISABLE_INTERRUPTS(ctx);
    assert(nrf5340_clock_hfxo_refcnt != 0);
    --nrf5340_clock_hfxo_refcnt;
    if (nrf5340_clock_hfxo_refcnt == 0) {
        nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
        stopped = 1;
    }
    __HAL_ENABLE_INTERRUPTS(ctx);

    return stopped;
}

int
nrf5340_clock_hfclk192m_request(void)
{
    int started;
    uint32_t ctx;

    started = 0;
    __HAL_DISABLE_INTERRUPTS(ctx);
    assert(nrf5340_clock_hfclk192m_refcnt < 0xff);
    if (nrf5340_clock_hfclk192m_refcnt == 0) {
        nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLK192MSTART);
        started = 1;
    }
    ++nrf5340_clock_hfclk192m_refcnt;
    __HAL_ENABLE_INTERRUPTS(ctx);

    return started;
}

int
nrf5340_clock_hfclk192m_release(void)
{
    int stopped;
    uint32_t ctx;

    stopped = 0;
    __HAL_DISABLE_INTERRUPTS(ctx);
    assert(nrf5340_clock_hfclk192m_refcnt != 0);
    --nrf5340_clock_hfclk192m_refcnt;
    if (nrf5340_clock_hfclk192m_refcnt == 0) {
        nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLK192MSTOP);
        stopped = 1;
    }
    __HAL_ENABLE_INTERRUPTS(ctx);

    return stopped;
}
