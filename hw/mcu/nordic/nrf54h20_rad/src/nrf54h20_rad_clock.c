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
#include "mcu/nrf54h20_rad_hal.h"
#include <nrfx.h>
#include <nrfx_grtc.h>

static uint8_t nrf54h20_rad_clock_hfxo_refcnt;

/**
 * Request HFXO clock be turned on. Note that each request must have a
 * corresponding release.
 *
 * @return int 0: hfxo was already on. 1: hfxo was turned on.
 */
int
nrf54h20_rad_clock_hfxo_request(void)
{
    int started;
    uint32_t ctx;

    // started = 0;
    // __HAL_DISABLE_INTERRUPTS(ctx);
    // assert(nrf54h20_rad_clock_hfxo_refcnt < 0xff);
    // if (nrf54h20_rad_clock_hfxo_refcnt == 0) {
    //     NRF_CLOCK_NS->TASKS_HFCLKSTART = 1;
    //     started = 1;
    // }
    // ++nrf54h20_rad_clock_hfxo_refcnt;
    // __HAL_ENABLE_INTERRUPTS(ctx);

    return 0;
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
nrf54h20_rad_clock_hfxo_release(void)
{
    int stopped;
    uint32_t ctx;

    // stopped = 0;
    // __HAL_DISABLE_INTERRUPTS(ctx);
    // assert(nrf54h20_rad_clock_hfxo_refcnt != 0);
    // --nrf54h20_rad_clock_hfxo_refcnt;
    // if (nrf54h20_rad_clock_hfxo_refcnt == 0) {
    //     NRF_CLOCK_NS->TASKS_HFCLKSTOP = 1;
    //     stopped = 1;
    // }
    // __HAL_ENABLE_INTERRUPTS(ctx);

    return 0;
}

static void sys_clock_timeout_handler(int32_t id, uint64_t cc_val, void *p_context) {
}

static uint32_t int_mask;
static nrfx_grtc_channel_t system_clock_channel_data = {
        .handler = sys_clock_timeout_handler,
        .p_context = NULL,
        .channel = (uint8_t)-1,
};

int
nrf54h20_rad_clock_grtc_init(void)
{
    nrfx_err_t err_code;

    err_code = nrfx_grtc_init(0);
    if (err_code != NRFX_SUCCESS) {
        return -1;
    }

//#if defined(CONFIG_NRF_GRTC_START_SYSCOUNTER)
//    err_code = nrfx_grtc_syscounter_start(true, &system_clock_channel_data.channel);
//	if (err_code != NRFX_SUCCESS) {
//		return err_code == NRFX_ERROR_NO_MEM ? -ENOMEM : -EPERM;
//	}
//#else
    err_code = nrfx_grtc_channel_alloc(&system_clock_channel_data.channel);
    if (err_code != NRFX_SUCCESS) {
        return -1;
    }
//#endif /* CONFIG_NRF_GRTC_START_SYSCOUNTER */

    int_mask = NRFX_GRTC_CONFIG_ALLOWED_CC_CHANNELS_MASK;

    return 0;
}
