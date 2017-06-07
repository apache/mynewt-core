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

#include <os/os_dev.h>
#if MYNEWT_VAL(UART_0)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#include <hal/hal_bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_i2c.h>
#include <hal/hal_timer.h>
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
#include <hal/hal_spi.h>
#endif
#include <stm32f401xe.h>
#include <stm32f4xx_hal_gpio_ex.h>
#include <mcu/mcu.h>
#include <mcu/stm32f4_bsp.h>
#include <mcu/stm32f4xx_mynewt_hal.h>
#include "bsp/bsp.h"
#include <assert.h>

#if MYNEWT_VAL(UART_0)
static struct uart_dev hal_uart0;

static const struct stm32f4_uart_cfg uart_cfg[UART_CNT] = {
    [0] = {
        .suc_uart = USART2,
        .suc_rcc_reg = &RCC->APB1ENR,
        .suc_rcc_dev = RCC_APB1ENR_USART2EN,
        .suc_pin_tx = MCU_GPIO_PORTA(2),	/* PA2 */
        .suc_pin_rx = MCU_GPIO_PORTA(3),	/* PA3 */
        .suc_pin_rts = MCU_GPIO_PORTA(1),	/* PA1 */
        .suc_pin_cts = MCU_GPIO_PORTA(0),	/* PA0 */
        .suc_pin_af = GPIO_AF7_USART2,
        .suc_irqn = USART2_IRQn
    }
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    }
};

#if MYNEWT_VAL(I2C_0)
static struct stm32f4_hal_i2c_cfg i2c_cfg0 = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR,
    .hic_rcc_dev = RCC_APB1ENR_I2C1EN,
    .hic_pin_sda = MCU_GPIO_PORTB(9),		/* PB9 */
    .hic_pin_scl = MCU_GPIO_PORTB(8),		/* PB8 */
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_speed = 100000				/* 100kHz */
};
#endif

#if MYNEWT_VAL(SPI_0_SLAVE) || MYNEWT_VAL(SPI_0_MASTER)
struct stm32f4_hal_spi_cfg spi0_cfg = {
    .ss_pin = MCU_GPIO_PORTA(4),		/* PA4 */
    .sck_pin  = MCU_GPIO_PORTA(5),		/* PA5 */
    .miso_pin = MCU_GPIO_PORTA(6),		/* PA6 */
    .mosi_pin = MCU_GPIO_PORTB(5),		/* PB5 */
    .irq_prio = 2
};
#endif

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

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, &i2c_cfg0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    hal_timer_init(0, TIM9);
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
