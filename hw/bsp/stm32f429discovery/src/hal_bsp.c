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

#include <syscfg/syscfg.h>
#include <bsp/bsp.h>
#include <bsp/cmsis_nvic.h>
#include <flash_map/flash_map.h>
#include <os/os_dev.h>
#include <stm32f429xx.h>
#include <stm32f4xx_hal_gpio_ex.h>

#if MYNEWT_VAL(UART_0)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#include <hal/hal_bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_timer.h>
#include <hal/hal_system.h>

#include <mcu/stm32f4_bsp.h>



#if MYNEWT_VAL(UART_0)
static struct uart_dev hal_uart0;

static const struct stm32f4_uart_cfg uart_cfg[UART_CNT] = {
    [0] = {
        .suc_uart = USART2,
        .suc_rcc_reg = &RCC->APB1ENR,
        .suc_rcc_dev = RCC_APB1ENR_USART2EN,
        .suc_pin_tx = MCU_GPIO_PORTD(5),	/* PD5 */
        .suc_pin_rx = MCU_GPIO_PORTD(6),	/* PD6 */
        .suc_pin_rts = -1,
        .suc_pin_cts = -1,
        .suc_pin_af = GPIO_AF7_USART2,
        .suc_irqn = USART2_IRQn
    }
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

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &stm32f4_flash_dev;
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
    int rc;

    (void)rc;

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &hal_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_cfg[0]);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_0)
    hal_timer_init(0, TIM1);
#endif
#if MYNEWT_VAL(TIMER_1)
    hal_timer_init(1, TIM9);
#endif
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
