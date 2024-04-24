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

#define STM32_FLASH_SIZE      (MYNEWT_VAL(STM32_FLASH_SIZE_KB) * 1024)

int
stm32_mcu_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    uint32_t PageError;
    HAL_StatusTypeDef rc;

    (void)PageError;

    if (!(sector_address & (FLASH_SECTOR_SIZE - 1))) {
        eraseinit.TypeErase = FLASH_TYPEERASE_SECTORS;
#ifdef FLASH_BANK_2
        if ((sector_address - dev->hf_base_addr) < (STM32_FLASH_SIZE / 2)) {
            eraseinit.Banks = FLASH_BANK_1;
        } else {
            eraseinit.Banks = FLASH_BANK_2;
        }
#else
        eraseinit.Banks = FLASH_BANK_1;
#endif
        eraseinit.Sector = (sector_address - dev->hf_base_addr) / FLASH_SECTOR_SIZE;
        eraseinit.NbSectors = 1;
#if defined(FLASH_CR_PSIZE)
        eraseinit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
#endif
        rc = HAL_FLASHEx_Erase(&eraseinit, &PageError);
        if (rc == HAL_OK) {
            return 0;
        }
    }

    return -1;
}
