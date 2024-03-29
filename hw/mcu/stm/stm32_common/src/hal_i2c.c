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

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "os/mynewt.h"

#include <hal/hal_i2c.h>
#include <hal/hal_gpio.h>

#include <mcu/stm32_hal.h>

#if MYNEWT_VAL(I2C_3)
#define HAL_I2C_MAX_DEVS    4
#elif MYNEWT_VAL(I2C_2)
#define HAL_I2C_MAX_DEVS    3
#elif MYNEWT_VAL(I2C_1)
#define HAL_I2C_MAX_DEVS    2
#else
#define HAL_I2C_MAX_DEVS    1
#endif

#define I2C_ADDRESS         0xae

extern HAL_StatusTypeDef HAL_I2C_Master_Transmit_Custom(I2C_HandleTypeDef *hi2c,
        uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout,
        uint8_t LastOp);
extern HAL_StatusTypeDef HAL_I2C_Master_Receive_Custom(I2C_HandleTypeDef *hi2c,
        uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout,
        uint8_t LastOp);

struct stm32_hal_i2c {
    I2C_HandleTypeDef hid_handle;
};

#if MYNEWT_VAL(I2C_0)
static struct stm32_hal_i2c i2c0;
#endif
#if MYNEWT_VAL(I2C_1)
static struct stm32_hal_i2c i2c1;
#endif
#if MYNEWT_VAL(I2C_2)
static struct stm32_hal_i2c i2c2;
#endif
#if MYNEWT_VAL(I2C_3)
static struct stm32_hal_i2c i2c3;
#endif

static struct stm32_hal_i2c *hal_i2c_devs[HAL_I2C_MAX_DEVS] = {
#if MYNEWT_VAL(I2C_0)
    &i2c0,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_1)
    &i2c1,
#elif HAL_I2C_MAX_DEVS > 2
    NULL,
#endif
#if MYNEWT_VAL(I2C_2)
    &i2c2,
#elif HAL_I2C_MAX_DEVS > 3
    NULL,
#endif
#if MYNEWT_VAL(I2C_3)
    &i2c3,
#elif HAL_I2C_MAX_DEVS > 4
    NULL,
#endif
};

#if MYNEWT_VAL(MCU_STM32F1)
static void
i2c_reset(I2C_HandleTypeDef *hi2c)
{
    __HAL_I2C_DISABLE(hi2c);
    hi2c->Instance->CR1 |= I2C_CR1_SWRST;
    hi2c->Instance->CR1 &= ~I2C_CR1_SWRST;
    __HAL_I2C_ENABLE(hi2c);
}
#endif

#if !MYNEWT_VAL(STM32_HAL_I2C_HAS_CLOCKSPEED)
#ifndef __LL_I2C_CONVERT_TIMINGS
/* This is copy of macro from STM HAL for HALs that don't have it */
#define __LL_I2C_CONVERT_TIMINGS(__PRESCALER__, __SETUP_TIME__, __HOLD_TIME__, __SCLH_PERIOD__, __SCLL_PERIOD__) \
    ((((uint32_t)(__PRESCALER__) << I2C_TIMINGR_PRESC_Pos) & I2C_TIMINGR_PRESC) | \
     (((uint32_t)(__SETUP_TIME__) << I2C_TIMINGR_SCLDEL_Pos) & I2C_TIMINGR_SCLDEL) | \
     (((uint32_t)(__HOLD_TIME__) << I2C_TIMINGR_SDADEL_Pos) & I2C_TIMINGR_SDADEL) | \
     (((uint32_t)(__SCLH_PERIOD__) << I2C_TIMINGR_SCLH_Pos) & I2C_TIMINGR_SCLH) | \
     (((uint32_t)(__SCLL_PERIOD__) << I2C_TIMINGR_SCLL_Pos) & I2C_TIMINGR_SCLL))
#endif

static uint32_t
hal_i2c_timing(uint32_t i2c_speed, uint32_t clock)
{
    uint32_t i2c_hold_time_min, i2c_setup_time_min;
    uint32_t i2c_h_min_time, i2c_l_min_time;
    uint32_t presc;
    uint32_t timing = 0U;

    /* Timings from I2S specification for standard/fast/fast+ */
    if (i2c_speed < 400000) {
        i2c_h_min_time = 4000U;
        i2c_l_min_time = 4700U;
        i2c_hold_time_min = 500U;
        i2c_setup_time_min = 1250U;
    } else if (i2c_speed < 1000000) {
        i2c_h_min_time = 600U;
        i2c_l_min_time = 1300U;
        i2c_hold_time_min = 375U;
        i2c_setup_time_min = 500U;
    } else {
        i2c_h_min_time = 260U;
        i2c_l_min_time = 500U;
        i2c_hold_time_min = 130U;
        i2c_setup_time_min = 50U;
    }
    uint32_t clock_khz = clock / 1000;
    presc = ((i2c_h_min_time * clock_khz / 1000) + 255999) / 256000;
    uint32_t presc_max = ((i2c_l_min_time * clock_khz / 1000) + 255999) / 256000;
    presc = max(presc, presc_max);
    presc_max = ((i2c_hold_time_min * clock_khz / 1000) + 14999) / 15000;
    presc = max(presc, presc_max);
    presc_max = ((i2c_setup_time_min * clock_khz / 1000) + 15999) / 16000;
    presc = max(presc, presc_max);

    if (presc <= 16) {
        uint32_t t_presc = clock / presc;
        uint32_t ns_presc = 1000000000 / t_presc;
        uint32_t sclh = (i2c_h_min_time + ns_presc - 1) / ns_presc;
        uint32_t scll = (i2c_l_min_time + ns_presc - 1) / ns_presc;
        uint32_t sdadel = (i2c_hold_time_min + ns_presc - 1) / ns_presc;
        uint32_t scldel = (i2c_setup_time_min + ns_presc - 1) / ns_presc;

        uint32_t scl_h_l = (t_presc / i2c_speed) - 5;
        if (scl_h_l > sclh + scll) {
            uint32_t scl_h_l_fill = scl_h_l - (sclh + scll);
            scll += scl_h_l_fill / 2;
            sclh += (scl_h_l_fill + 1) / 2;
        }
        timing = __LL_I2C_CONVERT_TIMINGS(presc - 1,
                                          scldel - 1, sdadel, sclh - 1, scll - 1);
    }

    return timing;
}
#endif

int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    struct stm32_hal_i2c_cfg *cfg = (struct stm32_hal_i2c_cfg *)usercfg;
    struct stm32_hal_i2c *dev;
    I2C_InitTypeDef *init;
#if MYNEWT_VAL(MCU_STM32F1)
    GPIO_InitTypeDef gpio;
#endif
    int rc;

    if (i2c_num >= HAL_I2C_MAX_DEVS || !(dev = hal_i2c_devs[i2c_num])) {
        return -1;
    }

    init = &dev->hid_handle.Init;
    dev->hid_handle.Instance = cfg->hic_i2c;
#if !MYNEWT_VAL(STM32_HAL_I2C_HAS_CLOCKSPEED)
    if (cfg->hic_timingr) {
        init->Timing = cfg->hic_timingr;
    } else {
        init->Timing = hal_i2c_timing(cfg->hic_speed, HAL_RCC_GetPCLK1Freq());
    }
#else
    init->ClockSpeed = cfg->hic_speed;
#endif
    if (cfg->hic_10bit) {
        init->AddressingMode = I2C_ADDRESSINGMODE_10BIT;
    } else {
        init->AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    }
    init->OwnAddress1 = I2C_ADDRESS;
    init->OwnAddress2 = 0xFE;

    /*
     * Configure GPIO pins for I2C.
     * Enable clock routing for I2C.
     */
#if !MYNEWT_VAL(MCU_STM32F1)
    rc = hal_gpio_init_af(cfg->hic_pin_sda, cfg->hic_pin_af, HAL_GPIO_PULL_UP,
                          1);
    if (rc) {
        goto err;
    }
    rc = hal_gpio_init_af(cfg->hic_pin_scl, cfg->hic_pin_af, HAL_GPIO_PULL_UP,
                          1);
    if (rc) {
        goto err;
    }

    *cfg->hic_rcc_reg |= cfg->hic_rcc_dev;
#else
    /* For STM32F1x initialize I2C clock before GPIOs */
    *cfg->hic_rcc_reg |= cfg->hic_rcc_dev;

    if (cfg->hic_pin_remap_fn != NULL) {
        cfg->hic_pin_remap_fn();
    }

    /*
     * The block below applies a workaround described in 2.13.7
     * of the STM32F103 errata (also described on F105/107 errata).
     */
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pull = GPIO_NOPULL;

    hal_gpio_init_stm(cfg->hic_pin_sda, &gpio);
    hal_gpio_write(cfg->hic_pin_sda, 1);
    hal_gpio_init_stm(cfg->hic_pin_scl, &gpio);
    hal_gpio_write(cfg->hic_pin_scl, 1);

    assert(hal_gpio_read(cfg->hic_pin_sda) == 1);
    assert(hal_gpio_read(cfg->hic_pin_scl) == 1);

    hal_gpio_write(cfg->hic_pin_sda, 0);
    assert(hal_gpio_read(cfg->hic_pin_sda) == 0);

    hal_gpio_write(cfg->hic_pin_scl, 0);
    assert(hal_gpio_read(cfg->hic_pin_scl) == 0);

    hal_gpio_write(cfg->hic_pin_scl, 1);
    assert(hal_gpio_read(cfg->hic_pin_scl) == 1);

    hal_gpio_write(cfg->hic_pin_sda, 1);
    assert(hal_gpio_read(cfg->hic_pin_sda) == 1);

    /*
     * Normal I2C PIN initialization
     */
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    /* NOTE: Pull is not used in AF mode */
    gpio.Pull = GPIO_NOPULL;

    hal_gpio_init_stm(cfg->hic_pin_scl, &gpio);
    hal_gpio_init_stm(cfg->hic_pin_sda, &gpio);

    /*
     * Reset I2C
     */
    dev->hid_handle.Instance->CR1 = I2C_CR1_SWRST;
    dev->hid_handle.Instance->CR1 = 0;
#endif

    rc = HAL_I2C_Init(&dev->hid_handle);
    if (rc) {
        goto err;
    }

    return 0;
err:
    *cfg->hic_rcc_reg &= ~cfg->hic_rcc_dev;
    return rc;
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *data,
  uint32_t timo, uint8_t last_op)
{
    struct stm32_hal_i2c *dev;

    if (i2c_num >= HAL_I2C_MAX_DEVS || !(dev = hal_i2c_devs[i2c_num])) {
        return -1;
    }

    return HAL_I2C_Master_Transmit_Custom(&dev->hid_handle, data->address << 1,
            data->buffer, data->len, timo, last_op);
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
  uint32_t timo, uint8_t last_op)
{
    struct stm32_hal_i2c *dev;

    if (i2c_num >= HAL_I2C_MAX_DEVS || !(dev = hal_i2c_devs[i2c_num])) {
        return -1;
    }

    return HAL_I2C_Master_Receive_Custom(&dev->hid_handle, pdata->address << 1,
      pdata->buffer, pdata->len, timo, last_op);
}

int
hal_i2c_master_probe(uint8_t i2c_num, uint8_t address, uint32_t timo)
{
    struct stm32_hal_i2c *dev;
    int rc;

    if (i2c_num >= HAL_I2C_MAX_DEVS || !(dev = hal_i2c_devs[i2c_num])) {
        return -1;
    }

    rc = HAL_I2C_IsDeviceReady(&dev->hid_handle, address << 1, 1, timo);

#if MYNEWT_VAL(MCU_STM32F1)
    if (rc == HAL_BUSY) {
        i2c_reset(&dev->hid_handle);
    }
#endif

    return rc;
}

