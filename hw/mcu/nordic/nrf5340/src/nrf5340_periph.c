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
#include <mcu/nrf5340_hal.h>
#include <bsp/bsp.h>
#include <nrfx.h>
#include "hal/hal_spi.h"

#if MYNEWT_VAL(ADC_0)
#include <adc/adc.h>
#include <adc_nrf52/adc_nrf52.h>
#endif

#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2) || MYNEWT_VAL(PWM_3)
#include <pwm/pwm.h>
#include <pwm_nrf52/pwm_nrf52.h>
#endif

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2) || MYNEWT_VAL(UART_3)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_3_MASTER) || MYNEWT_VAL(SPI_4_MASTER)
#include "bus/drivers/spi_hal.h"
#endif

#if MYNEWT_VAL(I2C_0) || MYNEWT_VAL(I2C_1) || MYNEWT_VAL(I2C_2) || MYNEWT_VAL(I2C_3)
#include <bus/drivers/i2c_common.h>
#include <bus/drivers/i2c_nrf5340.h>
#endif

#if MYNEWT_VAL(ADC_0)
static struct adc_dev os_bsp_adc0;
static struct nrf52_adc_dev_cfg os_bsp_adc0_config = {
    .nadc_refmv = MYNEWT_VAL(ADC_0_REFMV_0),
};
#endif

#if MYNEWT_VAL(PWM_0)
static struct pwm_dev os_bsp_pwm0;
#endif
#if MYNEWT_VAL(PWM_1)
static struct pwm_dev os_bsp_pwm1;
#endif
#if MYNEWT_VAL(PWM_2)
static struct pwm_dev os_bsp_pwm2;
#endif
#if MYNEWT_VAL(PWM_3)
static struct pwm_dev os_bsp_pwm3;
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf5340_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
static const struct nrf5340_uart_cfg os_bsp_uart1_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_1_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_1_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_1_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_1_PIN_CTS),
};
#endif
#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart0;
static const struct nrf5340_uart_cfg os_bsp_uart2_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_2_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_2_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_2_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_2_PIN_CTS),
};
#endif
#if MYNEWT_VAL(UART_3)
static struct uart_dev os_bsp_uart0;
static const struct nrf5340_uart_cfg os_bsp_uart3_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_3_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_3_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_3_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_3_PIN_CTS),
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
static const struct nrf5340_hal_spi_cfg os_bsp_spi0s_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_0_SLAVE_PIN_MISO),
    .ss_pin  = MYNEWT_VAL(SPI_0_SLAVE_PIN_SS),
};
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
static const struct bus_spi_dev_cfg spi1_cfg = {
    .spi_num = 1,
    .pin_sck = MYNEWT_VAL(SPI_1_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi1_bus;
#endif
#if MYNEWT_VAL(SPI_1_SLAVE)
static const struct nrf5340_hal_spi_cfg os_bsp_spi1s_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_1_SLAVE_PIN_SS),
};
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
static const struct bus_spi_dev_cfg spi2_cfg = {
    .spi_num = 2,
    .pin_sck = MYNEWT_VAL(SPI_2_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_2_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_2_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi2_bus;
#endif
#if MYNEWT_VAL(SPI_2_SLAVE)
static const struct nrf5340_hal_spi_cfg os_bsp_spi2s_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_2_SLAVE_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_2_SLAVE_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_2_SLAVE_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_2_SLAVE_PIN_SS),
};
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
static const struct bus_spi_dev_cfg spi3_cfg = {
    .spi_num = 3,
    .pin_sck = MYNEWT_VAL(SPI_3_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_3_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_3_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi3_bus;
#endif
#if MYNEWT_VAL(SPI_3_SLAVE)
static const struct nrf5340_hal_spi_cfg os_bsp_spi3s_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_3_SLAVE_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_3_SLAVE_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_3_SLAVE_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_3_SLAVE_PIN_SS),
};
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
static const struct bus_spi_dev_cfg spi4_cfg = {
    .spi_num = 4,
    .pin_sck = MYNEWT_VAL(SPI_4_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_4_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_4_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi4_bus;
#endif

#if MYNEWT_VAL(I2C_0)
static const struct bus_i2c_dev_cfg i2c0_cfg = {
    .i2c_num = 0,
    .pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
};
static struct bus_i2c_dev i2c0_bus;
#endif

#if MYNEWT_VAL(I2C_1)
static const struct bus_i2c_dev_cfg i2c1_cfg = {
    .i2c_num = 1,
    .pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
};
static struct bus_i2c_dev i2c1_bus;
#endif

#if MYNEWT_VAL(I2C_2)
static const struct bus_i2c_dev_cfg i2c2_cfg = {
    .i2c_num = 2,
    .pin_sda = MYNEWT_VAL(I2C_2_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_2_PIN_SCL),
};
static struct bus_i2c_dev i2c2_bus;
#endif

#if MYNEWT_VAL(I2C_3)
static const struct bus_i2c_dev_cfg i2c3_cfg = {
    .i2c_num = 3,
    .pin_sda = MYNEWT_VAL(I2C_3_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_3_PIN_SCL),
};
static struct bus_i2c_dev i2c3_bus;
#endif

static void
nrf5340_periph_create_timers(void)
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
nrf5340_periph_create_adc(void)
{
#if MYNEWT_VAL(ADC_0)
    int rc;

    (void)rc;

    rc = os_dev_create(&os_bsp_adc0.ad_dev, "adc0",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       nrf52_adc_dev_init, &os_bsp_adc0_config);
    assert(rc == 0);
#endif
}

static void
nrf5340_periph_create_pwm(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(PWM_0)
    rc = os_dev_create(&os_bsp_pwm0.pwm_os_dev, "pwm0",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       nrf52_pwm_dev_init, UINT_TO_POINTER(0));
    assert(rc == 0);
#endif
#if MYNEWT_VAL(PWM_1)
    rc = os_dev_create(&os_bsp_pwm1.pwm_os_dev, "pwm1",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       nrf52_pwm_dev_init, UINT_TO_POINTER(1));
    assert(rc == 0);
#endif
#if MYNEWT_VAL(PWM_2)
    rc = os_dev_create(&os_bsp_pwm2.pwm_os_dev, "pwm2",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       nrf52_pwm_dev_init, UINT_TO_POINTER(2));
    assert(rc == 0);
#endif
#if MYNEWT_VAL(PWM_3)
    rc = os_dev_create(&os_bsp_pwm3.pwm_os_dev, "pwm3",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       nrf52_pwm_dev_init, UINT_TO_POINTER(3));
    assert(rc == 0);
#endif
}

static void
nrf5340_periph_create_uart(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create(&os_bsp_uart0.ud_dev, "uart0",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init,
                       (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_1)
    rc = os_dev_create(&os_bsp_uart1.ud_dev, "uart1",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_2)
    rc = os_dev_create(&os_bsp_uart2.ud_dev, "uart2",
                       OS_DEV_INIT_PRIMARY, 2, uart_hal_init,
                       (void *)&os_bsp_uart2_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_3)
    rc = os_dev_create(&os_bsp_uart3.ud_dev, "uart3",
                       OS_DEV_INIT_PRIMARY, 3, uart_hal_init,
                       (void *)&os_bsp_uart3_cfg);
    assert(rc == 0);
#endif
}

static void
nrf5340_periph_create_spi(void)
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
#if MYNEWT_VAL(SPI_1_MASTER)
    rc = bus_spi_hal_dev_create("spi1", &spi1_bus,
                                (struct bus_spi_dev_cfg *)&spi1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_1_SLAVE)
    rc = hal_spi_init(1, (void *)&os_bsp_spi1s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    rc = bus_spi_hal_dev_create("spi2", &spi2_bus,
                                (struct bus_spi_dev_cfg *)&spi2_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_SLAVE)
    rc = hal_spi_init(2, (void *)&os_bsp_spi2s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
    rc = bus_spi_hal_dev_create("spi3", &spi3_bus,
                                (struct bus_spi_dev_cfg *)&spi3_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_3_SLAVE)
    rc = hal_spi_init(3, (void *)&os_bsp_spi3s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
    rc = bus_spi_hal_dev_create("spi4", &spi4_bus,
                                (struct bus_spi_dev_cfg *)&spi4_cfg);
    assert(rc == 0);
#endif
}

static void
nrf5340_periph_create_i2c(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(I2C_0)
    rc = bus_i2c_nrf5340_dev_create("i2c0", &i2c0_bus,
                                    (struct bus_i2c_dev_cfg *)&i2c0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_1)
    rc = bus_i2c_nrf5340_dev_create("i2c1", &i2c1_bus,
                                    (struct bus_i2c_dev_cfg *)&i2c1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_2)
    rc = bus_i2c_nrf5340_dev_create("i2c2", &i2c2_bus,
                                    (struct bus_i2c_dev_cfg *)&i2c2_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_3)
    rc = bus_i2c_nrf5340_dev_create("i2c3", &i2c3_bus,
                                    (struct bus_i2c_dev_cfg *)&i2c3_cfg);
    assert(rc == 0);
#endif
}

void
nrf5340_periph_create(void)
{
    nrf5340_periph_create_timers();
    nrf5340_periph_create_adc();
    nrf5340_periph_create_pwm();
    nrf5340_periph_create_uart();
    nrf5340_periph_create_spi();
    nrf5340_periph_create_i2c();
}
