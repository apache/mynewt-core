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

#include <hal/hal_i2c.h>
#include <hal/hal_gpio.h>

#include <string.h>
#include <errno.h>

#include <assert.h>

#include "stm32f4xx.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_i2c.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_rcc.h"
#include "mcu/stm32f4xx_mynewt_hal.h"

#define STM32F4_HAL_I2C_TIMEOUT (1000)

struct stm32f4_hal_i2c {
    I2C_HandleTypeDef handle;
};

#define STM32_HAL_I2C_MAX (3)

#ifdef I2C1
struct stm32f4_hal_i2c hal_i2c1;
#else
#define __HAL_RCC_I2C1_CLK_ENABLE()
#endif

#ifdef I2C2
struct stm32f4_hal_i2c hal_i2c2;
#else
#define __HAL_RCC_I2C2_CLK_ENABLE()
#endif

#ifdef I2C3
struct stm32f4_hal_i2c hal_i2c3;
#else
#define __HAL_RCC_I2C3_CLK_ENABLE()
#endif

struct stm32f4_hal_i2c *stm32f4_hal_i2cs[STM32_HAL_I2C_MAX] = {
#ifdef I2C1
        &hal_i2c1,
#else
        NULL,
#endif
#ifdef I2C2
        &hal_i2c2,
#else
        NULL,
#endif
#ifdef I2C3
        &hal_i2c3,
#else
        NULL,
#endif
};

#define STM32_HAL_I2C_RESOLVE(__n, __v) \
    if ((__n) >= STM32_HAL_I2C_MAX) {   \
        rc = EINVAL;                    \
        goto err;                       \
    }                                   \
    (__v) = stm32f4_hal_i2cs[(__n)];      \
    if ((__v) == NULL) {                \
        rc = EINVAL;                    \
        goto err;                       \
    }

int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    GPIO_InitTypeDef pcf;
    struct stm32f4_hal_i2c *i2c;
    struct stm32f4_hal_i2c_cfg *cfg;
    int rc;

    STM32_HAL_I2C_RESOLVE(i2c_num, i2c);

    cfg = (struct stm32f4_hal_i2c_cfg *) usercfg;
    assert(cfg != NULL);

    pcf.Mode = GPIO_MODE_OUTPUT_PP;
    pcf.Pull = GPIO_PULLUP;
    pcf.Speed = GPIO_SPEED_FREQ_MEDIUM;
    pcf.Alternate = 0;

    rc = hal_gpio_init_stm(cfg->sda_pin, &pcf);
    if (rc != 0) {
        goto err;
    }

    rc = hal_gpio_init_stm(cfg->scl_pin, &pcf);
    if (rc != 0) {
        goto err;
    }

    /* Configure I2C */
    switch (i2c_num) {
        case 0:
            __HAL_RCC_I2C1_CLK_ENABLE();
            break;
        case 1:
            __HAL_RCC_I2C2_CLK_ENABLE();
            break;
        case 2:
            __HAL_RCC_I2C3_CLK_ENABLE();
            break;
        default:
            rc = EINVAL;
            goto err;
    }

    if (cfg->i2c_settings != NULL) {
        /* Copy user defined settings onto the handle */
        memcpy(&i2c->handle.Init, cfg->i2c_settings, sizeof(I2C_InitTypeDef));
    } else {
        /* Initialize with default I2C Settings */
    }

    rc = HAL_I2C_Init(&i2c->handle);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
  uint32_t timo)
{
    struct stm32f4_hal_i2c *i2c;
    int rc;

    STM32_HAL_I2C_RESOLVE(i2c_num, i2c);

    rc = HAL_I2C_Master_Transmit_IT(&i2c->handle, pdata->address, pdata->buffer,
            pdata->len);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
  uint32_t timo)
{
    struct stm32f4_hal_i2c *i2c;
    int rc;

    STM32_HAL_I2C_RESOLVE(i2c_num, i2c);

    rc = HAL_I2C_Master_Receive_IT(&i2c->handle, pdata->address, pdata->buffer,
            pdata->len);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
hal_i2c_master_begin(uint8_t i2c_num)
{
    return (0);
}

int
hal_i2c_master_end(uint8_t i2c_num)
{
    struct stm32f4_hal_i2c *i2c;
    int rc;

    STM32_HAL_I2C_RESOLVE(i2c_num, i2c);

    i2c->handle.Instance->CR1 |= I2C_CR1_STOP;

    return (0);
err:
    return (rc);
}

int
hal_i2c_master_probe(uint8_t i2c_num, uint8_t address, uint32_t timo)
{
    struct stm32f4_hal_i2c *i2c;
    int rc;

    STM32_HAL_I2C_RESOLVE(i2c_num, i2c);

    rc = HAL_I2C_IsDeviceReady(&i2c->handle, address, 1,
            STM32F4_HAL_I2C_TIMEOUT);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
