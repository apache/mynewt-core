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
#include <stddef.h>

#include "hal/hal_bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_flash_int.h"
#include "hal/hal_system.h"
#include "stm32f3xx.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_gpio.h"
#include "mcu/stm32f3_bsp.h"
#include "bsp/bsp.h"
#include <syscfg/syscfg.h>

#if 0
static const struct stm32f3_uart_cfg uart_cfg[UART_CNT] = {
    [0] = {
        .suc_uart = USART2,
        .suc_rcc_cmd = RCC_APB2PeriphClockCmd,
        .suc_rcc_dev = RCC_APB2Periph_USART2,
        .suc_pin_tx =  0x09,  // PA9
        .suc_pin_rx =  0x0a,  // PA10
        .suc_pin_rts = 0x0c,  // PA12
        .suc_pin_cts = 0x0b,  // PA11
        .suc_pin_af = GPIO_AF_7,
        .suc_irqn = USART2_IRQn
    }
};

const struct stm32f3_uart_cfg *
bsp_uart_config(int port)
{
    assert(port < UART_CNT);
    return &uart_cfg[port];
}
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_sram_start,
        .hbmd_size = SRAM_SIZE
    },
    [1] = {
        .hbmd_start = &_ccram_start,
        .hbmd_size = CCRAM_SIZE
    }
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return stm32f3_flash_dev();
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    /* Add any interrupt priorities configured by the bsp here */
    return pri;
}


void
hal_bsp_init(void)
{
}
