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

#include <inttypes.h>
#include <assert.h>
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "hal/hal_system.h"
#include "hal/hal_flash_int.h"
#include "hal/hal_timer.h"
#include "hal/hal_bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "mcu/nrf52_hal.h"
#include "mcu/nrf52_periph.h"
#include "sgm4056/sgm4056.h"

/** What memory to include in coredump. */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE,
    }
};

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

/**
 * Retrieves the flash device with the specified ID.  Returns NULL if no such
 * device exists.
 */
const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    switch (id) {
    case 0:
        /* MCU internal flash. */
        return &nrf52k_flash_dev;

    default:
        /* External flash.  Assume not present in this BSP. */
        return NULL;
    }
}

/**
 * Retrieves the configured priority for the given interrupt. If no priority
 * is configured, returns the priority passed in.
 *
 * @param irq_num               The IRQ being queried.
 * @param pri                   The default priority if none is configured.
 *
 * @return uint32_t             The specified IRQ's priority.
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    uint32_t cfg_pri;

    switch (irq_num) {
    /* Radio gets highest priority */
    case RADIO_IRQn:
        cfg_pri = 0;
        break;
    default:
        cfg_pri = pri;
    }
    return cfg_pri;
}

static struct sgm4056_dev os_bsp_charger;
static struct sgm4056_dev_config os_bsp_charger_config = {
    .power_presence_pin = CHARGER_POWER_PRESENCE_PIN,
    .charge_indicator_pin = CHARGER_CHARGE_PIN,
};

void
hal_bsp_init(void)
{
    int rc;

    /* Make sure system clocks have started. */
    hal_system_clock_start();

    /* Create all available nRF52840 peripherals */
    nrf52_periph_create();

    /* Create charge controller */
    rc = os_dev_create(&os_bsp_charger.dev, "charger",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       sgm4056_dev_init, &os_bsp_charger_config);
    assert(rc == 0);
}
