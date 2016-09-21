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
    [FLASH_AREA_IMAGE_0] = {
        .fa_flash_id = 0,
        .fa_off = 0x00008000,
        .fa_size = (110 * 1024)
    },
    [FLASH_AREA_IMAGE_1] = {
        .fa_flash_id = 0,
        .fa_off = 0x00023800,
        .fa_size = (110 * 1024)
    },
    [FLASH_AREA_IMAGE_SCRATCH] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003f000,
        .fa_size = (2 * 1024)
    },
    [FLASH_AREA_NFFS] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003f800,
        .fa_size = (2 * 1024)
    }
};

void _close(int fd);
void bsp_hal_init(void);

/*
 * Returns the flash map slot where the currently active image is located.
 * If executing from internal flash from fixed location, that slot would
 * be easy to find.
 * If images are in external flash, and copied to RAM for execution, then
 * this routine would have to figure out which one of those slots is being
 * used.
 */
int current_image_slot = 1;

int
bsp_imgr_current_slot(void)
{
    return current_image_slot;
}

void bsp_slot_init_split_application(void) {
    current_image_slot = FLASH_AREA_IMAGE_1;
}

void
bsp_init(void)
{
#ifdef BSP_CFG_SPI_MASTER
    int rc;
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG(0);
#endif
#ifdef BSP_CFG_SPI_SLAVE
    int rc;
    nrf_drv_spis_config_t spi_cfg = NRF_DRV_SPIS_DEFAULT_CONFIG(1);
#endif

    /*
     * XXX this reference is here to keep this function in.
     */
    _sbrk(0);
    _close(0);

    flash_area_init(bsp_flash_areas,
      sizeof(bsp_flash_areas) / sizeof(bsp_flash_areas[0]));

    bsp_hal_init();

#ifdef BSP_CFG_SPI_MASTER
    /*  We initialize one SPI interface as a master. */
    rc = hal_spi_init(0, &spi_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#ifdef BSP_CFG_SPI_SLAVE
    /*  We initialize one SPI interface as a master. */
    spi_cfg.csn_pin = SPIS1_CONFIG_CSN_PIN;
    spi_cfg.csn_pullup = NRF_GPIO_PIN_PULLUP;
    rc = hal_spi_init(1, &spi_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
}
