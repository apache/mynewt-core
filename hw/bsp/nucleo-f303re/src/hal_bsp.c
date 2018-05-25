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

#include "os/mynewt.h"

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1)
/*
 * It is necessary to include uart/uart.h before stm32fxx_hal_uart.h gets included
 * due to a name conflict for UART_PARITY_NONE.
 */
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#include "assert.h"
#include "bsp/bsp.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash_int.h"
#include "hal/hal_gpio.h"
#include "hal/hal_system.h"
#include "mcu/mcu.h"
#include "mcu/stm32f3_bsp.h"
#include "stm32f3xx.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1)
static struct uart_dev hal_uart[2];

static const struct stm32_uart_cfg uart_cfg[UART_CNT] = {
#if MYNEWT_VAL(UART_0)
    {
        .suc_uart    = USART2,
        .suc_rcc_reg = &RCC->APB1ENR,
        .suc_rcc_dev = RCC_APB1ENR_USART2EN,
        .suc_pin_tx  = MCU_GPIO_PORTA(2),
        .suc_pin_rx  = MCU_GPIO_PORTA(3),
        .suc_pin_rts = MCU_GPIO_PORTA(1),
        .suc_pin_cts = MCU_GPIO_PORTA(0),
        .suc_pin_af  = GPIO_AF7_USART2,
        .suc_irqn    = USART2_IRQn
    },
#endif
#if MYNEWT_VAL(UART_1)
    {
        .suc_uart    = USART1,
        .suc_rcc_reg = &RCC->APB2ENR,
        .suc_rcc_dev = RCC_APB2ENR_USART1EN,
        .suc_pin_tx  = MCU_GPIO_PORTA(9),
        .suc_pin_rx  = MCU_GPIO_PORTA(10),
        .suc_pin_rts = MCU_GPIO_PORTA(12),
        .suc_pin_cts = MCU_GPIO_PORTA(11),
        .suc_pin_af  = GPIO_AF7_USART1,
        .suc_irqn    = USART1_IRQn
    },
#endif
};

const struct stm32_uart_cfg *
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
    int rc;
    (void)rc;  /* in case there are no devices declared */

    hal_system_clock_start();

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *)&hal_uart[UART_0_DEV_ID],
        UART_DEV_NAME(UART_0_DEV_ID),
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init,
        (void *)&uart_cfg[UART_0_DEV_ID]);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *)&hal_uart[UART_1_DEV_ID],
        UART_DEV_NAME(UART_1_DEV_ID),
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init,
        (void *)&uart_cfg[UART_1_DEV_ID]);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, TIM15);
    assert(rc == 0);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}
