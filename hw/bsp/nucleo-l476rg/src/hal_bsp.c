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

#include "os/mynewt.h"

#if MYNEWT_VAL(UART_0)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#include <hal/hal_bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_timer.h>

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
#include <hal/hal_spi.h>
#endif

#include <stm32l476xx.h>
#include <stm32l4xx_hal_rcc.h>
#include <stm32l4xx_hal_pwr.h>
#include <stm32l4xx_hal_flash.h>
#include <stm32l4xx_hal_gpio_ex.h>
#include <mcu/stm32l4_bsp.h>
#include "mcu/stm32l4xx_mynewt_hal.h"
#include "mcu/stm32_hal.h"
#include "hal/hal_i2c.h"

#include "bsp/bsp.h"

#if MYNEWT_VAL(UART_0)
static struct uart_dev hal_uart0;

static const struct stm32_uart_cfg uart0_cfg = {
    .suc_uart = USART2,
    .suc_rcc_reg = &RCC->APB1ENR1,
    .suc_rcc_dev = RCC_APB1ENR1_USART2EN,
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
    .suc_pin_af = GPIO_AF7_USART2,
    .suc_irqn = USART2_IRQn
};
#endif

#if MYNEWT_VAL(UART_1)
static struct uart_dev hal_uart1;

static const struct stm32_uart_cfg uart1_cfg = {
    .suc_uart = USART1,
    .suc_rcc_reg = &RCC->APB2ENR,
    .suc_rcc_dev = RCC_APB2ENR_USART1EN,
    .suc_pin_tx = MYNEWT_VAL(UART_1_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_1_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_1_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_1_PIN_CTS),
    .suc_pin_af = GPIO_AF7_USART1,
    .suc_irqn = USART1_IRQn
};
#endif

#if MYNEWT_VAL(I2C_0)
/*
 * NOTE: The PB8 and PB9 pins are connected through jumpers in the board to
 * both AIN and I2C pins. To enable I2C functionality SB51/SB56 need to
 * be removed (they are the default connections) and SB46/SB52 need to
 * be added.
 */
static struct stm32_hal_i2c_cfg i2c_cfg0 = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C1EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_timingr = 0x00000E14,            /* 100KHz at 4MHz of SysCoreClock */
};
#endif

#if MYNEWT_VAL(I2C_1)
static struct stm32_hal_i2c_cfg i2c_cfg1 = {
    .hic_i2c = I2C2,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C2EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C2,
    .hic_10bit = 0,
    .hic_timingr = 0x00000E14,            /* 100KHz at 4MHz of SysCoreClock */
};
#endif

#if MYNEWT_VAL(I2C_2)
/*
 * NOTE: The PC0 and PC1 pins are connected through solder bridges SB51/SB56
 * in the board to A4 and A5 pins.
 * If solder brides are removed I2C_2 can't be accessed since only PC0, PC1
 * are present on Nucleo-64 board for I2C3 to use.
 */
static struct stm32_hal_i2c_cfg i2c_cfg2 = {
    .hic_i2c = I2C3,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C3EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_2_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_2_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C3,
    .hic_10bit = 0,
    .hic_timingr = 0x00000E14,            /* 100KHz at 4MHz of SysCoreClock */
};
#endif

#if MYNEWT_VAL(SPI_0_SLAVE) || MYNEWT_VAL(SPI_0_MASTER)
struct stm32_hal_spi_cfg spi0_cfg = {
    .ss_pin   = MCU_GPIO_PORTA(4),
    .sck_pin  = MCU_GPIO_PORTA(5),
    .miso_pin = MCU_GPIO_PORTA(6),
    .mosi_pin = MCU_GPIO_PORTA(7),
    .irq_prio = 2,
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    },
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
    int rc;

    (void)rc;

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &hal_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart0_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *) &hal_uart1, "uart1",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart1_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    hal_timer_init(0, TIM2);
#endif

#if MYNEWT_VAL(TIMER_1)
    hal_timer_init(1, TIM3);
#endif

#if MYNEWT_VAL(TIMER_2)
    hal_timer_init(2, TIM4);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
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

#if MYNEWT_VAL(I2C_1)
    rc = hal_i2c_init(1, &i2c_cfg1);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_2)
    rc = hal_i2c_init(2, &i2c_cfg2);
    assert(rc == 0);
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
