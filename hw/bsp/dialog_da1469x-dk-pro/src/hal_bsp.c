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
#include <string.h>
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_system.h"
#include "mcu/mcu.h"
#include "mcu/da1469x_hal.h"
#include "mcu/da1469x_periph.h"
#include "bsp/bsp.h"

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = (void *)MCU_MEM_SYSRAM_START_ADDRESS,
        .hbmd_size = MCU_MEM_SYSRAM_END_ADDRESS - MCU_MEM_SYSRAM_START_ADDRESS,
    }
};

/* Note: This is just dummy implementation.
 * There is no special register for hardware id.
 * Most probably it should be generated and stored somewhere in OTP.
 */
static char hw_id[] = "DA1469X_HW_ID";

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id != 0) {
        return NULL;
    }

    return &da1469x_flash_dev;
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

int
hal_bsp_hw_id_len(void)
{
    return sizeof(hw_id);
}

int
hal_bsp_hw_id(uint8_t *id, int max_len)
{
    int len = sizeof(hw_id);

    assert(max_len > len);
    memcpy(id, hw_id, len);

    return len;
}

void
hal_bsp_init(void)
{
    /* Make sure system clocks have started */
    hal_system_clock_start();

    /* Create all available DA1649x peripherals */
    da1469x_periph_create();
}

