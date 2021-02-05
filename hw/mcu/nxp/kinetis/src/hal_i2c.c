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

#include <stdint.h>
#include <stdbool.h>
#include "syscfg/syscfg.h"
#include "os/mynewt.h"
#include "os/os_time.h"
#include "mcu/kinetis_hal.h"
#include "mcu/mcu.h"
#include "hal/hal_i2c.h"

#include <fsl_i2c.h>
#include <fsl_port.h>
#include <fsl_clock.h>

#define NXP_HAL_I2C_MAX 4

struct nxp_hal_i2c {
    I2C_Type *dev;
    uint32_t scl_pin;
    uint32_t sda_pin;
    PORT_Type *port;
    port_mux_t mux;
    IRQn_Type irqn;
    void (*irq_handler)(void);
    i2c_master_handle_t handle;
    struct os_sem sync;
    status_t stat;
    bool enabled;
    bool ongoing;
};

#if MYNEWT_VAL(I2C_0)
static void i2c0_irq(void);
static struct nxp_hal_i2c hal_i2c0 = {
    .dev = I2C0,
    .scl_pin = MYNEWT_VAL(I2C_0_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_0_PIN_SDA),
    .port = MYNEWT_VAL(I2C_0_PORT),
    .mux = MYNEWT_VAL(I2C_0_MUX),
    .irqn = I2C0_IRQn,
    .irq_handler = i2c0_irq,
    .stat = 0,
    .enabled = false
};
#endif

#if MYNEWT_VAL(I2C_1)
static void i2c1_irq(void);
static struct nxp_hal_i2c hal_i2c1 = {
    .dev = I2C1,
    .scl_pin = MYNEWT_VAL(I2C_1_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_1_PIN_SDA),
    .port = MYNEWT_VAL(I2C_1_PORT),
    .mux = MYNEWT_VAL(I2C_1_MUX),
    .irqn = I2C1_IRQn,
    .irq_handler = i2c1_irq,
    .stat = 0,
    .enabled = false
};
#endif

#if MYNEWT_VAL(I2C_2)
static void i2c2_irq(void);
static struct nxp_hal_i2c hal_i2c2 = {
    .dev = I2C2,
    .scl_pin = MYNEWT_VAL(I2C_2_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_2_PIN_SDA),
    .port = MYNEWT_VAL(I2C_2_PORT),
    .mux = MYNEWT_VAL(I2C_2_MUX),
    .irqn = I2C2_IRQn,
    .irq_handler = i2c2_irq,
    .stat = 0,
    .enabled = false
};
#endif

#if MYNEWT_VAL(I2C_3)
static void i2c3_irq(void);
static struct nxp_hal_i2c hal_i2c3 = {
    .dev = I2C3,
    .scl_pin = MYNEWT_VAL(I2C_3_PIN_SCL),
    .sda_pin = MYNEWT_VAL(I2C_3_PIN_SDA),
    .port = MYNEWT_VAL(I2C_3_PORT),
    .mux = MYNEWT_VAL(I2C_3_MUX),
    .irqn = I2C3_IRQn,
    .irq_handler = i2c3_irq,
    .stat = 0,
    .enabled = false
};
#endif

static struct nxp_hal_i2c *i2c_modules[NXP_HAL_I2C_MAX] = {
#if MYNEWT_VAL(I2C_0)
    &hal_i2c0,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_1)
    &hal_i2c1,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_2)
    &hal_i2c2,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_3)
    &hal_i2c3,
#else
    NULL,
#endif
};

#if MYNEWT_VAL(I2C_0)
static void
i2c0_irq(void)
{
    I2C_MasterTransferHandleIRQ(hal_i2c0.dev, &hal_i2c0.handle);
}
#endif
#if MYNEWT_VAL(I2C_1)
static void
i2c1_irq(void)
{
    I2C_MasterTransferHandleIRQ(hal_i2c1.dev, &hal_i2c1.handle);
}
#endif
#if MYNEWT_VAL(I2C_2)
static void
i2c2_irq(void)
{
    I2C_MasterTransferHandleIRQ(hal_i2c2.dev, &hal_i2c2.handle);
}
#endif
#if MYNEWT_VAL(I2C_3)
static void
i2c3_irq(void)
{
    I2C_MasterTransferHandleIRQ(hal_i2c3.dev, &hal_i2c3.handle);
}
#endif

static void
master_xfer_cb(I2C_Type *dev,
               i2c_master_handle_t *handle,
               status_t status,
               void *user_data)
{
    struct nxp_hal_i2c *i2c = user_data;
    i2c->stat = status;
    os_sem_release(&i2c->sync);
}

static struct nxp_hal_i2c *
hal_i2c_resolve(uint8_t i2c_num)
{
    if (i2c_num >= NXP_HAL_I2C_MAX) {
        return NULL;
    }

    return i2c_modules[i2c_num];
}

static void
i2c_init_hw(struct nxp_hal_i2c *i2c, int pin_scl, int pin_sda)
{
    uint32_t clock_freq;
    i2c_master_config_t master_cfg;

    const port_pin_config_t pincfg = {
        kPORT_PullUp,
        kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable,
        kPORT_OpenDrainEnable,
        kPORT_LowDriveStrength,
        i2c->mux,
        kPORT_UnlockRegister
    };
    PORT_SetPinConfig(i2c->port, i2c->scl_pin, &pincfg);
    PORT_SetPinConfig(i2c->port, i2c->sda_pin, &pincfg);

    clock_freq = CLOCK_GetFreq(kCLOCK_BusClk);
    I2C_MasterGetDefaultConfig(&master_cfg);
    I2C_MasterInit(i2c->dev, &master_cfg, clock_freq);

    I2C_MasterTransferCreateHandle(i2c->dev, &i2c->handle, master_xfer_cb, i2c);
    NVIC_ClearPendingIRQ(i2c->irqn);
    NVIC_SetVector(i2c->irqn, (uint32_t) i2c->irq_handler);
    NVIC_EnableIRQ(i2c->irqn);
}

static int
i2c_config(struct nxp_hal_i2c *i2c, uint32_t frequency)
{
    uint32_t clock_freq;
    uint32_t baudrate;

    switch (frequency) {
    case 100: /* Fallthrough */
    case 400: /* Fallthrough */
    case 1000:
        baudrate = frequency * 1000;
        break;
    default:
        return HAL_I2C_ERR_INVAL;
    }

    clock_freq = CLOCK_GetFreq(kCLOCK_BusClk);
    I2C_MasterSetBaudRate(i2c->dev, baudrate, clock_freq);
    return 0;
}

static int
i2c_master_xfer(struct nxp_hal_i2c *i2c,
                i2c_direction_t direction,
                struct hal_i2c_master_data *pdata,
                uint32_t timo,
                uint8_t last_op)
{
    i2c_master_transfer_t transfer;
    status_t status;
    uint32_t start;

    transfer.slaveAddress = pdata->address;
    transfer.direction = direction;
    transfer.subaddress = 0;
    transfer.subaddressSize = 0;
    transfer.data = pdata->buffer;
    transfer.dataSize = pdata->len;
    transfer.flags = kI2C_TransferDefaultFlag;

    i2c->stat = kStatus_Success;
    start = os_time_get();

    transfer.flags |= (i2c->ongoing) ? kI2C_TransferRepeatedStartFlag : 0;
    transfer.flags |= (!last_op) ? kI2C_TransferNoStopFlag : 0;
    i2c->ongoing = !last_op;

    os_sem_init(&i2c->sync, 0);
    status = I2C_MasterTransferNonBlocking(i2c->dev, &i2c->handle, &transfer);
    if (status != kStatus_Success) {
        I2C_MasterTransferAbort(i2c->dev, &i2c->handle);
        i2c->ongoing = false;
        return HAL_I2C_ERR_UNKNOWN;
    }

    os_sem_pend(&i2c->sync, timo - (os_time_get() - start));
    if ((os_time_get() - start) > timo) {
        i2c->ongoing = false;
        return HAL_I2C_ERR_TIMEOUT;
    }

    if (i2c->stat != kStatus_Success) {
        I2C_MasterTransferAbort(i2c->dev, &i2c->handle);
        i2c->ongoing = false;
        return HAL_I2C_ERR_UNKNOWN;
    }

    return 0;
}

int
hal_i2c_config(uint8_t i2c_num, const struct hal_i2c_settings *cfg)
{
    struct nxp_hal_i2c *i2c;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
        return HAL_I2C_ERR_INVAL;
    }

    return i2c_config(i2c, cfg->frequency);
}

int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    const struct nxp_hal_i2c_cfg *hal_i2c_cfg = usercfg;
    struct nxp_hal_i2c *i2c;
    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
        return HAL_I2C_ERR_INVAL;
    }

    if (!hal_i2c_cfg) {
        return HAL_I2C_ERR_INVAL;
    }

    i2c->ongoing = 0;
    i2c->enabled = true;
    i2c_init_hw(i2c, hal_i2c_cfg->pin_scl, hal_i2c_cfg->pin_sda);
    return i2c_config(i2c, hal_i2c_cfg->frequency);
}

int
hal_i2c_init_hw(uint8_t i2c_num, const struct hal_i2c_hw_settings *cfg)
{
    struct nxp_hal_i2c *i2c;
    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
        return HAL_I2C_ERR_INVAL;
    }

    i2c_init_hw(i2c, cfg->pin_scl, cfg->pin_sda);
    return 0;
}

int
hal_i2c_enable(uint8_t i2c_num)
{
    struct nxp_hal_i2c *i2c;
    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
        return HAL_I2C_ERR_INVAL;
    }
    if (i2c->enabled) {
        return 0;
    }

    i2c->ongoing = false;
    I2C_Enable(i2c->dev, true);
    NVIC_ClearPendingIRQ(i2c->irqn);
    NVIC_SetVector(i2c->irqn, (uint32_t) i2c->irq_handler);
    NVIC_EnableIRQ(i2c->irqn);
    return 0;
}

int
hal_i2c_disable(uint8_t i2c_num)
{
    struct nxp_hal_i2c *i2c;
    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
        return HAL_I2C_ERR_INVAL;
    }
    if (!i2c->enabled) {
        return 0;
    }

    I2C_Enable(i2c->dev, true);
    NVIC_ClearPendingIRQ(i2c->irqn);
    NVIC_DisableIRQ(i2c->irqn);
    return 0;
}

int
hal_i2c_master_write(uint8_t i2c_num,
                     struct hal_i2c_master_data *pdata,
                     uint32_t timo,
                     uint8_t last_op)
{
    struct nxp_hal_i2c *i2c;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
        return HAL_I2C_ERR_INVAL;
    }

    return i2c_master_xfer(i2c, kI2C_Write, pdata, timo, last_op);
}

int
hal_i2c_master_read(uint8_t i2c_num,
                    struct hal_i2c_master_data *pdata,
                    uint32_t timo,
                    uint8_t last_op)
{
    struct nxp_hal_i2c *i2c;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
        return HAL_I2C_ERR_INVAL;
    }

    return i2c_master_xfer(i2c, kI2C_Read, pdata, timo, last_op);
}

int
hal_i2c_master_probe(uint8_t i2c_num, uint8_t address, uint32_t timo)
{
    struct hal_i2c_master_data rx;
    uint8_t buf;

    rx.address = address;
    rx.buffer = &buf;
    rx.len = 1;

    return hal_i2c_master_read(i2c_num, &rx, timo, 1);
}
