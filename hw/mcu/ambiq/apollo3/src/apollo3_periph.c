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
#include "mcu/hal_apollo3.h"
#include "hal/hal_i2c.h"
#include "hal/hal_spi.h"
#include "bsp/bsp.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/bus.h"
#if MYNEWT_VAL(I2C_0) || MYNEWT_VAL(I2C_1) || MYNEWT_VAL(I2C_2) || MYNEWT_VAL(I2C_3) || MYNEWT_VAL(I2C_4) || MYNEWT_VAL(I2C_5)
#include "bus/drivers/i2c_hal.h"
#endif
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_3_MASTER) || MYNEWT_VAL(SPI_4_MASTER) || MYNEWT_VAL(SPI_5_MASTER)
#include "bus/drivers/spi_apollo3.h"
#endif
#endif
#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1)
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#endif
#if MYNEWT_VAL(ADC_0)
#include <adc/adc.h>
#include <adc_apollo3/adc_apollo3.h>
#endif

#if MYNEWT_VAL(ADC_0)
#define ADC_SAMPLE_BUF_SIZE 128
static uint32_t g_adc_sample_buffer[ADC_SAMPLE_BUF_SIZE];

static struct adc_dev os_bsp_adc0;
static struct adc_cfg os_bsp_adc0_config = {
    .adc_cfg = {
        .eClock             = AM_HAL_ADC_CLKSEL_HFRC,
        .ePolarity          = AM_HAL_ADC_TRIGPOL_RISING,
        .eTrigger           = AM_HAL_ADC_TRIGSEL_SOFTWARE,
        .eReference         = AM_HAL_ADC_REFSEL_INT_1P5,
        .eClockMode         = AM_HAL_ADC_CLKMODE_LOW_LATENCY,
        .ePowerMode         = AM_HAL_ADC_LPMODE0,
        .eRepeat            = AM_HAL_ADC_REPEATING_SCAN,
    },
    .adc_slot_cfg = {
        .eMeasToAvg      = AM_HAL_ADC_SLOT_AVG_128,
        .ePrecisionMode  = AM_HAL_ADC_SLOT_14BIT,
        .eChannel        = AM_HAL_ADC_SLOT_CHSEL_SE0,
        .bWindowCompare  = false,
        .bEnabled        = true,
    },
    .adc_dma_cfg = {
        .bDynamicPriority = true,
        .ePriority = AM_HAL_ADC_PRIOR_SERVICE_IMMED,
        .bDMAEnable = true,
        .ui32SampleCount = ADC_SAMPLE_BUF_SIZE,
        .ui32TargetAddress = (uint32_t)g_adc_sample_buffer
    },
    .clk_cfg = {
        .clk_freq = 12000000,
        .clk_period = 10,
        .clk_on_time = 5,
        .clk_num = APOLLO3_ADC_CLOCK_3,
        .timer_ab = APOLLO3_ADC_TIMER_A,
        .timer_func = APOLLO3_ADC_TIMER_FUNC_REPEAT,
    }
};
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct apollo3_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
static const struct apollo3_uart_cfg os_bsp_uart1_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_1_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_1_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_1_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_1_PIN_CTS),
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
static const struct apollo3_hal_i2c_cfg hal_i2c0_cfg = {
    .scl_pin = MYNEWT_VAL(I2C_0_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_0_PIN_SDA),
    .i2c_frequency = MYNEWT_VAL(I2C_0_FREQ_KHZ),
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
static const struct apollo3_hal_i2c_cfg hal_i2c1_cfg = {
    .scl_pin = MYNEWT_VAL(I2C_1_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_1_PIN_SDA),
    .i2c_frequency = MYNEWT_VAL(I2C_1_FREQ_KHZ),
};
#endif
#endif
#if MYNEWT_VAL(I2C_2)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_i2c_dev_cfg i2c2_cfg = {
    .i2c_num = 2,
    .pin_sda = MYNEWT_VAL(I2C_2_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_2_PIN_SCL),
};
static struct bus_i2c_dev i2c2_bus;
#else
static const struct apollo3_hal_i2c_cfg hal_i2c2_cfg = {
    .scl_pin = MYNEWT_VAL(I2C_2_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_2_PIN_SDA),
    .i2c_frequency = MYNEWT_VAL(I2C_2_FREQ_KHZ),
};
#endif
#endif
#if MYNEWT_VAL(I2C_3)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_i2c_dev_cfg i2c3_cfg = {
    .i2c_num = 3,
    .pin_sda = MYNEWT_VAL(I2C_3_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_3_PIN_SCL),
};
static struct bus_i2c_dev i2c3_bus;
#else
static const struct apollo3_hal_i2c_cfg hal_i2c3_cfg = {
    .scl_pin = MYNEWT_VAL(I2C_3_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_3_PIN_SDA),
    .i2c_frequency = MYNEWT_VAL(I2C_3_FREQ_KHZ),
};
#endif
#endif
#if MYNEWT_VAL(I2C_4)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_i2c_dev_cfg i2c4_cfg = {
    .i2c_num = 4,
    .pin_sda = MYNEWT_VAL(I2C_4_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_4_PIN_SCL),
};
static struct bus_i2c_dev i2c4_bus;
#else
static const struct apollo3_hal_i2c_cfg hal_i2c4_cfg = {
    .scl_pin = MYNEWT_VAL(I2C_4_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_4_PIN_SDA),
    .i2c_frequency = MYNEWT_VAL(I2C_4_FREQ_KHZ),
};
#endif
#endif
#if MYNEWT_VAL(I2C_5)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_i2c_dev_cfg i2c5_cfg = {
    .i2c_num = 5,
    .pin_sda = MYNEWT_VAL(I2C_5_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_5_PIN_SCL),
};
static struct bus_i2c_dev i2c5_bus;
#else
static const struct apollo3_hal_i2c_cfg hal_i2c5_cfg = {
    .scl_pin = MYNEWT_VAL(I2C_5_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_5_PIN_SDA),
    .i2c_frequency = MYNEWT_VAL(I2C_5_FREQ_KHZ),
};
#endif
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
static const struct apollo3_spi_cfg os_bsp_spi0s_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_0_SLAVE_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_0_SLAVE_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_0_SLAVE_PIN_MISO),
    .ss_pin       = { MYNEWT_VAL(SPI_0_SLAVE_PIN_CS), -1, -1, -1 }
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
static struct bus_spi_apollo3_dev spi0_bus;
#else
static const struct apollo3_spi_cfg os_bsp_spi0m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_0_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_0_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_0_MASTER_PIN_MISO),
    .ss_pin       = { MYNEWT_VAL(SPI_0_MASTER_PIN_CS),
                      MYNEWT_VAL(SPI_0_MASTER_PIN_CS1),
                      MYNEWT_VAL(SPI_0_MASTER_PIN_CS2),
                      MYNEWT_VAL(SPI_0_MASTER_PIN_CS3) }
};
#endif
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_spi_dev_cfg spi1_cfg = {
    .spi_num = 1,
    .pin_sck = MYNEWT_VAL(SPI_1_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
};
static struct bus_spi_apollo3_dev spi1_bus;
#else
static const struct apollo3_spi_cfg os_bsp_spi1m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_1_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
    .ss_pin       = { MYNEWT_VAL(SPI_1_MASTER_PIN_CS),
                      MYNEWT_VAL(SPI_1_MASTER_PIN_CS1),
                      MYNEWT_VAL(SPI_1_MASTER_PIN_CS2),
                      MYNEWT_VAL(SPI_1_MASTER_PIN_CS3) }
};
#endif
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_spi_dev_cfg spi2_cfg = {
    .spi_num = 2,
    .pin_sck = MYNEWT_VAL(SPI_2_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_2_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_2_MASTER_PIN_MISO),
};
static struct bus_spi_apollo3_dev spi2_bus;
#else
static const struct apollo3_spi_cfg os_bsp_spi2m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_2_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_2_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_2_MASTER_PIN_MISO),
    .ss_pin       = { MYNEWT_VAL(SPI_2_MASTER_PIN_CS),
                      MYNEWT_VAL(SPI_2_MASTER_PIN_CS1),
                      MYNEWT_VAL(SPI_2_MASTER_PIN_CS2),
                      MYNEWT_VAL(SPI_2_MASTER_PIN_CS3) }
};
#endif
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_spi_dev_cfg spi3_cfg = {
    .spi_num = 3,
    .pin_sck = MYNEWT_VAL(SPI_3_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_3_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_3_MASTER_PIN_MISO),
};
static struct bus_spi_apollo3_dev spi3_bus;
#else
static const struct apollo3_spi_cfg os_bsp_spi3m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_3_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_3_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_3_MASTER_PIN_MISO),
    .ss_pin       = { MYNEWT_VAL(SPI_3_MASTER_PIN_CS),
                      MYNEWT_VAL(SPI_3_MASTER_PIN_CS1),
                      MYNEWT_VAL(SPI_3_MASTER_PIN_CS2),
                      MYNEWT_VAL(SPI_3_MASTER_PIN_CS3) }
};
#endif
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_spi_dev_cfg spi4_cfg = {
    .spi_num = 4,
    .pin_sck = MYNEWT_VAL(SPI_4_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_4_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_4_MASTER_PIN_MISO),
};
static struct bus_spi_apollo3_dev spi4_bus;
#else
static const struct apollo3_spi_cfg os_bsp_spi4m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_4_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_4_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_4_MASTER_PIN_MISO),
    .ss_pin       = { MYNEWT_VAL(SPI_4_MASTER_PIN_CS),
                      MYNEWT_VAL(SPI_4_MASTER_PIN_CS1),
                      MYNEWT_VAL(SPI_4_MASTER_PIN_CS2),
                      MYNEWT_VAL(SPI_4_MASTER_PIN_CS3) }
};
#endif
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_spi_dev_cfg spi5_cfg = {
    .spi_num = 5,
    .pin_sck = MYNEWT_VAL(SPI_5_MASTER_PIN_SCK),
    .pin_mosi = MYNEWT_VAL(SPI_5_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_5_MASTER_PIN_MISO),
};
static struct bus_spi_apollo3_dev spi5_bus;
#else
static const struct apollo3_spi_cfg os_bsp_spi5m_cfg = {
    .sck_pin      = MYNEWT_VAL(SPI_5_MASTER_PIN_SCK),
    .mosi_pin     = MYNEWT_VAL(SPI_5_MASTER_PIN_MOSI),
    .miso_pin     = MYNEWT_VAL(SPI_5_MASTER_PIN_MISO),
    .ss_pin       = { MYNEWT_VAL(SPI_5_MASTER_PIN_CS),
                      MYNEWT_VAL(SPI_5_MASTER_PIN_CS1),
                      MYNEWT_VAL(SPI_5_MASTER_PIN_CS2),
                      MYNEWT_VAL(SPI_5_MASTER_PIN_CS3) }
};
#endif
#endif

static void
apollo3_periph_create_timers(void)
{
    struct apollo3_timer_cfg timer_cfg;
    int rc;

    (void) timer_cfg;
    (void) rc;

#if MYNEWT_VAL(TIMER_0)
    timer_cfg.source = MYNEWT_VAL(TIMER_0_SOURCE);
    rc = hal_timer_init(0, &timer_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    timer_cfg.source = MYNEWT_VAL(TIMER_1_SOURCE);
    rc = hal_timer_init(1, &timer_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
apollo3_periph_create_adc(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(ADC_0)
    rc = os_dev_create(&os_bsp_adc0.ad_dev, "adc0",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       apollo3_adc_dev_init, &os_bsp_adc0_config);
    assert(rc == 0);
#endif
}

static void
apollo3_periph_create_uart(void)
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
apollo3_periph_create_i2c(void)
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
#if MYNEWT_VAL(I2C_2)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_i2c_hal_dev_create("i2c2", &i2c2_bus,
                                (struct bus_i2c_dev_cfg *)&i2c2_cfg);
    assert(rc == 0);
#else
    rc = hal_i2c_init(2, (void *)&hal_i2c2_cfg);
    assert(rc == 0);
#endif
#endif
#if MYNEWT_VAL(I2C_3)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_i2c_hal_dev_create("i2c3", &i2c3_bus,
                                (struct bus_i2c_dev_cfg *)&i2c3_cfg);
    assert(rc == 0);
#else
    rc = hal_i2c_init(3, (void *)&hal_i2c3_cfg);
    assert(rc == 0);
#endif
#endif
#if MYNEWT_VAL(I2C_4)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_i2c_hal_dev_create("i2c4", &i2c4_bus,
                                (struct bus_i2c_dev_cfg *)&i2c4_cfg);
    assert(rc == 0);
#else
    rc = hal_i2c_init(4, (void *)&hal_i2c4_cfg);
    assert(rc == 0);
#endif
#endif
#if MYNEWT_VAL(I2C_5)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_i2c_hal_dev_create("i2c5", &i2c5_bus,
                                (struct bus_i2c_dev_cfg *)&i2c5_cfg);
    assert(rc == 0);
#else
    rc = hal_i2c_init(5, (void *)&hal_i2c5_cfg);
    assert(rc == 0);
#endif
#endif
}

static void
apollo3_periph_create_spi(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, (void *)&os_bsp_spi0s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_apollo3_dev_create("spi0",
                                    &spi0_bus, (struct bus_spi_dev_cfg *)&spi0_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(0, (void *)&os_bsp_spi0m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_apollo3_dev_create("spi1", &spi1_bus,
                                    (struct bus_spi_dev_cfg *)&spi1_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(1, (void *)&os_bsp_spi1m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_apollo3_dev_create("spi2", &spi2_bus,
                                    (struct bus_spi_dev_cfg *)&spi2_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(2, (void *)&os_bsp_spi2m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_apollo3_dev_create("spi3", &spi3_bus,
                                    (struct bus_spi_dev_cfg *)&spi3_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(3, (void *)&os_bsp_spi3m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_apollo3_dev_create("spi4", &spi4_bus,
                                    (struct bus_spi_dev_cfg *)&spi4_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(4, (void *)&os_bsp_spi4m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_spi_apollo3_dev_create("spi5", &spi5_bus,
                                    (struct bus_spi_dev_cfg *)&spi5_cfg);
    assert(rc == 0);
#else
    rc = hal_spi_init(5, (void *)&os_bsp_spi5m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#endif
}

void
apollo3_periph_create(void)
{
    apollo3_periph_create_timers();
    apollo3_periph_create_adc();
    apollo3_periph_create_uart();
    apollo3_periph_create_i2c();
    apollo3_periph_create_spi();
}
