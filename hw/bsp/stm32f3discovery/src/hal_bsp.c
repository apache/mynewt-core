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
#include <assert.h>
#include "os/mynewt.h"

#if MYNEWT_VAL(UART_0)
/*
 * It is necessary to include uart/uart.h before stm32fxx_hal_uart.h gets included
 * due to a name conflict for UART_PARITY_NONE.
 */
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#include <hal/hal_bsp.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_gpio.h>
#include <hal/hal_i2c.h>
#include <hal/hal_system.h>

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE) || \
    MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
#include <hal/hal_spi.h>
#endif

#include <stm32f3xx.h>
#include <stm32f3xx_hal.h>
#include "mcu/stm32f3_bsp.h"
#include "mcu/stm32f3xx_mynewt_hal.h"
#include "mcu/stm32_hal.h"

#include "bsp/bsp.h"
#include "mcu/mcu.h"
#include "os/os_cputime.h"

#if MYNEWT_VAL(UART_0)
static struct uart_dev hal_uart[1];

static const struct stm32_uart_cfg uart_cfg[1] = {
    {
        .suc_uart    = USART1,
        .suc_rcc_reg = &RCC->APB2ENR,
        .suc_rcc_dev = RCC_APB2ENR_USART1EN,
        .suc_pin_tx  = MCU_GPIO_PORTA(9),
        .suc_pin_rx  = MCU_GPIO_PORTA(10),
        .suc_pin_rts = -1,
        .suc_pin_cts = -1,
        .suc_pin_af  = GPIO_AF7_USART1,
        .suc_irqn    = USART1_IRQn,
    },
};

const struct stm32_uart_cfg *
bsp_uart_config(int port)
{
    assert(port == 0);
    return &uart_cfg[port];
}
#endif

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
struct stm32_hal_spi_cfg spi0_cfg = {
    .ss_pin   = MCU_GPIO_PORTA(4),
    .sck_pin  = MCU_GPIO_PORTA(5),
    .miso_pin = MCU_GPIO_PORTA(6),
    .mosi_pin = MCU_GPIO_PORTA(7),
    .irq_prio = 2,
};
#endif

#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
struct stm32_hal_spi_cfg spi1_cfg = {
    .ss_pin   = MCU_GPIO_PORTB(12),
    .sck_pin  = MCU_GPIO_PORTB(13),
    .miso_pin = MCU_GPIO_PORTB(14),
    .mosi_pin = MCU_GPIO_PORTB(15),
    .irq_prio = 2,
};
#endif

#if MYNEWT_VAL(I2C_0)
static struct stm32_hal_i2c_cfg i2c_cfg0 = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR,
    .hic_rcc_dev = RCC_APB1ENR_I2C1EN,
    .hic_pin_sda = MCU_GPIO_PORTB(9),
    .hic_pin_scl = MCU_GPIO_PORTB(8),
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_timingr = 0x10420F13,    /* 100KHz at 8MHz of SysCoreClock */
};
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
    rc = os_dev_create((struct os_dev *)&hal_uart[0], UART_DEV_NAME,
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_cfg[0]);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, &i2c_cfg0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, TIM15);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, TIM16);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, TIM17);
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

#if MYNEWT_VAL(SPI_1_MASTER)
    rc = hal_spi_init(1, &spi1_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_1_SLAVE)
    rc = hal_spi_init(1, &spi1_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}
