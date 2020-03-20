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
#include <mcu/stm32_hal.h>
#include <mcu/mcu.h>

void
stm32_start_bootloader(void)
{
    const uint32_t *system_memory = (const uint32_t *)STM32_SYSTEM_MEMORY;
    void (*system_memory_reset_handler)(void);

    HAL_RCC_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
#ifdef __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
#endif

    __set_MSP(system_memory[0]);

    system_memory_reset_handler = (void (*)(void))(system_memory[1]);
    system_memory_reset_handler();
}
