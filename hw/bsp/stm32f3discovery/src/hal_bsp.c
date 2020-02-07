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

#include "bsp/bsp.h"
#include "os/mynewt.h"

#include <hal/hal_bsp.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_system.h>

#include <stm32f3xx.h>
#include <stm32_common/stm32_hal.h>

#if MYNEWT_VAL(LSM303DLHC_ONB)
#include "lsm303dlhc/lsm303dlhc.h"
static struct lsm303dlhc lsm303dlhc;
#endif

#if MYNEWT_VAL(LSM303DLHC_ONB)
static struct sensor_itf i2c_0_itf = {
    .si_type = SENSOR_ITF_I2C,
    .si_num = 0,
    .si_addr = 0, /* sensor config function sets the addresses */
};
#endif

#if MYNEWT_VAL(UART_0)
const struct stm32_uart_cfg os_bsp_uart0_cfg = {
    .suc_uart = USART1,
    .suc_rcc_reg = &RCC->APB2ENR,
    .suc_rcc_dev = RCC_APB2ENR_USART1EN,
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
    .suc_pin_af = GPIO_AF7_USART1,
    .suc_irqn = USART1_IRQn,
};
#endif

#if MYNEWT_VAL(I2C_0)
const struct stm32_hal_i2c_cfg os_bsp_i2c0_cfg = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR,
    .hic_rcc_dev = RCC_APB1ENR_I2C1EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_timingr = 0x10420F13,    /* 100KHz at 8MHz of SysCoreClock */
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_sram_start,
        .hbmd_size = SRAM_SIZE
    },
    [1] = {
        .hbmd_start = &_ccram_start,
        .hbmd_size = CCRAM_SIZE
    }
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

/**
 * LSM303DLHC sensor default configuration
 *
 * @return 0 on success, non-zero on failure
 */
void
config_lsm303dlhc_sensor(void)
{
#if MYNEWT_VAL(LSM303DLHC_ONB)
    int rc;
    struct os_dev *dev;
    struct lsm303dlhc_cfg lsmcfg;

    dev = (struct os_dev *) os_dev_open("lsm303dlhc_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    /* read once per sec.  API should take this value in ms. */
    lsmcfg.accel_rate = LSM303DLHC_ACCEL_RATE_1;
    lsmcfg.accel_range = LSM303DLHC_ACCEL_RANGE_2;
    /* Device I2C addr for accelerometer */
    lsmcfg.acc_addr = LSM303DLHC_ADDR_ACCEL;
    /* Device I2C addr for magnetometer */
    lsmcfg.mag_addr = LSM303DLHC_ADDR_MAG;
    /* Set default mag gain to +/-1.3 gauss */
    lsmcfg.mag_gain = LSM303DLHC_MAG_GAIN_1_3;
    /* Set default mag sample rate to 15Hz */
    lsmcfg.mag_rate = LSM303DLHC_MAG_RATE_15;

    lsmcfg.mask = SENSOR_TYPE_ACCELEROMETER |
                  SENSOR_TYPE_MAGNETIC_FIELD;

    rc = lsm303dlhc_config((struct lsm303dlhc *)dev, &lsmcfg);
    assert(rc == 0);

    os_dev_close(dev);
#endif
}

static void
sensor_dev_create(void)
{
    int rc;
    (void)rc;

#if MYNEWT_VAL(LSM303DLHC_ONB)
    rc = os_dev_create((struct os_dev *)&lsm303dlhc, "lsm303dlhc_0",
                       OS_DEV_INIT_PRIMARY, 0, lsm303dlhc_init,
                       (void *)&i2c_0_itf);
    assert(rc == 0);
#endif
}

void
hal_bsp_init(void)
{
    stm32_periph_create();
    sensor_dev_create();
}
