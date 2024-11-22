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
#include <stdint.h>
#include <syscfg/syscfg.h>
#include <mcu/nrf54h20_hal.h>
#include <bsp/bsp.h>
#include <nrfx.h>
#include "hal/hal_spi.h"

#if MYNEWT_VAL(UART_0)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
#include "bus/drivers/spi_hal.h"
#endif

#if MYNEWT_VAL(I2C_0)
#include <bus/drivers/i2c_common.h>
#include <bus/drivers/i2c_nrf54h20.h>
#endif

#if MYNEWT_VAL(TRNG)
#include "trng/trng.h"
#include "trng_sw/trng_sw.h"
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf54h20_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
static const struct bus_spi_dev_cfg spi0_cfg = {
    .spi_num = 0,
    .pin_sck = MYNEWT_VAL(SPI_0_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_0_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_0_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi0_bus;
#endif
#if MYNEWT_VAL(SPI_0_SLAVE)
static const struct nrf54h20_hal_spi_cfg os_bsp_spi0s_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_MISO),
    .ss_pin  = MYNEWT_VAL(SPI_0_SLAVE_PIN_SS),
};
#endif

#if MYNEWT_VAL(I2C_0)
static const struct bus_i2c_dev_cfg i2c0_cfg = {
    .i2c_num = 0,
    .pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
};
static struct bus_i2c_dev i2c0_bus;
#endif

#if MYNEWT_VAL(TRNG)
//static struct trng_dev os_bsp_trng;
static struct trng_sw_dev os_bsp_trng;
static int32_t mypid;
static struct trng_sw_dev_cfg os_bsp_trng_cfg = {
    .tsdc_entr = &mypid,
    .tsdc_len = sizeof(mypid)
};
#endif

static void
nrf54h20_periph_create_timers(void)
{
    int rc;

    (void)rc;

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

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
nrf54h20_periph_create_uart(void)
{
#if MYNEWT_VAL(UART_0)
    int rc;

    rc = os_dev_create(&os_bsp_uart0.ud_dev, "uart0",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init,
                       (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif
}

static void
nrf54h20_periph_create_spi(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = bus_spi_hal_dev_create("spi0",
                                &spi0_bus, (struct bus_spi_dev_cfg *)&spi0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, (void *)&os_bsp_spi0s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
}

static void
nrf54h20_periph_create_i2c(void)
{
#if MYNEWT_VAL(I2C_0)
    int rc;

    rc = bus_i2c_nrf54h20_dev_create("i2c0", &i2c0_bus,
                                     (struct bus_i2c_dev_cfg *)&i2c0_cfg);
    assert(rc == 0);
#endif
}

#if MYNEWT_VAL(TRNG)
void
hal_bsp_init_trng(void)
{
    int i;
    int rc;

    /*
     * Add entropy (don't do it like this if you use this for real, use
     * something proper).
     */
    for (i = 0; i < 8; i++) {
        rc = trng_sw_dev_add_entropy(&os_bsp_trng, &mypid, sizeof(mypid));
        assert(rc == 0);
    }
}
#endif

static void
nrf54h20_periph_create_trng(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TRNG)
    mypid = 123;
    rc = os_dev_create((struct os_dev *)&os_bsp_trng, "trng",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       trng_sw_dev_init, &os_bsp_trng_cfg);
    assert(rc == 0);
#endif
}

void
nrf54h20_periph_create(void)
{
    nrf54h20_periph_create_timers();
    // nrf54h20_periph_create_uart();
    // nrf54h20_periph_create_spi();
    // nrf54h20_periph_create_i2c();
    nrf54h20_periph_create_trng();
}
