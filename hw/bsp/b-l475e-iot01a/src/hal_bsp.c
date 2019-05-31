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

#include "os/mynewt.h"

#if MYNEWT_VAL(UART_0)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#include <hal/hal_bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_timer.h>

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
#include <hal/hal_spi.h>
#endif

#if MYNEWT_VAL(LPS22HB_ONB)
#include "lps33hw/lps33hw.h"
static struct lps33hw lps22hb;
#endif

#include <stm32l475xx.h>
#include <stm32l4xx_hal_rcc.h>
#include <stm32l4xx_hal_pwr.h>
#include <stm32l4xx_hal_flash.h>
#include <stm32l4xx_hal_gpio_ex.h>
#include <mcu/stm32l4_bsp.h>
#include "mcu/stm32l4xx_mynewt_hal.h"
#include "mcu/stm32_hal.h"
#include "hal/hal_i2c.h"

#include "bsp/bsp.h"

#if MYNEWT_VAL(UART_0)
static struct uart_dev hal_uart0;

static const struct stm32_uart_cfg uart0_cfg = {
    .suc_uart = USART1,
    .suc_rcc_reg = &RCC->APB2ENR,
    .suc_rcc_dev = RCC_APB2ENR_USART1EN,
    .suc_pin_tx = MCU_GPIO_PORTB(6),
    .suc_pin_rx = MCU_GPIO_PORTB(7),
    .suc_pin_rts = -1,
    .suc_pin_cts = -1,
    .suc_pin_af = GPIO_AF7_USART1,
    .suc_irqn = USART1_IRQn
};
#endif

#if MYNEWT_VAL(I2C_0)
/*
 * NOTE: The PB8 and PB9 pins are connected through jumpers in the board to
 * both AIN and I2C pins. To enable I2C functionality SB51/SB56 need to
 * be removed (they are the default connections) and SB46/SB52 need to
 * be added.
 */
static struct stm32_hal_i2c_cfg i2c_cfg0 = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C1EN,
    .hic_pin_sda = MCU_GPIO_PORTB(9),     /* PB9 - D14 on CN1 */
    .hic_pin_scl = MCU_GPIO_PORTB(8),     /* PB8 - D15 on CN1 */
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_timingr = 0x10420F13,            /* 100KHz at 8MHz of SysCoreClock */
};
#endif

#if MYNEWT_VAL(I2C_1)
static struct stm32_hal_i2c_cfg i2c_cfg1 = {
    .hic_i2c = I2C2,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C2EN,
    .hic_pin_sda = MCU_GPIO_PORTB(10),
    .hic_pin_scl = MCU_GPIO_PORTB(11),
    .hic_pin_af = GPIO_AF4_I2C2,
    .hic_10bit = 0,
    .hic_timingr = 0x10420F13,            /* 100KHz at 8MHz of SysCoreClock */
};
#endif

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)
struct stm32_hal_spi_cfg spi0_cfg = {
    .ss_pin   = MCU_GPIO_PORTA(2),        /* D10 on CN1 */
    .sck_pin  = MCU_GPIO_PORTA(5),        /* D13 on CN1 */
    .miso_pin = MCU_GPIO_PORTA(6),        /* D12 on CN1 */
    .mosi_pin = MCU_GPIO_PORTA(7),        /* D11 on CN1 */
    .irq_prio = 2,
};
#endif

#if MYNEWT_VAL(LPS22HB_ONB)
static const struct sensor_itf i2c_0_itf_lps = {
    .si_type = SENSOR_ITF_I2C,
    .si_num = 1,
    .si_addr = 0x5d,
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    },
};

extern const struct hal_flash stm32_flash_dev;
const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &stm32_flash_dev;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
config_lps22hb_sensor(void)
{
    int rc = 0;

#if MYNEWT_VAL(LPS22HB_ONB)
    struct os_dev *dev;
    struct lps33hw_cfg cfg;

    cfg.mask = SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE;
    cfg.data_rate = LPS33HW_1HZ;
    cfg.lpf = LPS33HW_LPF_DISABLED;
    cfg.int_cfg.pin = 0;
    cfg.int_cfg.data_rdy = 0;
    cfg.int_cfg.pressure_low = 0;
    cfg.int_cfg.pressure_high = 0;
    cfg.int_cfg.active_low = 0;
    cfg.int_cfg.open_drain = 0;
    cfg.int_cfg.latched = 0;

    dev = (struct os_dev *) os_dev_open("lps22hb_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    rc = lps33hw_config((struct lps33hw *) dev, &cfg);

    os_dev_close(dev);
#endif

    return rc;
}

static void
sensor_dev_create(void)
{
    int rc;
    (void)rc;

#if MYNEWT_VAL(LPS22HB_ONB)
    rc = os_dev_create((struct os_dev *) &lps22hb, "lps22hb_0",
                       OS_DEV_INIT_PRIMARY, 0, lps33hw_init,
                       (void *)&i2c_0_itf_lps);
    assert(rc == 0);
#endif
}

void
hal_bsp_init(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &hal_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&uart0_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    hal_timer_init(0, TIM2);
#endif

#if MYNEWT_VAL(TIMER_1)
    hal_timer_init(1, TIM3);
#endif

#if MYNEWT_VAL(TIMER_2)
    hal_timer_init(2, TIM4);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, &spi0_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, &i2c_cfg0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_1)
    rc = hal_i2c_init(1, &i2c_cfg1);
    assert(rc == 0);
#endif

    sensor_dev_create();
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    /* Add any interrupt priorities configured by the bsp here */
    return pri;
}
