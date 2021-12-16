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
#include "hal/hal_timer.h"
#include "hal/hal_i2c.h"
#include "hal/hal_spi.h"
#include "bsp/bsp.h"
#if MYNEWT_VAL(TRNG)
#include "trng/trng.h"
#include "trng_stm32/trng_stm32.h"
#endif
#if MYNEWT_VAL(CRYPTO)
#include "crypto/crypto.h"
#include "crypto_stm32/crypto_stm32.h"
#endif
#if MYNEWT_VAL(HASH)
#include "hash/hash.h"
#include "hash_stm32/hash_stm32.h"
#endif
#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2) || MYNEWT_VAL(UART_3) || MYNEWT_VAL(UART_4) || \
    MYNEWT_VAL(UART_5) || MYNEWT_VAL(UART_6) || MYNEWT_VAL(UART_7) || MYNEWT_VAL(UART_8) || MYNEWT_VAL(UART_9)
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#endif
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/bus.h"
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_3_MASTER) || \
    MYNEWT_VAL(SPI_4_MASTER) || MYNEWT_VAL(SPI_5_MASTER)
#include "bus/drivers/spi_stm32.h"
#endif
#endif
#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2)
#include <pwm_stm32/pwm_stm32.h>
#endif
#if MYNEWT_VAL(ETH_0)
#include <stm32_eth/stm32_eth.h>
#include <stm32_eth/stm32_eth_cfg.h>
#endif
#if MYNEWT_VAL(ADC_0) || MYNEWT_VAL(ADC_1) || MYNEWT_VAL(ADC_2)
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_adc.h"
#include "adc_stm32f4/adc_stm32f4.h"
#endif
#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
#include "os/os_cputime.h"
#endif

#include "mcu/stm32_hal.h"

#if MYNEWT_VAL(PWM_0)
static struct pwm_dev os_bsp_pwm0;
extern struct stm32_pwm_conf os_bsp_pwm0_cfg;
#endif
#if MYNEWT_VAL(PWM_1)
static struct pwm_dev os_bsp_pwm1;
extern struct stm32_pwm_conf os_bsp_pwm1_cfg;
#endif
#if MYNEWT_VAL(PWM_2)
static struct pwm_dev os_bsp_pwm2;
extern struct stm32_pwm_conf os_bsp_pwm2_cfg;
#endif

#if MYNEWT_VAL(TRNG)
static struct trng_dev os_bsp_trng;
#endif

#if MYNEWT_VAL(CRYPTO)
static struct crypto_dev os_bsp_crypto;
#endif

#if MYNEWT_VAL(HASH)
static struct hash_dev os_bsp_hash;
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
extern const struct stm32_uart_cfg os_bsp_uart0_cfg;
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
extern const struct stm32_uart_cfg os_bsp_uart1_cfg;
#endif
#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart2;
extern const struct stm32_uart_cfg os_bsp_uart2_cfg;
#endif
#if MYNEWT_VAL(UART_3)
static struct uart_dev os_bsp_uart3;
extern const struct stm32_uart_cfg os_bsp_uart3_cfg;
#endif
#if MYNEWT_VAL(UART_4)
static struct uart_dev os_bsp_uart4;
extern const struct stm32_uart_cfg os_bsp_uart4_cfg;
#endif
#if MYNEWT_VAL(UART_5)
static struct uart_dev os_bsp_uart5;
extern const struct stm32_uart_cfg os_bsp_uart5_cfg;
#endif
#if MYNEWT_VAL(UART_6)
static struct uart_dev os_bsp_uart6;
extern const struct stm32_uart_cfg os_bsp_uart6_cfg;
#endif
#if MYNEWT_VAL(UART_7)
static struct uart_dev os_bsp_uart7;
extern const struct stm32_uart_cfg os_bsp_uart7_cfg;
#endif
#if MYNEWT_VAL(UART_8)
static struct uart_dev os_bsp_uart8;
extern const struct stm32_uart_cfg os_bsp_uart7_cfg;
#endif
#if MYNEWT_VAL(UART_9)
static struct uart_dev os_bsp_uart9;
extern const struct stm32_uart_cfg os_bsp_uart7_cfg;
#endif

#if MYNEWT_VAL(ADC_0)
static struct adc_dev os_bsp_adc0;
extern struct stm32f4_adc_dev_cfg os_bsp_adc0_cfg;
#endif
#if MYNEWT_VAL(ADC_1)
static struct adc_dev os_bsp_adc1;
extern struct stm32f4_adc_dev_cfg os_bsp_adc1_cfg;
#endif
#if MYNEWT_VAL(ADC_2)
static struct adc_dev os_bsp_adc2;
extern struct stm32f4_adc_dev_cfg os_bsp_adc2_cfg;
#endif

#if MYNEWT_VAL(I2C_0)
extern const struct stm32_hal_i2c_cfg os_bsp_i2c0_cfg;
#endif
#if MYNEWT_VAL(I2C_1)
extern const struct stm32_hal_i2c_cfg os_bsp_i2c1_cfg;
#endif
#if MYNEWT_VAL(I2C_2)
extern const struct stm32_hal_i2c_cfg os_bsp_i2c2_cfg;
#endif
#if MYNEWT_VAL(I2C_3)
extern const struct stm32_hal_i2c_cfg os_bsp_i2c3_cfg;
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

#if MYNEWT_VAL(SPI_0_MASTER)
static const struct bus_spi_dev_cfg spi0_cfg = {
    .spi_num = 0,
    .pin_sck = MYNEWT_VAL(SPI_0_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_0_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_0_PIN_MISO),
};
static struct bus_spi_dev spi0_bus;
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
static const struct bus_spi_dev_cfg spi1_cfg = {
    .spi_num = 1,
    .pin_sck = MYNEWT_VAL(SPI_1_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_1_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_1_PIN_MISO),
};
static struct bus_spi_dev spi1_bus;
#endif

#if MYNEWT_VAL(SPI_2_MASTER)
static const struct bus_spi_dev_cfg spi2_cfg = {
    .spi_num = 2,
    .pin_sck = MYNEWT_VAL(SPI_2_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_2_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_2_PIN_MISO),
};
static struct bus_spi_dev spi2_bus;
#endif

#if MYNEWT_VAL(SPI_3_MASTER)
static const struct bus_spi_dev_cfg spi3_cfg = {
    .spi_num = 3,
    .pin_sck = MYNEWT_VAL(SPI_3_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_3_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_3_PIN_MISO),
};
static struct bus_spi_dev spi3_bus;
#endif

#if MYNEWT_VAL(SPI_4_MASTER)
static const struct bus_spi_dev_cfg spi4_cfg = {
    .spi_num = 4,
    .pin_sck = MYNEWT_VAL(SPI_4_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_4_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_4_PIN_MISO),
};
static struct bus_spi_dev spi4_bus;
#endif

#if MYNEWT_VAL(SPI_5_MASTER)
static const struct bus_spi_dev_cfg spi5_cfg = {
    .spi_num = 5,
    .pin_sck = MYNEWT_VAL(SPI_5_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_5_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_5_PIN_MISO),
};
static struct bus_spi_dev spi5_bus;
#endif

#else
#if (MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE))
#if MYNEWT_VAL(SPI_0_CUSTOM_CFG)
extern const struct stm32_hal_spi_cfg os_bsp_spi0_cfg;
#else
static const struct stm32_hal_spi_cfg os_bsp_spi0_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_0_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_0_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_0_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_0_PIN_SS),
    .irq_prio = 2,
};
#endif
#endif
#if (MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_1_SLAVE))
#if MYNEWT_VAL(SPI_1_CUSTOM_CFG)
extern const struct stm32_hal_spi_cfg os_bsp_spi1_cfg;
#else
static const struct stm32_hal_spi_cfg os_bsp_spi1_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_1_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_1_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_1_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_1_PIN_SS),
    .irq_prio = 2,
};
#endif
#endif
#if (MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_2_SLAVE))
#if MYNEWT_VAL(SPI_2_CUSTOM_CFG)
extern const struct stm32_hal_spi_cfg os_bsp_spi2_cfg;
#else
static const struct stm32_hal_spi_cfg os_bsp_spi2_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_2_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_2_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_2_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_2_PIN_SS),
    .irq_prio = 2,
};
#endif
#endif
#if (MYNEWT_VAL(SPI_3_MASTER) || MYNEWT_VAL(SPI_3_SLAVE))
#if MYNEWT_VAL(SPI_3_CUSTOM_CFG)
extern const struct stm32_hal_spi_cfg os_bsp_spi3_cfg;
#else
static const struct stm32_hal_spi_cfg os_bsp_spi3_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_3_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_3_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_3_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_3_PIN_SS),
    .irq_prio = 2,
};
#endif
#endif
#if (MYNEWT_VAL(SPI_4_MASTER) || MYNEWT_VAL(SPI_4_SLAVE))
#if MYNEWT_VAL(SPI_4_CUSTOM_CFG)
extern const struct stm32_hal_spi_cfg os_bsp_spi4_cfg;
#else
static const struct stm32_hal_spi_cfg os_bsp_spi4_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_4_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_4_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_4_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_4_PIN_SS),
    .irq_prio = 2,
};
#endif
#endif
#if (MYNEWT_VAL(SPI_5_MASTER) || MYNEWT_VAL(SPI_5_SLAVE))
#if MYNEWT_VAL(SPI_5_CUSTOM_CFG)
extern const struct stm32_hal_spi_cfg os_bsp_spi5_cfg;
#else
static const struct stm32_hal_spi_cfg os_bsp_spi5_cfg = {
    .sck_pin = MYNEWT_VAL(SPI_5_PIN_SCK),
    .mosi_pin = MYNEWT_VAL(SPI_5_PIN_MOSI),
    .miso_pin = MYNEWT_VAL(SPI_5_PIN_MISO),
    .ss_pin = MYNEWT_VAL(SPI_5_PIN_SS),
    .irq_prio = 2,
};
#endif
#endif
#endif

#if MYNEWT_VAL(ETH_0)
extern const struct stm32_eth_cfg os_bsp_eth0_cfg;
#endif

static void
stm32_periph_create_timers(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, MYNEWT_VAL(TIMER_0_TIM));
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, MYNEWT_VAL(TIMER_1_TIM));
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, MYNEWT_VAL(TIMER_2_TIM));
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_3)
    rc = hal_timer_init(3, MYNEWT_VAL(TIMER_3_TIM));
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_4)
    rc = hal_timer_init(4, MYNEWT_VAL(TIMER_4_TIM));
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_5)
    rc = hal_timer_init(5, MYNEWT_VAL(TIMER_5_TIM));
    assert(rc == 0);
#endif
#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
stm32_periph_create_pwm(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(PWM_0)
    rc = os_dev_create(&os_bsp_pwm0.pwm_os_dev, "pwm0",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       stm32_pwm_dev_init, &os_bsp_pwm0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(PWM_1)
    rc = os_dev_create(&os_bsp_pwm1.pwm_os_dev, "pwm1",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       stm32_pwm_dev_init, &os_bsp_pwm1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(PWM_2)
    rc = os_dev_create(&os_bsp_pwm2.pwm_os_dev, "pwm2",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       stm32_pwm_dev_init, &os_bsp_pwm2_cfg);
    assert(rc == 0);
#endif
}

static void
stm32_periph_create_trng(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TRNG)
    rc = os_dev_create(&os_bsp_trng.dev, "trng",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       stm32_trng_dev_init, NULL);
    assert(rc == 0);
#endif
}

static void
stm32_periph_create_crypto(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(CRYPTO)
    rc = os_dev_create(&os_bsp_crypto.dev, "crypto",
                       OS_DEV_INIT_PRIMARY, OS_DEV_INIT_PRIO_DEFAULT,
                       stm32_crypto_dev_init, NULL);
    assert(rc == 0);
#endif
}

static void
stm32_periph_create_hash(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(HASH)
    rc = os_dev_create(&os_bsp_hash.dev, "hash", OS_DEV_INIT_KERNEL,
                       OS_DEV_INIT_PRIO_DEFAULT, stm32_hash_dev_init, NULL);
    assert(rc == 0);
#endif
}

static void
stm32_periph_create_uart(void)
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
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart2_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_3)
    rc = os_dev_create(&os_bsp_uart3.ud_dev, "uart3",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart3_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_4)
    rc = os_dev_create(&os_bsp_uart4.ud_dev, "uart4",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart4_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_5)
    rc = os_dev_create(&os_bsp_uart5.ud_dev, "uart5",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart5_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_6)
    rc = os_dev_create(&os_bsp_uart6.ud_dev, "uart6",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart6_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_7)
    rc = os_dev_create(&os_bsp_uart7.ud_dev, "uart7",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart7_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_8)
    rc = os_dev_create(&os_bsp_uart8.ud_dev, "uart8",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart8_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_9)
    rc = os_dev_create(&os_bsp_uart9.ud_dev, "uart9",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart9_cfg);
    assert(rc == 0);
#endif
}

static void
stm32_periph_create_i2c(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, (void *)&os_bsp_i2c0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_1)
    rc = hal_i2c_init(1, (void *)&os_bsp_i2c1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_2)
    rc = hal_i2c_init(2, (void *)&os_bsp_i2c2_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_3)
    rc = hal_i2c_init(3, (void *)&os_bsp_i2c3_cfg);
    assert(rc == 0);
#endif
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
stm32_periph_create_spi(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = bus_spi_stm32_dev_create("spi0", &spi0_bus,
                                  (struct bus_spi_dev_cfg *)&spi0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    rc = bus_spi_stm32_dev_create("spi1", &spi1_bus,
                                  (struct bus_spi_dev_cfg *)&spi1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    rc = bus_spi_stm32_dev_create("spi2", &spi2_bus,
                                  (struct bus_spi_dev_cfg *)&spi2_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
    rc = bus_spi_stm32_dev_create("spi3", &spi3_bus,
                                  (struct bus_spi_dev_cfg *)&spi3_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
    rc = bus_spi_stm32_dev_create("spi4", &spi4_bus,
                                  (struct bus_spi_dev_cfg *)&spi4_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
    rc = bus_spi_stm32_dev_create("spi5", &spi5_bus,
                                  (struct bus_spi_dev_cfg *)&spi5_cfg);
    assert(rc == 0);
#endif
}

#else
static void
stm32_periph_create_spi(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, (void *)&os_bsp_spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, (void *)&os_bsp_spi0_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    rc = hal_spi_init(1, (void *)&os_bsp_spi1_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_1_SLAVE)
    rc = hal_spi_init(1, (void *)&os_bsp_spi1_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    rc = hal_spi_init(2, (void *)&os_bsp_spi2_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_SLAVE)
    rc = hal_spi_init(2, (void *)&os_bsp_spi2_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
    rc = hal_spi_init(3, (void *)&os_bsp_spi3_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_3_SLAVE)
    rc = hal_spi_init(3, (void *)&os_bsp_spi3_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
    rc = hal_spi_init(4, (void *)&os_bsp_spi4_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_4_SLAVE)
    rc = hal_spi_init(4, (void *)&os_bsp_spi4_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
    rc = hal_spi_init(5, (void *)&os_bsp_spi5_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_5_SLAVE)
    rc = hal_spi_init(5, (void *)&os_bsp_spi5_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
}
#endif

static void
stm32_periph_create_adc(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(ADC_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_adc0, "adc0",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       stm32f4_adc_dev_init, &os_bsp_adc0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(ADC_1)
    rc = os_dev_create((struct os_dev *) &os_bsp_adc1, "adc1",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       stm32f4_adc_dev_init, &os_bsp_adc1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(ADC_2)
    rc = os_dev_create((struct os_dev *) &os_bsp_adc2, "adc2",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       stm32f4_adc_dev_init, &os_bsp_adc2_cfg);
    assert(rc == 0);
#endif
}

static void
stm32_periph_create_eth(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(ETH_0)
    rc = stm32_eth_init(&os_bsp_eth0_cfg);
    assert(rc == 0);
#endif
}

void
stm32_periph_create(void)
{
    stm32_periph_create_timers();
    stm32_periph_create_pwm();
    stm32_periph_create_trng();
    stm32_periph_create_crypto();
    stm32_periph_create_hash();
    stm32_periph_create_uart();
    stm32_periph_create_i2c();
    stm32_periph_create_spi();
    stm32_periph_create_adc();
    stm32_periph_create_eth();
}
