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
#include "hal/hal_flash.h"
#include "hal/hal_system.h"
#include "mcu/nrf52_hal.h"
#include "mcu/nrf52_periph.h"
#include "bsp/bsp.h"

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
        return &nrf52k_flash_dev;
    }
#if MYNEWT_VAL(QSPI_ENABLE)
    if (id == 1) {
        return &nrf52k_qspi_dev;
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
    return (0);
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
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

void
hal_bsp_init(void)
{
    /* Make sure system clocks have started */
    hal_system_clock_start();

    /* Create all available nRF52840 peripherals */
    nrf52_periph_create();
}

void
hal_bsp_deinit(void)
{
}

#if MYNEWT_VAL(BSP_USE_HAL_SPI)
void
bsp_spi_read_buf(uint8_t addr, uint8_t *buf, uint8_t size)
{
    int i;
    uint8_t rxval;
    NRF_SPI_Type *spi;
    spi = NRF_SPI0;

    if (size == 0) {
        return;
    }

    i = -1;
    spi->EVENTS_READY = 0;
    spi->TXD = (uint8_t)addr;
    while (size != 0) {
        spi->TXD = 0;
        while (!spi->EVENTS_READY) {}
        spi->EVENTS_READY = 0;
        rxval = (uint8_t)(spi->RXD);
        if (i >= 0) {
            buf[i] = rxval;
        }
        size -= 1;
        ++i;
        if (size == 0) {
            while (!spi->EVENTS_READY) {}
            spi->EVENTS_READY = 0;
            buf[i] = (uint8_t)(spi->RXD);
        }
    }
}

void
bsp_spi_write_buf(uint8_t addr, uint8_t *buf, uint8_t size)
{
    uint8_t i;
    uint8_t rxval;
    NRF_SPI_Type *spi;

    if (size == 0) {
        return;
    }

    spi = NRF_SPI0;

    spi->EVENTS_READY = 0;

    spi->TXD = (uint8_t)addr;
    for (i = 0; i < size; ++i) {
        spi->TXD = buf[i];
        while (!spi->EVENTS_READY) {}
        rxval = (uint8_t)(spi->RXD);
        spi->EVENTS_READY = 0;
    }

    while (!spi->EVENTS_READY) {}
    rxval = (uint8_t)(spi->RXD);
    spi->EVENTS_READY = 0;
    (void)rxval;
}
#endif
