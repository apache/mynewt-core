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
#include <hal/hal_flash_int.h>
#include <hal/hal_system.h>
#include <mcu/nrf54h20_rad_hal.h>
#include <mcu/nrf54h20_rad_periph.h>
#include <bsp/bsp.h>

/*
 * What memory to include in coredump.
 */
#if !MYNEWT_VAL(COREDUMP_SKIP_UNUSED_HEAP)
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    }
};
#else
static struct hal_bsp_mem_dump dump_cfg[2];
extern uint8_t __StackLimit;
extern uint8_t __StackTop;
#endif

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
//    if (id == 0) {
//        return &nrf54h20_flash_dev;
//    }
//#if MCUBOOT_MYNEWT
//    if (id == 1) {
//        return &nrf54h20_rad_vflash_dev.nv_flash;
//    }
//#endif
//#if MYNEWT_VAL(IPC_NRF54H20_FLASH_CLIENT)
//    if (id == 2) {
//        return ipc_flash();
//    }
//#endif

    return NULL;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
#if MYNEWT_VAL(COREDUMP_SKIP_UNUSED_HEAP)
    /* Interrupt stack first */
    dump_cfg[0].hbmd_start = &__StackLimit;
    dump_cfg[0].hbmd_size = &__StackTop - &__StackLimit;
    /* RAM from _ram_start to end of used heap */
    dump_cfg[1].hbmd_start = &_ram_start;
    dump_cfg[1].hbmd_size = (uint8_t *)_sbrk(0) - &_ram_start;
#endif

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

    /* Create all available nRF54H20 Net Core peripherals */
    nrf54h20_rad_periph_create();
}

void
hal_bsp_deinit(void)
{
}
