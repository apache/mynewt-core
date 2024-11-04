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
#include <assert.h>

#include "bsp/bsp.h"
#include "os/mynewt.h"
#include "mynewt_cm.h"

#include <hal/hal_bsp.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_system.h>

#include <stm32f429xx.h>
#include <stm32_common/stm32_hal.h>

#if MYNEWT_VAL(ETH_0)
#include <stm32_eth/stm32_eth.h>
#include <stm32_eth/stm32_eth_cfg.h>
#endif

#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2)
#include <pwm_stm32/pwm_stm32.h>
#endif

const uint32_t stm32_flash_sectors[] = {
    /* Bank 1 */
    0x08000000,     /* 16kB */
    0x08004000,     /* 16kB */
    0x08008000,     /* 16kB */
    0x0800c000,     /* 16kB */
    0x08010000,     /* 64kB */
    0x08020000,     /* 128kB */
    0x08040000,     /* 128kB */
    0x08060000,     /* 128kB */
    0x08080000,     /* 128kB */
    0x080a0000,     /* 128kB */
    0x080c0000,     /* 128kB */
    0x080e0000,     /* 128kB */
    /* Bank 2 */
    0x08100000,     /* 16kB */
    0x08104000,     /* 16kB */
    0x08108000,     /* 16kB */
    0x0810c000,     /* 16kB */
    0x08110000,     /* 64kB */
    0x08120000,     /* 128kB */
    0x08140000,     /* 128kB */
    0x08160000,     /* 128kB */
    0x08180000,     /* 128kB */
    0x081a0000,     /* 128kB */
    0x081c0000,     /* 128kB */
    0x081e0000,     /* 128kB */
    0x08200000,     /* End of flash */
};

#define SZ (sizeof(stm32_flash_sectors) / sizeof(stm32_flash_sectors[0]))
static_assert(MYNEWT_VAL(STM32_FLASH_NUM_AREAS) + 1 == SZ,
        "STM32_FLASH_NUM_AREAS does not match flash sectors");

#if MYNEWT_VAL(UART_0)
/* UART connected to st-link */
const struct stm32_uart_cfg os_bsp_uart0_cfg = {
    .suc_uart = USART1,
    .suc_rcc_reg = &RCC->APB2ENR,
    .suc_rcc_dev = RCC_APB2ENR_USART1EN,
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
    .suc_pin_af = GPIO_AF7_USART1,
    .suc_irqn = USART1_IRQn,
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    },
    [1] = {
        .hbmd_start = &_ccram_start,
        .hbmd_size = CCRAM_SIZE
    }
};

extern const struct hal_flash stm32_flash_dev;
const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &stm32_flash_dev;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

void
hal_bsp_init(void)
{
    stm32_periph_create();
}

void
hal_bsp_deinit(void)
{
    Cortex_DisableAll();
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
