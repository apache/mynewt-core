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

#include <syscfg/syscfg.h>
#include <mcu/stm32_hal.h>
#include "hal/hal_flash_int.h"

int
stm32_mcu_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    uint32_t PageError;
    uint32_t offset;
    HAL_StatusTypeDef rc;

    (void)PageError;

    if (!(sector_address & (FLASH_PAGE_SIZE - 1))) {
        offset = sector_address - dev->hf_base_addr;
        eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
        eraseinit.Banks = FLASH_BANK_1;
        eraseinit.Page = offset / FLASH_PAGE_SIZE;
#ifdef FLASH_DBANK_SUPPORT
        /*
         * Only some devices (or option byte DUAL_BANK) have two banks,
         * pages in bank 2 are numbered from 256.
         */
        if (FLASH_BANK_NB == 2 && offset >= FLASH_BANK_SIZE) {
            eraseinit.Banks = FLASH_BANK_2;
            eraseinit.Page = 256 + (offset - FLASH_BANK_SIZE) / FLASH_PAGE_SIZE;
        }
#endif
        eraseinit.NbPages = 1;
        rc = HAL_FLASHEx_Erase(&eraseinit, &PageError);
        if (rc == HAL_OK) {
            return 0;
        }
    }

    return -1;
}
