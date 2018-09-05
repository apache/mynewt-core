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
#include "syscfg/syscfg.h"
#include "mcu/nrf52_hal.h"
#include "hal/hal_i2c.h"
#include "hal/hal_spi.h"
#include "bsp/bsp.h"
#include "nrfx.h"
#if MYNEWT_VAL(ADC_0)
#include "adc/adc.h"
#include "adc_nrf52/adc_nrf52.h"
#endif
#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2) || MYNEWT_VAL(PWM_3)
#include "pwm/pwm.h"
#include "pwm_nrf52/pwm_nrf52.h"
#endif
#if MYNEWT_VAL(TRNG)
#include "trng/trng.h"
#include "trng_nrf52/trng_nrf52.h"
#endif
#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1)
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
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

#if MYNEWT_VAL(TRNG)
static struct trng_dev os_bsp_trng;
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf52_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
static const struct nrf52_uart_cfg os_bsp_uart1_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_1_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_1_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_1_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_1_PIN_CTS),
};
#endif

#if MYNEWT_VAL(I2C_0)
static const struct nrf52_hal_i2c_cfg hal_i2c0_cfg = {
    .scl_pin = MYNEWT_VAL(I2C_0_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_0_PIN_SDA),
    .i2c_frequency = MYNEWT_VAL(I2C_0_FREQ_KHZ),
};
#endif
#if MYNEWT_VAL(I2C_1)
static const struct nrf52_hal_i2c_cfg hal_i2c1_cfg = {
    .scl_pin = MYNEWT_VAL(I2C_1_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_1_PIN_SDA),
    .i2c_frequency = MYNEWT_VAL(I2C_1_FREQ_KHZ),
};
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
static const struct nrf52_hal_spi_cfg os_bsp_spi0m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_0_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_0_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_0_MASTER_PIN_MISO),
    /* For SPI master, SS pin is controlled as regular GPIO */
};
#endif
#if MYNEWT_VAL(SPI_0_SLAVE)
static const struct nrf52_hal_spi_cfg os_bsp_spi0s_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_0_SLAVE_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_0_SLAVE_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_0_SLAVE_PIN_MISO),
    .ss_pin       = MYNEWT_VAL(SPI_0_SLAVE_PIN_SS),
};
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
static const struct nrf52_hal_spi_cfg os_bsp_spi1m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_1_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
    /* For SPI master, SS pin is controlled as regular GPIO */
};
#endif
#if MYNEWT_VAL(SPI_1_SLAVE)
static const struct nrf52_hal_spi_cfg os_bsp_spi1s_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_1_SLAVE_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_1_SLAVE_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_1_SLAVE_PIN_MISO),
    .ss_pin       = MYNEWT_VAL(SPI_1_SLAVE_PIN_SS),
};
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
static const struct nrf52_hal_spi_cfg os_bsp_spi2m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_2_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_2_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_2_MASTER_PIN_MISO),
    /* For SPI master, SS pin is controlled as regular GPIO */
};
#endif
#if MYNEWT_VAL(SPI_2_SLAVE)
static const struct nrf52_hal_spi_cfg os_bsp_spi2s_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_2_SLAVE_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_2_SLAVE_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_2_SLAVE_PIN_MISO),
    .ss_pin       = MYNEWT_VAL(SPI_2_SLAVE_PIN_SS),
};
#endif

static void
nrf52_periph_create_timers(void)
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
#if MYNEWT_VAL(TIMER_5)
    rc = hal_timer_init(5, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
nrf52_periph_create_adc(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(ADC_0)
    rc = os_dev_create(&os_bsp_adc0.ad_dev, "adc0",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       nrf52_adc_dev_init, &os_bsp_adc0_config);
    assert(rc == 0);
#endif
}

static void
nrf52_periph_create_pwm(void)
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
nrf52_periph_create_trng(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TRNG)
    rc = os_dev_create(&os_bsp_trng.dev, "trng",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       nrf52_trng_dev_init, NULL);
    assert(rc == 0);
#endif
}

static void
nrf52_periph_create_uart(void)
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
}

static void
nrf52_periph_create_i2c(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, (void *)&hal_i2c0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_1)
    rc = hal_i2c_init(1, (void *)&hal_i2c1_cfg);
    assert(rc == 0);
#endif
}

static void
nrf52_periph_create_spi(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, (void *)&os_bsp_spi0m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, (void *)&os_bsp_spi0s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    rc = hal_spi_init(1, (void *)&os_bsp_spi1m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_1_SLAVE)
    rc = hal_spi_init(1, (void *)&os_bsp_spi1s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    rc = hal_spi_init(2, (void *)&os_bsp_spi2m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_SLAVE)
    rc = hal_spi_init(2, (void *)&os_bsp_spi2s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
}

void
nrf52_periph_create(void)
{
    nrf52_periph_create_timers();
    nrf52_periph_create_adc();
    nrf52_periph_create_pwm();
    nrf52_periph_create_trng();
    nrf52_periph_create_uart();
    nrf52_periph_create_i2c();
    nrf52_periph_create_spi();
}
