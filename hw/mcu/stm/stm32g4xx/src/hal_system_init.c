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

#include "os/mynewt.h"
#include "mcu/stm32_hal.h"
#include <hal/hal_system.h>

extern char __vector_tbl_reloc__[];

void
hal_system_init(void)
{
    SCB->VTOR = (uint32_t)&__vector_tbl_reloc__;

    if (PREFETCH_ENABLE) {
        __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    }

    if (MYNEWT_VAL(STM32_ENABLE_ICACHE)) {
        __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    }

    if (MYNEWT_VAL(STM32_ENABLE_DCACHE)) {
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }
}

