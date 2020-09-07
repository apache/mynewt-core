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
#include <limits.h>
#include <assert.h>
#include "os/mynewt.h"
#include <hal/hal_i2c.h>
#include <hal/hal_gpio.h>
#include <mcu/nrf91_hal.h>
#include "nrf_twim.h"
#include "nrfx_config.h"

#include <nrf.h>

/*** Custom master clock frequency */
/** 380 kbps */
#define TWIM_CUSTOM_FREQUENCY_FREQUENCY_K380 (0x06147ae9UL)

#define NRF91_HAL_I2C_MAX (2)

#define NRF91_SCL_PIN_CONF                                     \
    ((GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) | \
     (GPIO_PIN_CNF_DRIVE_S0D1    << GPIO_PIN_CNF_DRIVE_Pos) |  \
     (GPIO_PIN_CNF_PULL_Pullup   << GPIO_PIN_CNF_PULL_Pos) |   \
     (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |  \
     (GPIO_PIN_CNF_DIR_Input     << GPIO_PIN_CNF_DIR_Pos))
#define NRF91_SDA_PIN_CONF NRF91_SCL_PIN_CONF

#define NRF91_SCL_PIN_CONF_CLR                                  \
     ((GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) | \
      (GPIO_PIN_CNF_DRIVE_S0D1     << GPIO_PIN_CNF_DRIVE_Pos) | \
      (GPIO_PIN_CNF_PULL_Pullup    << GPIO_PIN_CNF_PULL_Pos) |  \
      (GPIO_PIN_CNF_INPUT_Connect  << GPIO_PIN_CNF_INPUT_Pos) | \
      (GPIO_PIN_CNF_DIR_Output     << GPIO_PIN_CNF_DIR_Pos))
#define NRF91_SDA_PIN_CONF_CLR    NRF91_SCL_PIN_CONF_CLR

struct nrf91_hal_i2c {
    NRF_TWIM_Type *nhi_regs;
};

#if MYNEWT_VAL(I2C_0)
struct nrf91_hal_i2c hal_twi_i2c0 = {
    .nhi_regs = NRF_TWI0
};
#endif
#if MYNEWT_VAL(I2C_1)
struct nrf91_hal_i2c hal_twi_i2c1 = {
    .nhi_regs = NRF_TWI1
};
#endif

static struct nrf91_hal_i2c *nrf91_hal_i2cs[NRF91_HAL_I2C_MAX] = {
#if MYNEWT_VAL(I2C_0)
    &hal_twi_i2c0,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_1)
    &hal_twi_i2c1
#else
    NULL
#endif
};

static void
hal_i2c_delay_us(uint32_t number_of_us)
{
    register uint32_t delay __ASM("r0") = number_of_us;
    __ASM volatile (
#ifdef NRF91
        ".syntax unified\n"
#endif
        "1:\n"
        " SUBS %0, %0, #1\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
#ifdef NRF91
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
        " NOP\n"
#endif
        " BNE 1b\n"
#ifdef NRF91
        ".syntax divided\n"
#endif
        : "+r" (delay));
}

static int
hal_i2c_resolve(uint8_t i2c_num, struct nrf91_hal_i2c **out_i2c)
{
    if (i2c_num >= NRF91_HAL_I2C_MAX) {
        *out_i2c = NULL;
        return HAL_I2C_ERR_INVAL;
    }

    *out_i2c = nrf91_hal_i2cs[i2c_num];
    if (*out_i2c == NULL) {
        return HAL_I2C_ERR_INVAL;
    }

    return 0;
}

/**
 * Reads the input buffer of the specified pin regardless
 * of if it is set as output or input
 */
static int
read_gpio_inbuffer(int pin)
{
    NRF_GPIO_Type *port;
    port = HAL_GPIO_PORT(pin);

    return (port->IN >> HAL_GPIO_INDEX(pin)) & 1UL;
}

/*
 * Clear the bus after reset by clocking 9 bits manually.
 * This should reset state from (most of) the devices on the other end.
 */
static void
hal_i2c_clear_bus(int scl_pin, int sda_pin)
{
    int i;
    NRF_GPIO_Type *scl_port, *sda_port;
    int scl_pin_ix;
    int sda_pin_ix;
    /* Resolve which GPIO port these pins belong to */
    scl_port = HAL_GPIO_PORT(scl_pin);
    sda_port = HAL_GPIO_PORT(sda_pin);
    scl_pin_ix = HAL_GPIO_INDEX(scl_pin);
    sda_pin_ix = HAL_GPIO_INDEX(sda_pin);

    /* Input connected, standard-low disconnected-high, pull-ups */
    scl_port->PIN_CNF[scl_pin_ix] = NRF91_SCL_PIN_CONF;
    sda_port->PIN_CNF[sda_pin_ix] = NRF91_SDA_PIN_CONF;

    hal_gpio_write(scl_pin, 1);
    hal_gpio_write(sda_pin, 1);

    scl_port->PIN_CNF[scl_pin_ix] = NRF91_SCL_PIN_CONF_CLR;
    sda_port->PIN_CNF[sda_pin_ix] = NRF91_SDA_PIN_CONF_CLR;

    hal_i2c_delay_us(4);

    for (i = 0; i < 9; i++) {
        if (read_gpio_inbuffer(sda_pin)) {
            if (i == 0) {
                /*
                 * Nothing to do here.
                 */
                goto ret;
            } else {
                break;
            }
        }
        hal_gpio_write(scl_pin, 0);
        hal_i2c_delay_us(4);
        hal_gpio_write(scl_pin, 1);
        hal_i2c_delay_us(4);
    }

    /*
     * Send STOP.
     */
    hal_gpio_write(sda_pin, 0);
    hal_i2c_delay_us(4);
    hal_gpio_write(sda_pin, 1);

ret:
    /* Restore GPIO config */
    scl_port->PIN_CNF[scl_pin_ix] = NRF91_SCL_PIN_CONF;
    sda_port->PIN_CNF[sda_pin_ix] = NRF91_SDA_PIN_CONF;
}

int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    struct nrf91_hal_i2c *i2c;
    NRF_TWIM_Type *regs;
    struct nrf91_hal_i2c_cfg *cfg;
    uint32_t freq;
    int rc;
    NRF_GPIO_Type *scl_port, *sda_port;

    assert(usercfg != NULL);

    rc = hal_i2c_resolve(i2c_num, &i2c);
    if (rc != 0) {
        goto err;
    }

    cfg = (struct nrf91_hal_i2c_cfg *) usercfg;
    regs = i2c->nhi_regs;

    switch (cfg->i2c_frequency) {
    case 100:
        freq = TWIM_FREQUENCY_FREQUENCY_K100;
        break;
    case 250:
        freq = TWIM_FREQUENCY_FREQUENCY_K250;
        break;
    case 380:
        freq = TWIM_CUSTOM_FREQUENCY_FREQUENCY_K380;
        break;
    case 400:
        freq = TWIM_FREQUENCY_FREQUENCY_K400;
        break;
    default:
        rc = HAL_I2C_ERR_INVAL;
        goto err;
    }

    hal_i2c_clear_bus(cfg->scl_pin, cfg->sda_pin);

    /* Resolve which GPIO port these pins belong to */
    scl_port = HAL_GPIO_PORT(cfg->scl_pin);
    sda_port = HAL_GPIO_PORT(cfg->sda_pin);

    scl_port->PIN_CNF[HAL_GPIO_INDEX(cfg->scl_pin)] = NRF91_SCL_PIN_CONF;
    sda_port->PIN_CNF[HAL_GPIO_INDEX(cfg->sda_pin)] = NRF91_SDA_PIN_CONF;

    regs->PSEL.SCL = cfg->scl_pin;
    regs->PSEL.SDA = cfg->sda_pin;
    regs->FREQUENCY = freq;
    regs->ENABLE = TWIM_ENABLE_ENABLE_Enabled;

    return (0);
err:
    return (rc);
}

static inline NRF_TWIM_Type *
hal_i2c_get_regs(uint8_t i2c_num)
{
    struct nrf91_hal_i2c *i2c;
    int rc;

    rc = hal_i2c_resolve(i2c_num, &i2c);
    if (rc != 0) {
        return NULL;
    }

    return i2c->nhi_regs;
}

int
hal_i2c_init_hw(uint8_t i2c_num, const struct hal_i2c_hw_settings *cfg)
{
    NRF_TWIM_Type *regs;
    NRF_GPIO_Type *port;
    int index;

    regs = hal_i2c_get_regs(i2c_num);
    if (!regs) {
        return HAL_I2C_ERR_INVAL;
    }

    regs->ENABLE = TWIM_ENABLE_ENABLE_Disabled;

    port = HAL_GPIO_PORT(cfg->pin_scl);
    index = HAL_GPIO_INDEX(cfg->pin_scl);
    port->PIN_CNF[index] = NRF91_SCL_PIN_CONF;

    port = HAL_GPIO_PORT(cfg->pin_sda);
    index = HAL_GPIO_INDEX(cfg->pin_sda);
    port->PIN_CNF[index] = NRF91_SDA_PIN_CONF;

    regs->PSEL.SCL = cfg->pin_scl;
    regs->PSEL.SDA = cfg->pin_sda;
    regs->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K100;

    return 0;
}

static int
hal_i2c_set_enabled(uint8_t i2c_num, bool enabled)
{
    NRF_TWIM_Type *regs;

    regs = hal_i2c_get_regs(i2c_num);
    if (!regs) {
        return HAL_I2C_ERR_INVAL;
    }

    regs->ENABLE = enabled ? TWIM_ENABLE_ENABLE_Enabled : TWIM_ENABLE_ENABLE_Disabled;

    return 0;
}

int
hal_i2c_enable(uint8_t i2c_num)
{
    return hal_i2c_set_enabled(i2c_num, 1);
}

int
hal_i2c_disable(uint8_t i2c_num)
{
    return hal_i2c_set_enabled(i2c_num, 0);
}

int
hal_i2c_config(uint8_t i2c_num, const struct hal_i2c_settings *cfg)
{
    NRF_TWIM_Type *regs;
    int freq;

    regs = hal_i2c_get_regs(i2c_num);
    if (!regs) {
        return HAL_I2C_ERR_INVAL;
    }

    switch (cfg->frequency) {
    case 100:
        freq = TWIM_FREQUENCY_FREQUENCY_K100;
        break;
    case 250:
        freq = TWIM_FREQUENCY_FREQUENCY_K250;
        break;
    case 380:
        freq = TWIM_CUSTOM_FREQUENCY_FREQUENCY_K380;
        break;
    case 400:
        freq = TWIM_FREQUENCY_FREQUENCY_K400;
        break;
    default:
        return HAL_I2C_ERR_INVAL;
    }

    regs->FREQUENCY = freq;

    return 0;
}
