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

#if MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE)
#include <hal/hal_spi.h>
#endif

#include <stm32l072xx.h>
#include <stm32l0xx_hal_rcc.h>
#include <stm32l0xx_hal_pwr.h>
#include <stm32l0xx_hal_flash.h>
#include <stm32l0xx_hal_gpio_ex.h>
#include <mcu/stm32l0_bsp.h>
#include "mcu/stm32l0xx_mynewt_hal.h"
#include "mcu/stm32_hal.h"
#include "hal/hal_i2c.h"

#include "bsp/bsp.h"

#if MYNEWT_VAL(UART_0)
static struct uart_dev hal_uart0;

static const struct stm32_uart_cfg uart_cfg[UART_CNT] = {
    [0] = {
        .suc_uart = USART2,
        .suc_rcc_reg = &RCC->APB1ENR,
        .suc_rcc_dev = RCC_APB1ENR_USART2EN,
        .suc_pin_tx = MCU_GPIO_PORTA(2),
        .suc_pin_rx = MCU_GPIO_PORTA(3),
        .suc_pin_rts = -1,
        .suc_pin_cts = -1,
        .suc_pin_af = GPIO_AF4_USART2,
        .suc_irqn = USART2_IRQn
    }
};
#endif

#if MYNEWT_VAL(I2C_0)
static struct stm32_hal_i2c_cfg i2c_cfg0 = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR,
    .hic_rcc_dev = RCC_APB1ENR_I2C1EN,
    .hic_pin_sda = MCU_GPIO_PORTB(9),     /* PB9 on CN3 */
    .hic_pin_scl = MCU_GPIO_PORTB(8),     /* PB8 on CN3 */
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_timingr = 0x10420F13,      /* FIXME: 100KHz at 8MHz of SysCoreClock */
};
#endif

#if MYNEWT_VAL(SPI_1_SLAVE) || MYNEWT_VAL(SPI_1_MASTER)
struct stm32_hal_spi_cfg spi1_cfg = {
    .ss_pin   = MCU_GPIO_PORTB(12),
    .sck_pin  = MCU_GPIO_PORTB(13),
    .miso_pin = MCU_GPIO_PORTB(14),
    .mosi_pin = MCU_GPIO_PORTB(15),
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
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_cfg[0]);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    hal_timer_init(0, TIM2);
#endif

#if MYNEWT_VAL(TIMER_1)
    hal_timer_init(1, TIM3);
#endif

#if MYNEWT_VAL(TIMER_2)
    hal_timer_init(2, TIM21);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
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

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, &i2c_cfg0);
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

#if MYNEWT_VAL(LORA_NODE)
void lora_bsp_enable_mac_timer(void)
{
    /* Turn on the LoRa MAC timer. This function is automatically
     * called by the LoRa stack when exiting low power mode.*/
    #if MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 0
        #define TIMER_INIT  TIM2
    #elif MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 1
        #define TIMER_INIT  TIM3
    #elif MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 2
        #define TIMER_INIT  TIM21
    #else
        #error "Invalid LORA_MAC_TIMER_NUM"
    #endif

    hal_timer_init(MYNEWT_VAL(LORA_MAC_TIMER_NUM), TIMER_INIT);
}
#endif
