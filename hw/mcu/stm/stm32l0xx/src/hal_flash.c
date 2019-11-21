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

/* Since the sectors on L0xx are too small, avoid having the map each one
 * of them by aggregating them into PAGES_PER_SECTOR amount.
 */
static_assert(EMULATED_SECTOR_SIZE > FLASH_PAGE_SIZE,
        "EMULATED_SECTOR_SIZE too small!");
static const uint32_t PAGES_PER_SECTOR = EMULATED_SECTOR_SIZE / FLASH_PAGE_SIZE;

int
stm32_mcu_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    uint32_t PageError;
    HAL_StatusTypeDef rc;

    (void)PageError;
    (void)dev;

    if (!(sector_address & (EMULATED_SECTOR_SIZE - 1))) {
        /* FIXME: why is an err flag set? */
        /* Clear status of previous operation. */
        STM32_HAL_FLASH_CLEAR_ERRORS();

        eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
        eraseinit.PageAddress = sector_address;
        eraseinit.NbPages = PAGES_PER_SECTOR;
        rc = HAL_FLASHEx_Erase(&eraseinit, &PageError);
        if (rc == HAL_OK) {
            return 0;
        }
    }

    return -1;
}
