/**
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
#include <hal/flash_map.h>
#include <hal/hal_bsp.h>
#include <hal/hal_cputime.h>
#include <mcu/nrf52_hal.h>

#include <os/os_dev.h>

#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#include <hal/hal_spi.h>
#ifdef BSP_CFG_SPI_MASTER
#include "nrf_drv_spi.h"
#endif
#ifdef BSP_CFG_SPI_SLAVE
#include "nrf_drv_spis.h"
#endif
#include "nrf_drv_config.h"
#include <app_util_platform.h>

static struct flash_area bsp_flash_areas[] = {
    [FLASH_AREA_BOOTLOADER] = {
        .fa_flash_id = 0,       /* internal flash */
        .fa_off = 0x00000000,   /* beginning */
        .fa_size = (32 * 1024)
    },
    /* 2*16K and 1*64K sectors here */
    [FLASH_AREA_IMAGE_0] = {
        .fa_flash_id = 0,
        .fa_off = 0x00008000,
        .fa_size = (232 * 1024)
    },
    [FLASH_AREA_IMAGE_1] = {
        .fa_flash_id = 0,
        .fa_off = 0x00042000,
        .fa_size = (232 * 1024)
    },
    [FLASH_AREA_IMAGE_SCRATCH] = {
        .fa_flash_id = 0,
        .fa_off = 0x0007c000,
        .fa_size = (4 * 1024)
    },
    [FLASH_AREA_NFFS] = {
        .fa_flash_id = 0,
        .fa_off = 0x0007d000,
        .fa_size = (12 * 1024)
    }
};
static struct uart_dev hal_uart0;

void _close(int fd);

/*
 * Returns the flash map slot where the currently active image is located.
 * If executing from internal flash from fixed location, that slot would
 * be easy to find.
 * If images are in external flash, and copied to RAM for execution, then
 * this routine would have to figure out which one of those slots is being
 * used.
 */
int
bsp_imgr_current_slot(void)
{
    return FLASH_AREA_IMAGE_0;
}

void
bsp_init(void)
{
    int rc;
#ifdef BSP_CFG_SPI_MASTER
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG(0);
#endif
#ifdef BSP_CFG_SPI_SLAVE
    nrf_drv_spis_config_t spi_cfg = NRF_DRV_SPIS_DEFAULT_CONFIG(0);
#endif

    /*
     * XXX this reference is here to keep this function in.
     */
    _sbrk(0);
    _close(0);

    flash_area_init(bsp_flash_areas,
      sizeof(bsp_flash_areas) / sizeof(bsp_flash_areas[0]));

    rc = os_dev_create((struct os_dev *) &hal_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)bsp_uart_config());
    assert(rc == 0);

#ifdef BSP_CFG_SPI_MASTER
    /*  We initialize one SPI interface as a master. */
    rc = hal_spi_init(0, &spi_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#ifdef BSP_CFG_SPI_SLAVE
    /*  We initialize one SPI interface as a master. */
    spi_cfg.csn_pin = SPIS0_CONFIG_CSN_PIN;
    rc = hal_spi_init(0, &spi_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
}
