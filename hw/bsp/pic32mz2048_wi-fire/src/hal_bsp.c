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

#include "os/mynewt.h"

#include <hal/hal_bsp.h>
#include <mcu/mips_bsp.h>
#include <mcu/mips_hal.h>
#include "hal/hal_i2c.h"
#if MYNEWT_VAL(TIMER_0) || MYNEWT_VAL(TIMER_1) || MYNEWT_VAL(TIMER_2) || \
    MYNEWT_VAL(TIMER_3) || MYNEWT_VAL(TIMER_4) || MYNEWT_VAL(TIMER_5) || \
    MYNEWT_VAL(TIMER_6) || MYNEWT_VAL(TIMER_7)
#include "hal/hal_timer.h"
#endif

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2) || \
    MYNEWT_VAL(UART_3) || MYNEWT_VAL(UART_4) || MYNEWT_VAL(UART_5)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER) || \
    MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_3_MASTER) || \
    MYNEWT_VAL(SPI_4_MASTER) || MYNEWT_VAL(SPI_5_MASTER)
#include "hal/hal_spi.h"
#endif
#include <bsp/bsp.h>
#include <string.h>
#include <xc.h>

/* JTAG on, WDT off */
#pragma config JTAGEN=1, FWDTEN=0
/* DMT off, primary oscilator EC mode, PLL */
#pragma config FDMTEN=0, POSCMOD=0, FNOSC=1, DMTCNT=1
/* 24MHz posc input to pll, div by 3, multiply by 50, div by 2 -> 200mhz*/
#pragma config FPLLODIV=1, FPLLMULT=49, FPLLICLK=0, FPLLRNG=1, FPLLIDIV=2
/* USB off */
#pragma config FUSBIDIO=0
/*
 * Watchdog in non-window mode, watchdog disabled during flash programming,
 * period: 32s
 */
#pragma config WINDIS=1, WDTSPGM=1, WDTPS=15

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
#endif

#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
#endif

#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart2;
#endif

#if MYNEWT_VAL(UART_3)
static struct uart_dev os_bsp_uart3;
#endif

#if MYNEWT_VAL(UART_4)
static struct uart_dev os_bsp_uart4;
static const struct mips_uart_cfg uart4_cfg = {
    .tx = MCU_GPIO_PORTF(8),
    .rx = MCU_GPIO_PORTF(2)
};
#endif

#if MYNEWT_VAL(UART_5)
static struct uart_dev os_bsp_uart5;
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
/*
 * SPI 1 (J9 connector)
 *   MOSI -> RF0
 *   MISO -> RD11
 *   SCK  -> RG6
 */
static const struct mips_spi_cfg spi1_cfg = {
    .mosi = MCU_GPIO_PORTF(0),
    .miso = MCU_GPIO_PORTD(11),
    .sck = MCU_GPIO_PORTG(6)
};
#endif

#if MYNEWT_VAL(SPI_2_MASTER)
/*
 * SPI 2 (microSD card)
 *   MOSI -> RB10
 *   MISO -> RC4
 *   SCK  -> RB14
 */
static const struct mips_spi_cfg spi2_cfg = {
    .mosi = MCU_GPIO_PORTB(10),
    .miso = MCU_GPIO_PORTC(4),
    .sck = MCU_GPIO_PORTB(14)
};
#endif

#if MYNEWT_VAL(SPI_3_MASTER)
/*
 * SPI 3 (MRF24WG0MA)
 *   MOSI -> RF5
 *   MISO -> RG0
 *   SCK  -> RD10
 */
static const struct mips_spi_cfg spi3_cfg = {
    .mosi = MCU_GPIO_PORTF(5),
    .miso = MCU_GPIO_PORTG(0),
    .sck = MCU_GPIO_PORTD(10)
};
#endif

#if MYNEWT_VAL(I2C_3)
/*
 * I2C 3 (J6 connector)
 *   SCL -> RG8
 *   SDA -> RG7
 */
static const struct mips_i2c_cfg hal_i2c3_cfg = {
    .scl = MCU_GPIO_PORTG(8),
    .sda = MCU_GPIO_PORTG(7),
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

    return &pic32mz_flash_dev;
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

    #if MYNEWT_VAL(TIMER_4)
        rc = hal_timer_init(4, NULL);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(TIMER_5)
        rc = hal_timer_init(5, NULL);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(TIMER_6)
        rc = hal_timer_init(6, NULL);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(TIMER_7)
        rc = hal_timer_init(7, NULL);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(UART_0)
        rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(UART_1)
        rc = os_dev_create((struct os_dev *) &os_bsp_uart1, "uart1",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(UART_2)
        rc = os_dev_create((struct os_dev *) &os_bsp_uart2, "uart2",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(UART_3)
        rc = os_dev_create((struct os_dev *) &os_bsp_uart3, "uart3",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart4_cfg);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(UART_4)
        rc = os_dev_create((struct os_dev *) &os_bsp_uart4, "uart4",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(UART_5)
        rc = os_dev_create((struct os_dev *) &os_bsp_uart5, "uart5",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(SPI_0_MASTER)
        rc = hal_spi_init(0, NULL, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(SPI_1_MASTER)
        rc = hal_spi_init(1, (void *)&spi1_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(SPI_2_MASTER)
        rc = hal_spi_init(2, (void *)&spi2_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(SPI_3_MASTER)
        rc = hal_spi_init(3, (void *)&spi3_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(SPI_4_MASTER)
        rc = hal_spi_init(4, NULL, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(SPI_5_MASTER)
        rc = hal_spi_init(5, NULL, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    #endif

    #if MYNEWT_VAL(I2C_3)
        rc = hal_i2c_init(3, (void*)&hal_i2c3_cfg);
        assert(rc == 0);
    #endif

    (void)rc;
}

int
hal_bsp_hw_id(uint8_t *id, int max_len)
{
    if (max_len > sizeof(DEVID)) {
        max_len = sizeof(DEVID);
    }

    memcpy(id, (const void *)&DEVID, max_len);
    return max_len;
}
