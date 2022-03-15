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
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include "os/mynewt.h"
#include "hal/hal_i2c.h"
#include "mcu/hal_apollo3.h"
#include "mcu/cmsis_nvic.h"

#include "am_mcu_apollo.h"

/* Prevent CMSIS from breaking apollo3 macros. */
#undef GPIO
#undef IOSLAVE
#undef CLKGEN

/* Pointer array that points to am_hal_iom */
void *g_i2c_handles[AM_REG_IOM_NUM_MODULES];

static am_hal_iom_config_t g_iom_i2c_default_config =
{
    .eInterfaceMode = AM_HAL_IOM_I2C_MODE,
    .ui32ClockFreq  = AM_HAL_IOM_1MHZ,
};

/*  | i2c:cfg   | scl   | sda   |
 *  |-----------+-------+-------|
 *  | 0:0       | 5     | 6     |
 *  | 1:0       | 8     | 9     |
 *  | 2:4       | 27    | 25    |
 *  | 3:4       | 42    | 43    |
 *  | 4:4       | 39    | 40    |
 *  | 5:4       | 48    | 49    |
 */
static int
hal_i2c_pin_config(int i2c_num, const struct apollo3_i2c_cfg *pins)
{
    switch (i2c_num) {
#if MYNEWT_VAL(I2C_0)
    case 0:
        if (pins->scl_pin == 5 && pins->sda_pin == 6) {
            return 0;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(I2C_1)
    case 1:
        if (pins->scl_pin == 8 && pins->sda_pin == 9) {
            return 0;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(I2C_2)
    case 2:
        if (pins->scl_pin == 27 && pins->sda_pin == 25) {
            return 4;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(I2C_3)
    case 3:
        if (pins->scl_pin == 42 && pins->sda_pin == 43) {
            return 4;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(I2C_4)
    case 4:
        if (pins->scl_pin == 39 && pins->sda_pin == 40) {
            return 4;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(I2C_5)
    case 5:
        if (pins->scl_pin == 48 && pins->sda_pin == 49) {
            return 4;
        } else {
            return -1;
        }
#endif
    default:
        return -1;
    }
}

int hal_i2c_init_hw(uint8_t i2c_num, const struct hal_i2c_hw_settings *cfg) 
{
    int pin_cfg;
    am_hal_gpio_pincfg_t i2c_cfg;
    struct apollo3_i2c_cfg apollo_i2c_cfg;
    
     apollo_i2c_cfg.sda_pin = cfg->pin_sda;
     apollo_i2c_cfg.scl_pin = cfg->pin_scl;

    /* Initialize the IOM. */
    if (am_hal_iom_initialize(i2c_num, &g_i2c_handles[i2c_num]) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    if (am_hal_iom_power_ctrl(g_i2c_handles[i2c_num], AM_HAL_SYSCTRL_WAKE, false) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    /* Set the required configuration settings for the IOM. */
    if (am_hal_iom_configure(g_i2c_handles[i2c_num], &g_iom_i2c_default_config) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    /* Configure GPIOs for I2C based on i2c_num */
    pin_cfg = hal_i2c_pin_config(i2c_num, &apollo_i2c_cfg);
    if (pin_cfg == -1) {
        return SYS_EINVAL;
    }

    i2c_cfg.uFuncSel            = pin_cfg;
    i2c_cfg.ePullup             = AM_HAL_GPIO_PIN_PULLUP_1_5K;
    i2c_cfg.eDriveStrength      = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA;
    i2c_cfg.eGPOutcfg           = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN;
    i2c_cfg.uIOMnum             = i2c_num;

    if (am_hal_gpio_pinconfig(apollo_i2c_cfg.sda_pin, i2c_cfg) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }
    if (am_hal_gpio_pinconfig(apollo_i2c_cfg.scl_pin, i2c_cfg) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    return hal_i2c_enable(i2c_num);
}

int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    int pin_cfg;
    am_hal_gpio_pincfg_t i2c_cfg;
    struct apollo3_i2c_cfg *cfg = usercfg;

    /* Initialize the IOM. */
    if (am_hal_iom_initialize(i2c_num, &g_i2c_handles[i2c_num]) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    if (am_hal_iom_power_ctrl(g_i2c_handles[i2c_num], AM_HAL_SYSCTRL_WAKE, false) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    /* Set the required configuration settings for the IOM. */
    if (am_hal_iom_configure(g_i2c_handles[i2c_num], &g_iom_i2c_default_config) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    /* Configure GPIOs for I2C based on i2c_num */
    pin_cfg = hal_i2c_pin_config(i2c_num, cfg);
    if (pin_cfg == -1) {
        return SYS_EINVAL;
    }

    i2c_cfg.uFuncSel            = pin_cfg;
    i2c_cfg.ePullup             = AM_HAL_GPIO_PIN_PULLUP_1_5K;
    i2c_cfg.eDriveStrength      = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA;
    i2c_cfg.eGPOutcfg           = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN;
    i2c_cfg.uIOMnum             = i2c_num;

    if (am_hal_gpio_pinconfig(cfg->sda_pin, i2c_cfg) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }
    if (am_hal_gpio_pinconfig(cfg->scl_pin, i2c_cfg) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    return hal_i2c_enable(i2c_num);
}

int hal_i2c_config(uint8_t i2c_num, const struct hal_i2c_settings *cfg) 
{
    am_hal_iom_config_t iom_cfg;
        
    iom_cfg.eInterfaceMode = AM_HAL_IOM_I2C_MODE;

    /* Frequency is in khz, map to AM_HAL_IOM frequencies */
    switch(cfg->frequency) {
        case 100:
            iom_cfg.ui32ClockFreq = AM_HAL_IOM_100KHZ;
            break;
        case 400:
            iom_cfg.ui32ClockFreq = AM_HAL_IOM_400KHZ;
            break;
        case 1000:
            iom_cfg.ui32ClockFreq = AM_HAL_IOM_1MHZ;
            break;
        default:
            return SYS_EINVAL;
    }

    if (am_hal_iom_configure(g_i2c_handles[i2c_num], &iom_cfg) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    return 0;
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                     uint32_t timeout, uint8_t last_op)
{
    am_hal_iom_transfer_t       transaction;

    transaction.ui32InstrLen    = 0;
    transaction.ui32Instr       = 0;
    transaction.eDirection      = AM_HAL_IOM_TX;
    transaction.ui32NumBytes    = pdata->len;
    transaction.pui32TxBuffer   = (uint32_t *)pdata->buffer;
    transaction.bContinue       = !(bool)last_op;
    transaction.ui8RepeatCount  = 0;
    transaction.ui32PauseCondition = 0;
    transaction.ui32StatusSetClr = 0;
    transaction.uPeerInfo.ui32I2CDevAddr = pdata->address;

    if (am_hal_iom_blocking_transfer(g_i2c_handles[i2c_num], &transaction) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    return 0;
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                    uint32_t timeout, uint8_t last_op)
{
    am_hal_iom_transfer_t       transaction;

    transaction.ui32InstrLen    = 0;
    transaction.ui32Instr       = 0;
    transaction.eDirection      = AM_HAL_IOM_RX;
    transaction.ui32NumBytes    = pdata->len;
    transaction.pui32RxBuffer   = (uint32_t *)pdata->buffer;
    transaction.bContinue       = !(bool)last_op;
    transaction.ui8RepeatCount  = 0;
    transaction.ui32PauseCondition = 0;
    transaction.ui32StatusSetClr = 0;
    transaction.uPeerInfo.ui32I2CDevAddr = pdata->address;

    if (am_hal_iom_blocking_transfer(g_i2c_handles[i2c_num], &transaction) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    return 0;
}

int hal_i2c_master_probe(uint8_t i2c_num, uint8_t address, uint32_t timeout) 
{
    am_hal_iom_transfer_t       transaction;

    transaction.ui32InstrLen    = 0;
    transaction.ui32Instr       = 0;
    transaction.eDirection      = AM_HAL_IOM_RX;
    transaction.ui32NumBytes    = 0;
    transaction.pui32RxBuffer   = NULL;
    transaction.bContinue       = false;
    transaction.ui8RepeatCount  = 0;
    transaction.ui32PauseCondition = 0;
    transaction.ui32StatusSetClr = 0;
    transaction.uPeerInfo.ui32I2CDevAddr = (uint32_t)address;

    if (am_hal_iom_blocking_transfer(g_i2c_handles[i2c_num], &transaction) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    return 0;
}

int hal_i2c_enable(uint8_t i2c_num) 
{
    if (am_hal_iom_enable(g_i2c_handles[i2c_num]) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }
    
    return 0;
}

int hal_i2c_disable(uint8_t i2c_num) 
{
    if (am_hal_iom_disable(g_i2c_handles[i2c_num]) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }
    
    return 0;
}