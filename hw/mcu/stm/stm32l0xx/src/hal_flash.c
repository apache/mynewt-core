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

#include <string.h>
#include <assert.h>
#include <syscfg/syscfg.h>
#include <mcu/stm32_hal.h>
#include "hal/hal_flash_int.h"
#include "hal/hal_watchdog.h"

/*
 * NOTE: the actual page size if 128 bytes, but that would mean an
 * enormous amount of pages, so here make it look like 1K.
 */
#define _REAL_SECTOR_SIZE    128
#define _FLASH_SECTOR_SIZE   MYNEWT_VAL(STM32_FLASH_SECTOR_SIZE)
#define _PAGES_PER_SECTOR    8

/* Since the sectors on L0 are too small, avoid having the map each one
 * of them by aggregating them into _PAGES_PER_SECTOR amount.
 *
 * This asserts the MCU constant is not changed, and if it's changed
 * intentionally alerts the user that this code needs to be updated
 * as well.
 */
static_assert(_FLASH_SECTOR_SIZE == (_REAL_SECTOR_SIZE * _PAGES_PER_SECTOR),
    "STM32_FLASH_SECTOR_SIZE was changed; the erase function needs updating!");

int
stm32_mcu_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    uint32_t PageError;
    HAL_StatusTypeDef rc;

    (void)PageError;
    (void)dev;

    if (!(sector_address & (_FLASH_SECTOR_SIZE - 1))) {
        /* FIXME: why is an err flag set? */
        /* Clear status of previous operation. */
        STM32_HAL_FLASH_CLEAR_ERRORS();

        eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
        eraseinit.PageAddress = sector_address;
        eraseinit.NbPages = _PAGES_PER_SECTOR;
        rc = HAL_FLASHEx_Erase(&eraseinit, &PageError);
        if (rc == HAL_OK) {
            return 0;
        }
    }

    return -1;
}
