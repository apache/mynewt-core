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
#include <mcu/stm32_hal.h>
#include "hal/hal_flash_int.h"

int
stm32_mcu_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef erase;
    int rc = -1;
    uint32_t errorPage = -1;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = sector_address;
    erase.NbPages = 1;

    HAL_FLASH_Unlock();
    if (HAL_OK == HAL_FLASHEx_Erase(&erase, &errorPage)) {
      rc = 0;
    }
    HAL_FLASH_Lock();

    return rc;
}
