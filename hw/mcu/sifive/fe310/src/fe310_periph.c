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

#include <os/mynewt.h>
#include <assert.h>

#include <hal/hal_spi.h>

#include <defs/error.h>
#include <syscfg/syscfg.h>

#include <mcu/fe310_hal.h>
#include <mcu/sys_clock.h>
#include <sifive/devices/spi.h>
#include <env/freedom-e300-hifive1/platform.h>
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/bus.h"
#if MYNEWT_VAL(SPI_0) || MYNEWT_VAL(SPI_1) || MYNEWT_VAL(SPI_2)
#include "bus/drivers/spi_common.h"
#include "bus/drivers/spi_hal.h"
#endif
#endif
#if MYNEWT_VAL(UART_0)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#if MYNEWT_VAL(TIMER_0)
extern struct fe310_hal_tmr fe310_pwm2;
#endif
#if MYNEWT_VAL(TIMER_1)
extern struct fe310_hal_tmr fe310_pwm1;
#endif
#if MYNEWT_VAL(TIMER_2)
extern struct fe310_hal_tmr fe310_pwm0;
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#if MYNEWT_VAL(SPI_1)
static const struct bus_spi_dev_cfg spi1_cfg = {
    .spi_num = 1,
};
static struct bus_spi_hal_dev spi1_bus;
#endif
#if MYNEWT_VAL(SPI_2)
static const struct bus_spi_hal_dev spi2_cfg = {
    .spi_num = 2,
};
static struct bus_spi_dev spi2_bus;
#endif
#endif

static void
fe310_periph_create_timers(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, (void *)&fe310_pwm2);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, (void *)&fe310_pwm1);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, (void *)&fe310_pwm0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
fe310_periph_create_uart(void)
{
#if MYNEWT_VAL(UART_0)
    int rc;

    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif

}

static void
fe310_periph_create_spi(void)
{
    int rc;

    (void)rc;
#if MYNEWT_VAL(SPI_1)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_hal_dev_create("spi1", &spi1_bus,
                                (struct bus_spi_dev_cfg *)&spi1_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(1, NULL, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif

#if MYNEWT_VAL(SPI_2)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_hal_dev_create("spi2", &spi2_bus,
                                (struct bus_spi_dev_cfg *)&spi2_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(2, NULL, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif
}

void
fe310_periph_create(void)
{
    fe310_periph_create_timers();
    fe310_periph_create_uart();
    fe310_periph_create_spi();
}
