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
#include <mcu/nrf52_hal.h>
#include "nrf_twim.h"

#include <nrf.h>

/*** Custom master clock frequency */
/** 380 kbps */
#define TWI_CUSTOM_FREQUENCY_FREQUENCY_K380 (0x06147ae9UL)

#if defined(NRF52810_XXAA) || defined(NRF52811_XXAA)
#define PSELSCL PSEL.SCL
#define PSELSDA PSEL.SDA
#endif

#define NRF52_HAL_I2C_MAX (2)

#define NRF52_SCL_PIN_CONF                                              \
    ((GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) |          \
      (GPIO_PIN_CNF_DRIVE_S0D1    << GPIO_PIN_CNF_DRIVE_Pos) |          \
      (GPIO_PIN_CNF_PULL_Pullup   << GPIO_PIN_CNF_PULL_Pos) |           \
      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |          \
      (GPIO_PIN_CNF_DIR_Input     << GPIO_PIN_CNF_DIR_Pos))
#define NRF52_SDA_PIN_CONF NRF52_SCL_PIN_CONF

#define NRF52_SCL_PIN_CONF_CLR                                  \
     ((GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) | \
      (GPIO_PIN_CNF_DRIVE_S0D1     << GPIO_PIN_CNF_DRIVE_Pos) | \
      (GPIO_PIN_CNF_PULL_Pullup    << GPIO_PIN_CNF_PULL_Pos)  | \
      (GPIO_PIN_CNF_INPUT_Connect  << GPIO_PIN_CNF_INPUT_Pos) | \
      (GPIO_PIN_CNF_DIR_Output     << GPIO_PIN_CNF_DIR_Pos))
#define NRF52_SDA_PIN_CONF_CLR    NRF52_SCL_PIN_CONF_CLR

struct nrf52_hal_i2c {
    NRF_TWI_Type *nhi_regs;
};

#if MYNEWT_VAL(I2C_0)
struct nrf52_hal_i2c hal_twi_i2c0 = {
    .nhi_regs = NRF_TWI0
};
#endif
#if MYNEWT_VAL(I2C_1)
struct nrf52_hal_i2c hal_twi_i2c1 = {
    .nhi_regs = NRF_TWI1
};
#endif

static struct nrf52_hal_i2c *nrf52_hal_i2cs[NRF52_HAL_I2C_MAX] = {
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
register uint32_t delay __ASM ("r0") = number_of_us;
__ASM volatile (
#ifdef NRF51
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
#ifdef NRF52
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
#ifdef NRF51
    ".syntax divided\n"
#endif
    : "+r" (delay));
}

static int
hal_i2c_resolve(uint8_t i2c_num, struct nrf52_hal_i2c **out_i2c)
{
    if (i2c_num >= NRF52_HAL_I2C_MAX) {
        *out_i2c = NULL;
        return HAL_I2C_ERR_INVAL;
    }

    *out_i2c = nrf52_hal_i2cs[i2c_num];
    if (*out_i2c == NULL) {
        return HAL_I2C_ERR_INVAL;
    }

    return 0;
}

/**
 * Converts an nRF SDK I2C status to a HAL I2C error code.
 */
static int
hal_i2c_convert_status(int nrf_status)
{
    if (nrf_status == 0) {
        return 0;
    } else if (nrf_status & NRF_TWIM_ERROR_DATA_NACK) {
        return HAL_I2C_ERR_DATA_NACK;
    } else if (nrf_status & NRF_TWIM_ERROR_ADDRESS_NACK) {
        return HAL_I2C_ERR_ADDR_NACK;
    } else {
        return HAL_I2C_ERR_UNKNOWN;
    }
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
    scl_port->PIN_CNF[scl_pin_ix] = NRF52_SCL_PIN_CONF;
    sda_port->PIN_CNF[sda_pin_ix] = NRF52_SDA_PIN_CONF;

    hal_gpio_write(scl_pin, 1);
    hal_gpio_write(sda_pin, 1);

    scl_port->PIN_CNF[scl_pin_ix] = NRF52_SCL_PIN_CONF_CLR;
    sda_port->PIN_CNF[sda_pin_ix] = NRF52_SDA_PIN_CONF_CLR;

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
    scl_port->PIN_CNF[scl_pin_ix] = NRF52_SCL_PIN_CONF;
    sda_port->PIN_CNF[sda_pin_ix] = NRF52_SDA_PIN_CONF;
}

int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    struct nrf52_hal_i2c *i2c;
    NRF_TWI_Type *regs;
    struct nrf52_hal_i2c_cfg *cfg;
    uint32_t freq;
    int rc;
    NRF_GPIO_Type *scl_port, *sda_port;

    assert(usercfg != NULL);

    rc = hal_i2c_resolve(i2c_num, &i2c);
    if (rc != 0) {
        goto err;
    }

    cfg = (struct nrf52_hal_i2c_cfg *) usercfg;
    regs = i2c->nhi_regs;

    switch (cfg->i2c_frequency) {
    case 100:
        freq = TWI_FREQUENCY_FREQUENCY_K100;
        break;
    case 250:
        freq = TWI_FREQUENCY_FREQUENCY_K250;
        break;
    case 380:
        freq = TWI_CUSTOM_FREQUENCY_FREQUENCY_K380;
        break;
    case 400:
        freq = TWI_FREQUENCY_FREQUENCY_K400;
        break;
    default:
        rc = HAL_I2C_ERR_INVAL;
        goto err;
    }

    hal_i2c_clear_bus(cfg->scl_pin, cfg->sda_pin);

    /* Resolve which GPIO port these pins belong to */
    scl_port = HAL_GPIO_PORT(cfg->scl_pin);
    sda_port = HAL_GPIO_PORT(cfg->sda_pin);

    scl_port->PIN_CNF[HAL_GPIO_INDEX(cfg->scl_pin)] = NRF52_SCL_PIN_CONF;
    sda_port->PIN_CNF[HAL_GPIO_INDEX(cfg->sda_pin)] = NRF52_SDA_PIN_CONF;

    regs->PSELSCL = cfg->scl_pin;
    regs->PSELSDA = cfg->sda_pin;
    regs->FREQUENCY = freq;
    regs->ENABLE = TWI_ENABLE_ENABLE_Enabled;

    return (0);
err:
    return (rc);
}

static inline NRF_TWI_Type *
hal_i2c_get_regs(uint8_t i2c_num)
{
    struct nrf52_hal_i2c *i2c;
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
    NRF_TWI_Type *regs;
    NRF_GPIO_Type *port;
    int index;

    regs = hal_i2c_get_regs(i2c_num);
    if (!regs) {
        return HAL_I2C_ERR_INVAL;
    }

    regs->ENABLE = TWI_ENABLE_ENABLE_Disabled;

    port = HAL_GPIO_PORT(cfg->pin_scl);
    index = HAL_GPIO_INDEX(cfg->pin_scl);
    port->PIN_CNF[index] = NRF52_SCL_PIN_CONF;

    port = HAL_GPIO_PORT(cfg->pin_sda);
    index = HAL_GPIO_INDEX(cfg->pin_sda);
    port->PIN_CNF[index] = NRF52_SDA_PIN_CONF;

    regs->PSELSCL = cfg->pin_scl;
    regs->PSELSDA = cfg->pin_sda;
    regs->FREQUENCY = TWI_FREQUENCY_FREQUENCY_K100;

    return 0;
}

static int
hal_i2c_set_enabled(uint8_t i2c_num, bool enabled)
{
    NRF_TWI_Type *regs;

    regs = hal_i2c_get_regs(i2c_num);
    if (!regs) {
        return HAL_I2C_ERR_INVAL;
    }

    regs->ENABLE = enabled ? TWI_ENABLE_ENABLE_Enabled : TWI_ENABLE_ENABLE_Disabled;

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
    NRF_TWI_Type *regs;
    int freq;

    regs = hal_i2c_get_regs(i2c_num);
    if (!regs) {
        return HAL_I2C_ERR_INVAL;
    }

    switch (cfg->frequency) {
    case 100:
        freq = TWI_FREQUENCY_FREQUENCY_K100;
        break;
    case 250:
        freq = TWI_FREQUENCY_FREQUENCY_K250;
        break;
    case 380:
        freq = TWI_CUSTOM_FREQUENCY_FREQUENCY_K380;
        break;
    case 400:
        freq = TWI_FREQUENCY_FREQUENCY_K400;
        break;
    default:
        return HAL_I2C_ERR_INVAL;
    }

    regs->FREQUENCY = freq;

    return 0;
}

static inline void
hal_i2c_trigger_start(NRF_TWI_Type *twi, __O uint32_t *task)
{
    uint32_t end_ticks;
    int retry = 2;

    /*
     * Some devices [1] can cause glitch on I2C bus which makes TWI controller
     * unresponsive and it won't write anything onto bus until disabled and
     * enabled again. To workaround this we can just check if SCL line is
     * pulled low after triggering start task which means controller works
     * properly. In other case, we disable and enable TWI controller and try
     * again. If this still fails to make controller work properly
     *
     * [1] LP5523 does this on reset
     */

    do {
        twi->EVENTS_BB = 0;
        *task = 1;

        /*
         * Wait a bit for low state on SCL as this indicates that controller has
         * started writing something one the bus. It does not matter whether low
         * state is due to START condition on bus or one of clock cycles when
         * writing address on bus - in any case this means controller seems to
         * write something on bus.
         */

        end_ticks = os_cputime_get32() +
                    os_cputime_usecs_to_ticks(MYNEWT_VAL(MCU_I2C_RECOVERY_DELAY_USEC));

        do {
            /*
             * For write op controller will always keep SCL low after writing
             * START and address on bus and until we write 1st byte of data to
             * TXD. This allows to reliably detect activity on bus by using SCL
             * only.
             *
             * For read op with only single byte to read it's possible that it
             * will be read before we start checking SCL line and thus we'll
             * never detect any activity this way. To avoid this, we'll also
             * check BB event which in such case indicates that some activity
             * on bus happened. This won't work for writes since BB is generated
             * after byte is transmitted, so we need to use both methods to be
             * able to handle unresponsive TWI controller for both reads and
             * writes.
             */
            if (!hal_gpio_read(twi->PSELSCL) || twi->EVENTS_BB) {
                return;
            }
        } while (CPUTIME_LT(os_cputime_get32(), end_ticks));

        twi->ENABLE = TWI_ENABLE_ENABLE_Disabled;
        /*
         * This is to "clear" other devices on bus which may be affected by the
         * same glitch.
         */
        hal_i2c_clear_bus(twi->PSELSCL, twi->PSELSDA);
        twi->ENABLE = TWI_ENABLE_ENABLE_Enabled;
    } while (--retry);
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                     uint32_t timo, uint8_t last_op)
{
    struct nrf52_hal_i2c *i2c;
    NRF_TWI_Type *regs;
    int nrf_status;
    int rc;
    int i;
    uint32_t start;

    rc = hal_i2c_resolve(i2c_num, &i2c);
    if (rc != 0) {
        return rc;
    }
    regs = i2c->nhi_regs;

    regs->ADDRESS = pdata->address;

    regs->EVENTS_ERROR = 0;
    regs->EVENTS_STOPPED = 0;
    regs->EVENTS_SUSPENDED = 0;
    regs->SHORTS = 0;

    hal_i2c_trigger_start(regs, &regs->TASKS_STARTTX);

    start = os_time_get();
    for (i = 0; i < pdata->len; i++) {
        regs->EVENTS_TXDSENT = 0;
        regs->TXD = pdata->buffer[i];
        while (!regs->EVENTS_TXDSENT && !regs->EVENTS_ERROR) {
            if (os_time_get() - start > timo) {
                rc = HAL_I2C_ERR_TIMEOUT;
                goto err;
            }
        }
        if (regs->EVENTS_ERROR) {
            goto err;
        }
    }
    /* If last_op is zero it means we dont put a stop at end. */
    if (last_op) {
        regs->EVENTS_STOPPED = 0;
        regs->TASKS_STOP = 1;
        while (!regs->EVENTS_STOPPED && !regs->EVENTS_ERROR) {
            if (os_time_get() - start > timo) {
                rc = HAL_I2C_ERR_TIMEOUT;
                goto err;
            }
        }
        if (regs->EVENTS_ERROR) {
            goto err;
        }
    }

    return 0;

err:
    regs->TASKS_STOP = 1;

    if (regs->EVENTS_ERROR) {
        nrf_status = regs->ERRORSRC;
        regs->ERRORSRC = nrf_status;
        rc = hal_i2c_convert_status(nrf_status);
    }

    return (rc);
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                    uint32_t timo, uint8_t last_op)
{
    struct nrf52_hal_i2c *i2c;
    NRF_TWI_Type *regs;
    int nrf_status;
    int rc;
    int i;
    uint32_t start;

    rc = hal_i2c_resolve(i2c_num, &i2c);
    if (rc != 0) {
        return rc;
    }
    regs = i2c->nhi_regs;

    start = os_time_get();

    if (regs->EVENTS_RXDREADY) {
        /*
         * If previous read was interrupted, flush RXD.
         */
        (void)regs->RXD;
        (void)regs->RXD;
    }
    regs->EVENTS_ERROR = 0;
    regs->EVENTS_STOPPED = 0;
    regs->EVENTS_SUSPENDED = 0;
    regs->EVENTS_RXDREADY = 0;

    regs->ADDRESS = pdata->address;

    if (pdata->len == 1 && last_op) {
        regs->SHORTS = TWI_SHORTS_BB_STOP_Msk;
    } else {
        regs->SHORTS = TWI_SHORTS_BB_SUSPEND_Msk;
    }

    hal_i2c_trigger_start(regs, &regs->TASKS_STARTRX);

    for (i = 0; i < pdata->len; i++) {
        regs->TASKS_RESUME = 1;
        while (!regs->EVENTS_RXDREADY && !regs->EVENTS_ERROR) {
            if (os_time_get() - start > timo) {
                rc = HAL_I2C_ERR_TIMEOUT;
                goto err;
            }
        }
        if (regs->EVENTS_ERROR) {
            goto err;
        }
        pdata->buffer[i] = regs->RXD;
        if (i == pdata->len - 2) {
            if (last_op) {
                regs->SHORTS = TWI_SHORTS_BB_STOP_Msk;
            }
        }
        regs->EVENTS_RXDREADY = 0;
    }

    return (0);

err:
    regs->TASKS_STOP = 1;
    regs->SHORTS = 0;

    if (regs->EVENTS_ERROR) {
        nrf_status = regs->ERRORSRC;
        regs->ERRORSRC = nrf_status;
        rc = hal_i2c_convert_status(nrf_status);
    }

    return (rc);
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
