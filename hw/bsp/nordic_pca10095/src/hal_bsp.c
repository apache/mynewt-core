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

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <os/mynewt.h>
#include <nrfx.h>
#include <flash_map/flash_map.h>
#include <hal/hal_bsp.h>
#include <hal/hal_flash.h>
#include <hal/hal_system.h>
#include <mcu/nrf5340_hal.h>
#include <mcu/nrf5340_periph.h>
#include <bsp/bsp.h>

/*
 * What memory to include in coredump.
 */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    }
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id == 0) {
        return &nrf5340_flash_dev;
    }
#if MYNEWT_VAL(QSPI_ENABLE)
    if (id == 1) {
        return &nrf5340_qspi_dev;
    }
#endif
    return NULL;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return 0;
}

void
hal_bsp_init(void)
{
    /* Make sure system clocks have started */
    hal_system_clock_start();

    /* Create all available nRF5340 peripherals */
    nrf5340_periph_create();
}

void
hal_bsp_deinit(void)
{
}
