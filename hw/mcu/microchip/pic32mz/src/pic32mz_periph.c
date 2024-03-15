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

#include <os/mynewt.h>

#include <hal/hal_bsp.h>
#include <hal/hal_i2c.h>
#include <hal/hal_timer.h>
#include <hal/hal_spi.h>
#include <uart/uart.h>
#include <mcu/mips_bsp.h>
#include <mcu/mips_hal.h>
#include <uart_hal/uart_hal.h>
#include <bsp/bsp.h>

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/bus.h"
#include "bus/drivers/spi_hal.h"
#endif

#if MYNEWT_VAL(ETH_0)
#include <pic32_eth/pic32_eth.h>
#endif

static struct uart_dev uart_0_dev;
static struct uart_dev uart_1_dev;
static struct uart_dev uart_2_dev;
static struct uart_dev uart_3_dev;
static struct uart_dev uart_4_dev;
static struct uart_dev uart_5_dev;

static const struct mips_uart_cfg uart_0_cfg = {
    .tx = MYNEWT_VAL(UART_0_PIN_TX),
    .rx = MYNEWT_VAL(UART_0_PIN_RX),
    .rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .cts = MYNEWT_VAL(UART_0_PIN_CTS),
};

static const struct mips_uart_cfg uart_1_cfg = {
    .tx = MYNEWT_VAL(UART_1_PIN_TX),
    .rx = MYNEWT_VAL(UART_1_PIN_RX),
    .rts = MYNEWT_VAL(UART_1_PIN_RTS),
    .cts = MYNEWT_VAL(UART_1_PIN_CTS),
};

static const struct mips_uart_cfg uart_2_cfg = {
    .tx = MYNEWT_VAL(UART_2_PIN_TX),
    .rx = MYNEWT_VAL(UART_2_PIN_RX),
    .rts = MYNEWT_VAL(UART_2_PIN_RTS),
    .cts = MYNEWT_VAL(UART_2_PIN_CTS),
};

static const struct mips_uart_cfg uart_3_cfg = {
    .tx = MYNEWT_VAL(UART_3_PIN_TX),
    .rx = MYNEWT_VAL(UART_3_PIN_RX),
    .rts = MYNEWT_VAL(UART_3_PIN_RTS),
    .cts = MYNEWT_VAL(UART_3_PIN_CTS),
};

static const struct mips_uart_cfg uart_4_cfg = {
    .tx = MYNEWT_VAL(UART_4_PIN_TX),
    .rx = MYNEWT_VAL(UART_4_PIN_RX),
    .rts = MYNEWT_VAL(UART_4_PIN_RTS),
    .cts = MYNEWT_VAL(UART_4_PIN_CTS),
};

static const struct mips_uart_cfg uart_5_cfg = {
    .tx = MYNEWT_VAL(UART_5_PIN_TX),
    .rx = MYNEWT_VAL(UART_5_PIN_RX),
    .rts = MYNEWT_VAL(UART_5_PIN_RTS),
    .cts = MYNEWT_VAL(UART_5_PIN_CTS),
};

/*
 * SPI_0
 *   SCK1  -> RD1
 */
static const struct hal_spi_hw_settings spi_0_cfg = {
    .pin_mosi = MYNEWT_VAL(SPI_0_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_0_MASTER_PIN_MISO),
    .pin_sck = MCU_GPIO_PORTD(1),
};

/*
 * SPI_1
 *   SCK2  -> RG6
 */
static const struct hal_spi_hw_settings spi_1_cfg = {
    .pin_mosi = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
    .pin_sck = MCU_GPIO_PORTG(6),
};

/*
 * SPI_2
 *   SCK3  -> B14
 */
static const struct hal_spi_hw_settings spi_2_cfg = {
    .pin_mosi = MYNEWT_VAL(SPI_2_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_2_MASTER_PIN_MISO),
    .pin_sck = MCU_GPIO_PORTB(14),
};

/*
 * SPI_3
 *   SCK4  -> RD10
 */
static const struct hal_spi_hw_settings spi_3_cfg = {
    .pin_mosi = MYNEWT_VAL(SPI_3_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_3_MASTER_PIN_MISO),
    .pin_sck = MCU_GPIO_PORTD(10),
};

#ifdef _SPI5_BASE_ADDRESS
/*
 * SPI_4
 *   SCK5  -> RF13
 */
static const struct hal_spi_hw_settings spi_4_cfg = {
    .pin_mosi = MYNEWT_VAL(SPI_4_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_4_MASTER_PIN_MISO),
    .pin_sck = MCU_GPIO_PORTF(13),
};
#endif

#ifdef _SPI6_BASE_ADDRESS
/*
 * SPI_5
 *   SCK6  -> RD15
 */
static const struct hal_spi_hw_settings spi_5_cfg = {
    .pin_mosi = MYNEWT_VAL(SPI_5_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_5_MASTER_PIN_MISO),
    .pin_sck = MCU_GPIO_PORTD(15),
};
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static const struct bus_spi_dev_cfg spi0_cfg = {
    .spi_num = 0,
    .pin_sck = MCU_GPIO_PORTD(1),
    .pin_mosi = MYNEWT_VAL(SPI_0_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_0_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi0_bus;

static const struct bus_spi_dev_cfg spi1_cfg = {
    .spi_num = 1,
    .pin_sck = MCU_GPIO_PORTG(6),
    .pin_mosi = MYNEWT_VAL(SPI_1_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_1_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi1_bus;

static const struct bus_spi_dev_cfg spi2_cfg = {
    .spi_num = 2,
    .pin_sck = MCU_GPIO_PORTB(14),
    .pin_mosi = MYNEWT_VAL(SPI_2_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_2_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi2_bus;

static const struct bus_spi_dev_cfg spi3_cfg = {
    .spi_num = 3,
    .pin_sck = MCU_GPIO_PORTD(10),
    .pin_mosi = MYNEWT_VAL(SPI_3_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_3_MASTER_PIN_MISO),
};
static struct bus_spi_hal_dev spi3_bus;

static const struct bus_spi_dev_cfg spi4_cfg = {
    .spi_num = 4,
#ifdef _SPI5_BASE_ADDRESS
    .pin_sck = MCU_GPIO_PORTF(13),
    .pin_mosi = MYNEWT_VAL(SPI_4_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_4_MASTER_PIN_MISO),
#endif
};
static struct bus_spi_hal_dev spi4_bus;

static const struct bus_spi_dev_cfg spi5_cfg = {
    .spi_num = 5,
#ifdef _SPI6_BASE_ADDRESS
    .pin_sck = MCU_GPIO_PORTD(15),
    .pin_mosi = MYNEWT_VAL(SPI_5_MASTER_PIN_MOSI),
    .pin_miso = MYNEWT_VAL(SPI_5_MASTER_PIN_MISO),
#endif
};
static struct bus_spi_hal_dev spi5_bus;
#endif

/*
 * I2C_0 -> I2C1
 *   SCL1 -> RA14 (D10 for 64 pin packges)
 *   SDA1 -> RA15 (D9 for 64 pin packges)
 */
static const struct mips_i2c_cfg i2c_0_cfg = {
#if __PIC32_PIN_COUNT > 64
    .scl = MCU_GPIO_PORTA(14),
    .sda = MCU_GPIO_PORTA(15),
#else
    .scl = MCU_GPIO_PORTD(10),
    .sda = MCU_GPIO_PORTD(9),
#endif
    .frequency = MYNEWT_VAL(I2C_0_FREQ_KHZ) * 1000,
};

#ifdef _I2C2_BASE_ADDRESS
/*
 * I2C_1 -> I2C2
 *   SCL2 -> RA2
 *   SDA2 -> RA3
 */
static const struct mips_i2c_cfg i2c_1_cfg = {
    .scl = MCU_GPIO_PORTA(2),
    .sda = MCU_GPIO_PORTA(3),
    .frequency = MYNEWT_VAL(I2C_1_FREQ_KHZ) * 1000,
};
#endif

/*
 * I2C_2 -> I2C3
 *   SCL3 -> RF8 (D3 for 64 pin packges)
 *   SDA3 -> RF2 (D2 for 64 pin packges)
 */
static const struct mips_i2c_cfg i2c_2_cfg = {
#if __PIC32_PIN_COUNT > 64
    .scl = MCU_GPIO_PORTF(8),
    .sda = MCU_GPIO_PORTF(2),
#else
    .scl = MCU_GPIO_PORTD(3),
    .sda = MCU_GPIO_PORTD(2),
#endif
    .frequency = MYNEWT_VAL(I2C_2_FREQ_KHZ) * 1000,
};

/*
 * I2C_3 -> I2C4
 *   SCL4 -> RG8
 *   SDA4 -> RG7
 */
static const struct mips_i2c_cfg i2c_3_cfg = {
    .scl = MCU_GPIO_PORTG(8),
    .sda = MCU_GPIO_PORTG(7),
    .frequency = MYNEWT_VAL(I2C_3_FREQ_KHZ) * 1000,
};

#if MYNEWT_VAL(ETH_0)
static const struct pic32_eth_cfg eth0_cfg = {
    .phy_addr = MYNEWT_VAL(PIC32_ETH_0_PHY_ADDR),
    .phy_type = MYNEWT_VAL(PIC32_ETH_0_PHY_CHIP),
    .phy_irq_pin = MYNEWT_VAL(PIC32_ETH_0_PHY_IRQ_PIN),
    .phy_irq_pin_pull_up = MYNEWT_VAL(PIC32_ETH_0_PHY_IRQ_PIN_PULLUP),
};
#endif

/*
 * I2C_4 -> I2C5
 *   SCL5 -> RF5
 *   SDA5 -> RF4
 */
static const struct mips_i2c_cfg i2c_4_cfg = {
    .scl = MCU_GPIO_PORTF(5),
    .sda = MCU_GPIO_PORTF(4),
    .frequency = MYNEWT_VAL(I2C_4_FREQ_KHZ) * 1000,
};

static void
pic32mz_periph_create_timer_devs(void)
{
    int rc;

    if (MYNEWT_VAL(TIMER_0)) {
        rc = hal_timer_init(0, NULL);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(TIMER_1)) {
        rc = hal_timer_init(1, NULL);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(TIMER_2)) {
        rc = hal_timer_init(2, NULL);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(TIMER_3)) {
        rc = hal_timer_init(3, NULL);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(TIMER_4)) {
        rc = hal_timer_init(4, NULL);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(TIMER_5)) {
        rc = hal_timer_init(5, NULL);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(TIMER_6)) {
        rc = hal_timer_init(6, NULL);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(TIMER_7)) {
        rc = hal_timer_init(7, NULL);
        assert(rc == 0);
    }

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
pic32mz_periph_create_uart_devs(void)
{
    int rc;

    if (MYNEWT_VAL(UART_0)) {
        rc = os_dev_create((struct os_dev *)&uart_0_dev, "uart0",
                           OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_0_cfg);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(UART_1)) {
        rc = os_dev_create((struct os_dev *)&uart_1_dev, "uart1",
                           OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_1_cfg);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(UART_2)) {
        rc = os_dev_create((struct os_dev *)&uart_2_dev, "uart2",
                           OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_2_cfg);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(UART_3)) {
        rc = os_dev_create((struct os_dev *)&uart_3_dev, "uart3",
                           OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_3_cfg);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(UART_4)) {
        rc = os_dev_create((struct os_dev *)&uart_4_dev, "uart4",
                           OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_4_cfg);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(UART_5)) {
        rc = os_dev_create((struct os_dev *)&uart_5_dev, "uart5",
                           OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart_5_cfg);
        assert(rc == 0);
    }
}

static void
pic32mz_periph_spi_devs(void)
{
    int rc;
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    if (MYNEWT_VAL(SPI_0_MASTER)) {
        rc = bus_spi_hal_dev_create("spi0",
                                    &spi0_bus, (struct bus_spi_dev_cfg *)&spi0_cfg);
        assert(rc == 0);
    }
    if (MYNEWT_VAL(SPI_1_MASTER)) {
        rc = bus_spi_hal_dev_create("spi1", &spi1_bus,
                                    (struct bus_spi_dev_cfg *)&spi1_cfg);
        assert(rc == 0);
    }
    if (MYNEWT_VAL(SPI_2_MASTER)) {
        rc = bus_spi_hal_dev_create("spi2", &spi2_bus,
                                    (struct bus_spi_dev_cfg *)&spi2_cfg);
        assert(rc == 0);
    }
    if (MYNEWT_VAL(SPI_3_MASTER)) {
        rc = bus_spi_hal_dev_create("spi3", &spi3_bus,
                                    (struct bus_spi_dev_cfg *)&spi3_cfg);
        assert(rc == 0);
    }
    if (MYNEWT_VAL(SPI_4_MASTER)) {
        rc = bus_spi_hal_dev_create("spi4", &spi4_bus,
                                    (struct bus_spi_dev_cfg *)&spi4_cfg);
        assert(rc == 0);
    }
    if (MYNEWT_VAL(SPI_5_MASTER)) {
        rc = bus_spi_hal_dev_create("spi5", &spi5_bus,
                                    (struct bus_spi_dev_cfg *)&spi5_cfg);
        assert(rc == 0);
    }
#else
    if (MYNEWT_VAL(SPI_0_MASTER)) {
        rc = hal_spi_init(0, (void *)&spi_0_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    }
    if (MYNEWT_VAL(SPI_1_MASTER)) {
        rc = hal_spi_init(1, (void *)&spi_1_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    }
    if (MYNEWT_VAL(SPI_2_MASTER)) {
        rc = hal_spi_init(2, (void *)&spi_2_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    }
    if (MYNEWT_VAL(SPI_3_MASTER)) {
        rc = hal_spi_init(3, (void *)&spi_3_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    }
#ifdef _SPI5_BASE_ADDRESS
    if (MYNEWT_VAL(SPI_4_MASTER)) {
        rc = hal_spi_init(4, (void *)&spi_4_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    }
#endif
#ifdef _SPI6_BASE_ADDRESS
    if (MYNEWT_VAL(SPI_5_MASTER)) {
        rc = hal_spi_init(5, (void *)&spi_5_cfg, HAL_SPI_TYPE_MASTER);
        assert(rc == 0);
    }
#endif
#endif
}

static void
pic32mz_periph_i2c_devs(void)
{
    int rc;

    if (MYNEWT_VAL(I2C_0)) {
        rc = hal_i2c_init(0, (void *)&i2c_0_cfg);
        assert(rc == 0);
    }

#ifdef _I2C2_BASE_ADDRESS
    if (MYNEWT_VAL(I2C_1)) {
        rc = hal_i2c_init(1, (void *)&i2c_1_cfg);
        assert(rc == 0);
    }
#endif

    if (MYNEWT_VAL(I2C_2)) {
        rc = hal_i2c_init(2, (void *)&i2c_2_cfg);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(I2C_3)) {
        rc = hal_i2c_init(3, (void *)&i2c_3_cfg);
        assert(rc == 0);
    }

    if (MYNEWT_VAL(I2C_4)) {
        rc = hal_i2c_init(4, (void *)&i2c_4_cfg);
        assert(rc == 0);
    }
}

static void
pic32mz_periph_create_eth(void)
{
#if MYNEWT_VAL(ETH_0)
    int rc;
    rc = pic32_eth_init(&eth0_cfg);
    assert(rc == 0);
    (void)rc;
#endif
}

void
pic32mz_periph_create(void)
{
    pic32mz_periph_create_timer_devs();
    pic32mz_periph_create_uart_devs();
    pic32mz_periph_spi_devs();
    pic32mz_periph_i2c_devs();
    pic32mz_periph_create_eth();
}
