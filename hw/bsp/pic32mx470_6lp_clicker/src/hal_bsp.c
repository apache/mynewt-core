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
#include <assert.h>

#include <hal/hal_bsp.h>
#include <syscfg/syscfg.h>
#include <mcu/mcu.h>
#include <mcu/mips_bsp.h>
#include <mcu/mips_hal.h>

#include "hal/hal_i2c.h"
#if MYNEWT_VAL(TIMER_0) || MYNEWT_VAL(TIMER_1) || MYNEWT_VAL(TIMER_2) || \
    MYNEWT_VAL(TIMER_3)
#include "hal/hal_timer.h"
#endif

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || \
    MYNEWT_VAL(UART_2) || MYNEWT_VAL(UART_3)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER)
#include "hal/hal_spi.h"
#endif

#include <xc.h>

#pragma config CP=1, FWDTEN=0, FCKSM=1, FPBDIV=1, OSCIOFNC=1, POSCMOD=1
/* PLL conf div in: 2, mul: 20, div out: 1 8->4->80->80 */
#pragma config FNOSC=3, FPLLODIV=0, UPLLEN=1, FPLLMUL=5, FPLLIDIV=1, FSRSSEL=7
/* PGEC2/PGED2 pair is used */
#pragma config ICESEL=2
/* Watchdog in non-window mode, period: 32s */
#pragma config WINDIS=1, WDTPS=15

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
#endif

#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
static const struct mips_uart_cfg uart1_cfg = {
    .tx = MCU_GPIO_PORTD(11),
    .rx = MCU_GPIO_PORTB(9)
};
#endif

#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart2;
static const struct mips_uart_cfg uart2_cfg = {
    .tx = MCU_GPIO_PORTF(4),
    .rx = MCU_GPIO_PORTF(5)
};
#endif

#if MYNEWT_VAL(UART_3)
static struct uart_dev os_bsp_uart3;
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
/*
 * SPI 0 (connected to CA8210)
 *   MOSI -> RD4
 *   MISO -> RD3
 *   SCK  -> RD2
 */
static const struct mips_spi_cfg spi0_cfg = {
    .mosi = MCU_GPIO_PORTD(4),
    .miso = MCU_GPIO_PORTD(3),
    .sck = MCU_GPIO_PORTD(2)
};
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
/*
 * SPI 1 (Mikrobus connector)
 *   MOSI -> RG8
 *   MISO -> RG7
 *   SCK  -> RG6
 */
static const struct mips_spi_cfg spi1_cfg = {
    .mosi = MCU_GPIO_PORTG(8),
    .miso = MCU_GPIO_PORTG(7),
    .sck = MCU_GPIO_PORTG(6)
};
#endif

#if MYNEWT_VAL(I2C_0)
/*
 * I2C 0 (Mikrobus connector)
 *   SCL -> RD10
 *   SDA -> RD9
 */
static const struct mips_i2c_cfg hal_i2c0_cfg = {
    .scl = MCU_GPIO_PORTD(10),
    .sda = MCU_GPIO_PORTD(9),
    .frequency = 400000
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

    return &pic32mx_flash_dev;
}

void
hal_bsp_init(void)
{
    int rc;

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_3)
    rc = hal_timer_init(3, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart1, "uart1",
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart1_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_2)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart2, "uart2",
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart2_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_3)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart3, "uart3",
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, (void *)&spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
    rc = hal_spi_init(1, (void *)&spi1_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, (void *)&hal_i2c0_cfg);
    assert(rc == 0);
#endif

    (void)rc;
}
