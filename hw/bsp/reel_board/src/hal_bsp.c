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
#include "os/mynewt.h"
#include "nrfx.h"
#include "flash_map/flash_map.h"
#include "hal/hal_bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_flash.h"
#include "hal/hal_system.h"
#include "mcu/nrf52_hal.h"
#include "mcu/nrf52_periph.h"
#include "bsp/bsp.h"

/* What memory to include in coredump. */
static const struct hal_bsp_mem_dump hal_bsp_core_dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    }
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id == 0) {
        return &nrf52k_flash_dev;
    }

    return NULL;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(hal_bsp_core_dump_cfg) / sizeof(hal_bsp_core_dump_cfg[0]);
    return hal_bsp_core_dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return 0;
}

uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    uint32_t cfg_pri;

    switch (irq_num) {
    case RADIO_IRQn:
        /* Radio gets highest priority */
        cfg_pri = 0;
        break;
    default:
        cfg_pri = pri;
    }
    return cfg_pri;
}

void
hal_bsp_init(void)
{
    int rc;

    /* Make sure system clocks have started */
    hal_system_clock_start();

    /* Create all available nRF52840 peripherals */
    nrf52_periph_create();

    if (MYNEWT_VAL(REEL_BOARD_ENABLE_ACTIVE_MODE)) {
        rc = hal_gpio_init_out(32, 1);
        assert(rc == 0);
    }
}
