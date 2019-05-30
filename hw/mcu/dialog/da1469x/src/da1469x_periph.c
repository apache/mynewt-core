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
#include "syscfg/syscfg.h"
#include "hal/hal_timer.h"
#include "os/os_cputime.h"
#include "mcu/da1469x_hal.h"
#include "mcu/da1469x_dma.h"
#include "bsp/bsp.h"

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2)
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#endif
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/bus.h"
#if MYNEWT_VAL(I2C_0) || MYNEWT_VAL(I2C_1)
#include "bus/drivers/i2c_hal.h"
#endif
#else
#if MYNEWT_VAL(I2C_0) || MYNEWT_VAL(I2C_1)
#include "hal/hal_i2c.h"
#endif
#endif
#if MYNEWT_VAL(TRNG)
#include "trng/trng.h"
#include "trng_da1469x.h"
#endif
#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2)
#include "pwm/pwm.h"
#include "pwm_da1469x/pwm_da1469x.h"
#endif

#if MYNEWT_VAL(TRNG)
static struct trng_dev os_bsp_trng;
#endif

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/spi_hal.h"
#else
#include "hal/hal_spi.h"
#endif
#endif

#if MYNEWT_VAL(SPI_0_SLAVE) || MYNEWT_VAL(SPI_1_SLAVE)
#include "hal/hal_spi.h"
#endif

#if MYNEWT_VAL(GPADC)
#include <gpadc_da1469x/gpadc_da1469x.h>
#endif
#if MYNEWT_VAL(SDADC)
#include <sdadc_da1469x/sdadc_da1469x.h>
#endif

#if MYNEWT_VAL(CHARGER)
#include "da1469x_charger/da1469x_charger.h"
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct da1469x_uart_cfg os_bsp_uart0_cfg = {
    .pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .pin_rts = -1,
    .pin_cts = -1,
};
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
static const struct da1469x_uart_cfg os_bsp_uart1_cfg = {
    .pin_tx = MYNEWT_VAL(UART_1_PIN_TX),
    .pin_rx = MYNEWT_VAL(UART_1_PIN_RX),
    .pin_rts = MYNEWT_VAL(UART_1_PIN_RTS),
    .pin_cts = MYNEWT_VAL(UART_1_PIN_CTS),
};
#endif
#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart2;
static const struct da1469x_uart_cfg os_bsp_uart2_cfg = {
    .pin_tx = MYNEWT_VAL(UART_2_PIN_TX),
    .pin_rx = MYNEWT_VAL(UART_2_PIN_RX),
    .pin_rts = MYNEWT_VAL(UART_2_PIN_RTS),
    .pin_cts = MYNEWT_VAL(UART_2_PIN_CTS),
};
#endif

#if MYNEWT_VAL(I2C_0)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_i2c_dev_cfg i2c0_cfg = {
    .i2c_num = 0,
    .pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
};
static struct bus_i2c_dev i2c0_bus;
#else
static const struct da1469x_hal_i2c_cfg hal_i2c0_cfg = {
    .pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .frequency = MYNEWT_VAL(I2C_0_FREQ_KHZ),
};
#endif
#endif
#if MYNEWT_VAL(I2C_1)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_i2c_dev_cfg i2c1_cfg = {
    .i2c_num = 1,
    .pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
};
static struct bus_i2c_dev i2c1_bus;
#else
static const struct da1469x_hal_i2c_cfg hal_i2c1_cfg = {
    .pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
    .frequency = MYNEWT_VAL(I2C_1_FREQ_KHZ),
};
#endif
#endif
#if MYNEWT_VAL(GPADC)
static struct da1469x_gpadc_dev os_bsp_gpadc;
static struct da1469x_gpadc_init_cfg os_bsp_gpadc_cfg = {
    .dgic_adc_clk_div = MYNEWT_VAL(GPADC_CLK_DIV),
    .dgic_dma_cidx = MYNEWT_VAL(GPADC_DMA_CIDX),
    .dgic_dma_prio = MYNEWT_VAL(GPADC_DMA_PRIO)
};
#endif
#if MYNEWT_VAL(SDADC)
static struct da1469x_sdadc_dev os_bsp_sdadc;
static struct da1469x_sdadc_init_cfg os_bsp_sdadc_cfg = {
    .dsic_dma_cidx = MYNEWT_VAL(SDADC_DMA_CIDX),
    .dsic_dma_prio = MYNEWT_VAL(SDADC_DMA_PRIO)
};
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_spi_dev_cfg spi0_cfg = {
    .spi_num = 0,
    .pin_sck = MYNEWT_VAL(SPI_0_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_0_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_0_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi0_bus;
#else
static const struct da1469x_hal_spi_cfg hal_spi0_cfg = {
    .pin_sck = MYNEWT_VAL(SPI_0_MASTER_PIN_SCK),
    .pin_do = MYNEWT_VAL(SPI_0_MASTER_PIN_MOSI),
    .pin_di = MYNEWT_VAL(SPI_0_MASTER_PIN_MISO),
};
#endif
#elif MYNEWT_VAL(SPI_0_SLAVE)
static const struct da1469x_hal_spi_cfg hal_spi0_cfg = {
    .pin_sck = MYNEWT_VAL(SPI_0_SLAVE_PIN_SCK),
    .pin_do = MYNEWT_VAL(SPI_0_SLAVE_PIN_MISO),
    .pin_di = MYNEWT_VAL(SPI_0_SLAVE_PIN_MOSI),
};
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_spi_dev_cfg spi1_cfg = {
    .spi_num = 1,
    .pin_sck = MYNEWT_VAL(SPI_1_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi1_bus;
#else
static const struct da1469x_hal_spi_cfg hal_spi1_cfg = {
    .pin_sck = MYNEWT_VAL(SPI_1_MASTER_PIN_SCK),
    .pin_do = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
    .pin_di = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
};
#endif
#elif MYNEWT_VAL(SPI_1_SLAVE)
static const struct da1469x_hal_spi_cfg hal_spi1_cfg = {
    .pin_sck = MYNEWT_VAL(SPI_1_SLAVE_PIN_SCK),
    .pin_do = MYNEWT_VAL(SPI_1_SLAVE_PIN_MISO),
    .pin_di = MYNEWT_VAL(SPI_1_SLAVE_PIN_MOSI),
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

#if MYNEWT_VAL(CHARGER)
struct da1469x_charger_dev da1469x_charger_dev;
#endif

static void
da1469x_periph_create_timers(void)
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

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
da1469x_periph_create_pwm()
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(PWM_0)
    rc = os_dev_create(&os_bsp_pwm0.pwm_os_dev, "pwm0",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       da1469x_pwm_init, UINT_TO_POINTER(0));
    assert(rc == 0);
#endif

#if MYNEWT_VAL(PWM_1)
    rc = os_dev_create(&os_bsp_pwm1.pwm_os_dev, "pwm1",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       da1469x_pwm_init, UINT_TO_POINTER(1));
    assert(rc == 0);
#endif

#if MYNEWT_VAL(PWM_2)
    rc = os_dev_create(&os_bsp_pwm2.pwm_os_dev, "pwm2",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       da1469x_pwm_init, UINT_TO_POINTER(2));
    assert(rc == 0);
#endif
}

static void
da1469x_periph_create_trng(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TRNG)
    rc = os_dev_create(&os_bsp_trng.dev, "trng",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       da1469x_trng_init, NULL);
    assert(rc == 0);
#endif
}

static void
da1469x_periph_create_adc(void)
{
    int rc;

    (void)rc;
#if MYNEWT_VAL(GPADC)
    rc = os_dev_create(&os_bsp_gpadc.dgd_adc.ad_dev, "gpadc",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       da1469x_gpadc_init, &os_bsp_gpadc_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SDADC)
    rc = os_dev_create(&os_bsp_sdadc.dsd_adc.ad_dev, "sdadc",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       da1469x_sdadc_init, &os_bsp_sdadc_cfg);
    assert(rc == 0);
#endif
}

static void
da1469x_periph_create_uart(void)
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
    rc = os_dev_create(&os_bsp_uart1.ud_dev, "uart2",
                       OS_DEV_INIT_PRIMARY, 2, uart_hal_init,
                       (void *)&os_bsp_uart2_cfg);
    assert(rc == 0);
#endif
}

static void
da1469x_periph_create_i2c(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(I2C_0)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_i2c_hal_dev_create("i2c0", &i2c0_bus,
                                (struct bus_i2c_dev_cfg *)&i2c0_cfg);
    assert(rc == 0);
#else
    rc = hal_i2c_init(0, (void *)&hal_i2c0_cfg);
    assert(rc == 0);
#endif
#endif

#if MYNEWT_VAL(I2C_1)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_i2c_hal_dev_create("i2c1", &i2c1_bus,
                                (struct bus_i2c_dev_cfg *)&i2c1_cfg);
    assert(rc == 0);
#else
    rc = hal_i2c_init(1, (void *)&hal_i2c1_cfg);
    assert(rc == 0);
#endif
#endif
}

static void
da1469x_periph_create_spi(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(SPI_0_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_hal_dev_create("spi0", &spi0_bus,
                                (struct bus_spi_dev_cfg *)&spi0_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(0, (void *)&hal_spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_hal_dev_create("spi1", &spi1_bus,
                                (struct bus_spi_dev_cfg *)&spi1_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(1, (void *)&hal_spi1_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, (void *)&hal_spi0_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_1_SLAVE)
    rc = hal_spi_init(0, (void *)&hal_spi1_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
}

#if MYNEWT_VAL(CHARGER)
struct da1469x_charger_config cfg = {
    .ctrl = 63U << CHARGER_CHARGER_CTRL_REG_EOC_INTERVAL_CHECK_THRES_Pos |
            1U << CHARGER_CHARGER_CTRL_REG_PRE_CHARGE_MODE_Pos |
            1U << CHARGER_CHARGER_CTRL_REG_CHARGE_LOOP_HOLD_Pos |
            (MYNEWT_VAL(DA1469X_CHARGER_TBAT_MONITOR_MODE)) <<
                CHARGER_CHARGER_CTRL_REG_TBAT_MONITOR_MODE_Pos |
            1U << CHARGER_CHARGER_CTRL_REG_CHARGE_TIMERS_HALT_ENABLE_Pos |
            (MYNEWT_VAL(DA1469X_CHARGER_NTC_ENABLE)) <<
                CHARGER_CHARGER_CTRL_REG_TBAT_PROT_ENABLE_Pos |
            1U << CHARGER_CHARGER_CTRL_REG_TDIE_PROT_ENABLE_Pos |
            1U << CHARGER_CHARGER_CTRL_REG_CHARGER_RESUME_Pos,
    .ctrl_valid = 1,
    .voltage_param =
        DA1469X_ENCODE_V(MYNEWT_VAL(DA1469X_CHARGER_V_OVP)) <<
            CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_OVP_Pos |
        DA1469X_ENCODE_V(MYNEWT_VAL(DA1469X_CHARGER_V_REPLENISH)) <<
            CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_REPLENISH_Pos |
        DA1469X_ENCODE_V(MYNEWT_VAL(DA1469X_CHARGER_V_PRECHARGE)) <<
            CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_PRECHARGE_Pos |
        DA1469X_ENCODE_V(MYNEWT_VAL(DA1469X_CHARGER_V_CHARGE)) <<
            CHARGER_CHARGER_VOLTAGE_PARAM_REG_V_CHARGE_Pos,
    .voltage_param_valid = 1,
    .current_param =
        DA1469X_ENCODE_PRECHG_I(MYNEWT_VAL(DA1469X_CHARGER_I_PRECHARGE)) |
        DA1469X_ENCODE_CHG_I(MYNEWT_VAL(DA1469X_CHARGER_I_CHARGE)) |
        DA1469X_ENCODE_EOC_I(MYNEWT_VAL(DA1469X_CHARGER_I_END_OF_CHARGE)),
    .current_param_valid = 1,
};
#endif

void
da1469x_periph_create_charger(void)
{
#if MYNEWT_VAL(CHARGER)
    int rc;

    rc = da1469x_charger_create(&da1469x_charger_dev, "charger", &cfg);

    assert(rc == 0);
#endif
}

void
da1469x_periph_create(void)
{
    da1469x_dma_init();

    da1469x_periph_create_timers();
    da1469x_periph_create_adc();
    da1469x_periph_create_pwm();
    da1469x_periph_create_trng();
    da1469x_periph_create_uart();
    da1469x_periph_create_i2c();
    da1469x_periph_create_spi();
    da1469x_periph_create_charger();
}
