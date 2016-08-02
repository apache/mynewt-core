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
#include "hal/hal_bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_flash_int.h"
#include "mcu/stm32f407xx.h"
#include "mcu/stm32f4xx_hal_gpio_ex.h"
#include "mcu/stm32f4_bsp.h"
#include "bsp/bsp.h"
#include <assert.h>

static const struct stm32f4_uart_cfg uart_cfg[UART_CNT] = {
    [0] = {
        .suc_uart = USART6,
        .suc_rcc_reg = &RCC->APB2ENR,
        .suc_rcc_dev = RCC_APB2ENR_USART6EN,
        .suc_pin_tx = 38,
        .suc_pin_rx = 39,
        .suc_pin_rts = 34,
        .suc_pin_cts = 35,
        .suc_pin_af = GPIO_AF8_USART6,
        .suc_irqn = USART6_IRQn
    }
};

static const struct bsp_mem_dump dump_cfg[] = {
    [0] = {
        .bmd_start = &_ram_start,
        .bmd_size = RAM_SIZE
    },
    [1] = {
        .bmd_start = &_ccram_start,
        .bmd_size = CCRAM_SIZE
    }
};

const struct stm32f4_uart_cfg *
bsp_uart_config(int port)
{
    assert(port < UART_CNT);
    return &uart_cfg[port];
}

const struct hal_flash *
bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &stm32f4_flash_dev;
}

const struct bsp_mem_dump *
bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}
