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
#include <mcu/cmsis_nvic.h>
#include <hal/hal_i2c.h>
#include <hal/hal_gpio.h>
#include <mcu/nrf52_hal.h>
#include "nrf_twim.h"
#include <console/console.h>

#include <nrf.h>

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

typedef void (*nrf52_i2c_irq_handler_t)(void);

/*
 * OS semaphore for pending on a transaction to complete.
 * The semaphore is released in the interrupt context.
 * This should probably be replaced with callbacks which get
 * registered before the interface is enabled, similar to
 * hal_spi_set_rxrx_cb
 */
static struct os_sem hal_i2c_sync_sem;

struct nrf52_hal_i2c {
    NRF_TWIM_Type *nhi_regs;
    uint32_t irq_number;
    nrf52_i2c_irq_handler_t irq_handler;
    volatile int last_error;
};

void hal_i2c_irq_handler(struct nrf52_hal_i2c *i2c);


#if MYNEWT_VAL(I2C_0)
void i2c0_irq_handler(void);
struct nrf52_hal_i2c hal_twi_i2c0 = {
    .nhi_regs = NRF_TWIM0,
    .irq_number = SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQn,
    .irq_handler = i2c0_irq_handler
};

void
i2c0_irq_handler(void)
{
    os_trace_isr_enter();
    hal_i2c_irq_handler(&hal_twi_i2c0);
    os_trace_isr_exit();
}
#endif

#if MYNEWT_VAL(I2C_1)
void i2c1_irq_handler(void);
struct nrf52_hal_i2c hal_twi_i2c1 = {
    .nhi_regs = NRF_TWIM1,
    .irq_number = SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn,
    .irq_handler = i2c1_irq_handler
};

void
i2c1_irq_handler(void)
{
    os_trace_isr_enter();
    hal_i2c_irq_handler(&hal_twi_i2c1);
    os_trace_isr_exit();
}
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

/**
 * Converts an nRF SDK I2C status to a HAL I2C error code.
 */
static int
hal_i2c_convert_status(int nrf_status)
{
    if (nrf_status == 0) {
        return 0;
    } else if (nrf_status & NRF_TWIM_ERROR_DATA_NACK) {
        console_printf("<>i2c error: DATA_NAK<>\n");
        return HAL_I2C_ERR_DATA_NACK;
    } else if (nrf_status & NRF_TWIM_ERROR_ADDRESS_NACK) {
        console_printf("<>i2c error: ADDR_NAK<>\n");
        return HAL_I2C_ERR_ADDR_NACK;
    } else if (nrf_status & TWIM_ERRORSRC_OVERRUN_Msk) {
        console_printf("<>i2c error: OVERRUN<>\n");
        return HAL_I2C_ERR_OVERRUN;
    } else {
        console_printf("<>i2c error: UNKNOWN<>\n");
        return HAL_I2C_ERR_UNKNOWN;
    }
}

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
    /* Resolve which GPIO port these pins belong to */
    scl_port = HAL_GPIO_PORT(scl_pin);
    sda_port = HAL_GPIO_PORT(sda_pin);

    /* Input connected, standard-low disconnected-high, pull-ups */
    scl_port->PIN_CNF[scl_pin] = NRF52_SCL_PIN_CONF;
    sda_port->PIN_CNF[sda_pin] = NRF52_SDA_PIN_CONF;

    hal_gpio_write(scl_pin, 1);
    hal_gpio_write(sda_pin, 1);

    scl_port->PIN_CNF[scl_pin] = NRF52_SCL_PIN_CONF_CLR;
    sda_port->PIN_CNF[sda_pin] = NRF52_SDA_PIN_CONF_CLR;

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
    scl_port->PIN_CNF[scl_pin] = NRF52_SCL_PIN_CONF;
    sda_port->PIN_CNF[sda_pin] = NRF52_SDA_PIN_CONF;
}

/**
 * Note: this function is left here for backward compatibility
 * with existing code that isn't yet using the bus driver. It
 * will be superceded by hal_i2c_init_hw() and hal_i2c_config()
 * once all code is using the bus driver.
 */
int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    struct hal_i2c_hw_settings hw_cfg;
    struct nrf52_hal_i2c_cfg *legacy_cfg;
    struct hal_i2c_settings new_cfg;
    int rc;

    assert(usercfg != NULL);
    legacy_cfg = (struct nrf52_hal_i2c_cfg *) usercfg;
    hw_cfg.pin_scl = legacy_cfg->scl_pin;
    hw_cfg.pin_sda = legacy_cfg->sda_pin;
    new_cfg.frequency = legacy_cfg->i2c_frequency;

    /** Set all TWIM registers, excluding frequency, and NVIC */
    rc = hal_i2c_init_hw(i2c_num, &hw_cfg);
    if (rc != 0) {
        goto err;
    }

    /** Set frequency */
    rc = hal_i2c_config(i2c_num, &new_cfg);
    if (rc != 0) {
        goto err;
    }

    /** Do an initial bus clear operation, in case some device is misbehaving */
    hal_i2c_clear_bus(legacy_cfg->scl_pin, legacy_cfg->sda_pin);

    return (0);
err:
    return (rc);
}

static inline NRF_TWIM_Type *
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

/**
 * This is the init function which gets called by the bus driver and should be the
 * one to use going forward. hal_i2c_init() is left in the code base for now, just
 * for backward compatibility with existing code that hasn't yet moved to using the
 * bus driver.
 */
int
hal_i2c_init_hw(uint8_t i2c_num, const struct hal_i2c_hw_settings *cfg)
{
    NRF_TWIM_Type *regs;
    NRF_GPIO_Type *port;
    struct nrf52_hal_i2c *i2c;
    int index;

    hal_i2c_resolve(i2c_num, &i2c);
    regs = hal_i2c_get_regs(i2c_num);
    if (!regs) {
        return HAL_I2C_ERR_INVAL;
    }


    regs->ENABLE = TWIM_ENABLE_ENABLE_Disabled;

    port = HAL_GPIO_PORT(cfg->pin_scl);
    index = HAL_GPIO_INDEX(cfg->pin_scl);
    port->PIN_CNF[index] = NRF52_SCL_PIN_CONF;

    port = HAL_GPIO_PORT(cfg->pin_sda);
    index = HAL_GPIO_INDEX(cfg->pin_sda);
    port->PIN_CNF[index] = NRF52_SDA_PIN_CONF;

    regs->PSEL.SCL = cfg->pin_scl;
    regs->PSEL.SDA = cfg->pin_sda;
    regs->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K100;
    regs->ADDRESS = 0;
    regs->ENABLE = TWIM_ENABLE_ENABLE_Enabled;
    regs->INTENCLR = NRF_TWIM_ALL_INTS_MASK;

    assert(i2c->irq_handler != NULL);
    NVIC_SetVector( i2c->irq_number, (uint32_t) i2c->irq_handler );
    NVIC_SetPriority( i2c->irq_number, (1 << __NVIC_PRIO_BITS) - 1 );
    NVIC_ClearPendingIRQ( i2c->irq_number );
    NVIC_EnableIRQ( i2c->irq_number );

    if (os_sem_init(&hal_i2c_sync_sem, 0) != OS_OK) {
        assert(0);
    }

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
        freq = TWIM_FREQUENCY_FREQUENCY_K380;
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

static inline void
hal_i2c_trigger_start(NRF_TWIM_Type *twim, __O uint32_t *task)
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
        twim->EVENTS_TXSTARTED = 0;
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
            if (!hal_gpio_read(twim->PSEL.SCL) || twim->EVENTS_TXSTARTED) {
                return;
            }
        } while (CPUTIME_LT(os_cputime_get32(), end_ticks));

        twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
        /*
         * This is to "clear" other devices on bus which may be affected by the
         * same glitch.
         */
        hal_i2c_clear_bus(twim->PSEL.SCL, twim->PSEL.SDA);
        twim->ENABLE = TWIM_ENABLE_ENABLE_Enabled;
    } while (--retry);
}

/*
 * Handle errors returned from the TWIM peripheral along with timeouts
 */
static int
hal_i2c_handle_errors(struct nrf52_hal_i2c *i2c, int rc)
{
    int nrf_status;
    NRF_TWIM_Type *regs;

    regs = i2c->nhi_regs;
    if (regs->EVENTS_SUSPENDED) {
        regs->EVENTS_SUSPENDED = 0;
        regs->TASKS_RESUME = 1;
    }
    regs->TASKS_STOP = 1;

    if (regs->EVENTS_ERROR) {
        regs->EVENTS_ERROR = 0;
        nrf_status = regs->ERRORSRC;
        regs->ERRORSRC = nrf_status;
        rc = hal_i2c_convert_status(nrf_status);
    } else if (rc) {
        /* Some I2C slave peripherals cause a glitch on the bus when they
         * reset which puts the TWI in an unresponsive state. Disabling and
         * re-enabling the TWI returns it to normal operation.
         * A clear operation is performed in case one of the devices on
         * the bus is in a bad state.
         */
        regs->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
        hal_i2c_clear_bus(regs->PSEL.SCL, regs->PSEL.SDA);
        regs->ENABLE = TWIM_ENABLE_ENABLE_Enabled;
        regs->EVENTS_STOPPED = 0;
    }

    return rc;
}

/*
 * Perform a I2C master write transaction using TWIM/EasyDMA
 */
int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                     uint32_t timo, uint8_t last_op)
{
    int rc;
    uint32_t int_mask = 0;
    NRF_TWIM_Type *regs;
    struct nrf52_hal_i2c *i2c;

    /* Resolve the I2C bus */
    rc = hal_i2c_resolve(i2c_num, &i2c);
    if (rc != 0) {
        return rc;
    }

    regs = i2c->nhi_regs;

    /*
     * Configure the TXD registers for EasyDMA access to work with buffers of
     * specific length and address of the slave
     */
    regs->ADDRESS    = pdata->address;
    regs->TXD.MAXCNT = pdata->len;
    regs->TXD.PTR    = (uint32_t)pdata->buffer;
    regs->TXD.LIST   = 0;

    /* Disable and clear interrupts */
    regs->INTENCLR   = NRF_TWIM_ALL_INTS_MASK;
    regs->INTEN      = 0;

    /* Setup shorts to end transaction based on last_op,
     * 0 : STOP transaction,
     * 1 : SUSPEND transaction
     */
    if (last_op) {
        /* Enable short for LASTX->STOP, interrupt to happen on STOP or ERROR */
        regs->SHORTS = TWIM_SHORTS_LASTTX_STOP_Msk;
        int_mask = NRF_TWIM_INT_STOPPED_MASK | NRF_TWIM_INT_ERROR_MASK;
    } else {
        /* Enable short for LASTX->SUSPEND, interrupt to happen on SUSPENDED or ERROR */
        regs->SHORTS = TWIM_SHORTS_LASTTX_SUSPEND_Msk;
        int_mask = NRF_TWIM_INT_SUSPENDED_MASK | NRF_TWIM_INT_ERROR_MASK;
    }

    i2c->last_error = 0;

    if (os_sem_init(&hal_i2c_sync_sem, 0) != OS_OK) {
        // todo: DEBUG_PANIC
        assert(0);
    }

    regs->EVENTS_ERROR = 0;
    regs->EVENTS_STOPPED = 0;
    regs->EVENTS_SUSPENDED = 0;

    /* Start an I2C transmit transaction */
    hal_i2c_trigger_start(regs, &regs->TASKS_STARTTX);
    //regs->TASKS_STARTTX = 1;

    /* Enable the interrupt for the event which will end the tranaction */
    regs->INTENSET = int_mask;

    /* Pend on semaphore until it is released by the interrupt */
    if (os_sem_pend(&hal_i2c_sync_sem, timo) == OS_TIMEOUT) {
        rc = HAL_I2C_ERR_TIMEOUT;
        console_printf("<>wr timeout: a=%x<>\n",pdata->address);
        goto err;
    }

//    console_printf("<>wr stop a=%x, reg=%x<>\n", pdata->address, pdata->buffer[0]);
    rc = i2c->last_error;
    if (rc != 0) {
        goto err;
    }
    return 0;
err:
    return hal_i2c_handle_errors(i2c, rc);
}

/*
 * Perform a I2C master read transaction using TWIM/EasyDMA
 */
int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                    uint32_t timo, uint8_t last_op)
{
    int rc;
    uint32_t int_mask = 0;
    NRF_TWIM_Type *regs;
    struct nrf52_hal_i2c *i2c;

    /* Resolve the I2C bus */
    rc = hal_i2c_resolve(i2c_num, &i2c);
    if (rc != 0) {
        return rc;
    }
    regs = i2c->nhi_regs;

    /*
     * Configure the RXD registers for EasyDMA access to work with buffers of
     * specific length and address of the slave
     */
    regs->ADDRESS    = pdata->address;
    regs->RXD.MAXCNT = pdata->len;
    regs->RXD.PTR    = (uint32_t)pdata->buffer;
    regs->RXD.LIST   = 0;

    /* Disable and clear interrupts */
    regs->INTENCLR   = NRF_TWIM_ALL_INTS_MASK;
    regs->INTEN      = 0;

    i2c->last_error = 0;

    /*
     * Only set short for RX->STOP for last_op:1 since there is no suspend short
     * available in nrf52832
     */
    if (last_op) {
        /* Enable short for LASTRX->STOP, interrupt to happen on STOPPED or ERROR */
        regs->SHORTS = TWIM_SHORTS_LASTRX_STOP_Msk;
        int_mask = NRF_TWIM_INT_STOPPED_MASK | NRF_TWIM_INT_ERROR_MASK;
    } else {
        /* No short, interrupt to happen on LASTRX or ERROR */
        regs->SHORTS = 0;
        int_mask = NRF_TWIM_INT_LASTRX_MASK | NRF_TWIM_INT_ERROR_MASK;
    }

    regs->EVENTS_ERROR = 0;
    regs->EVENTS_STOPPED = 0;
    regs->EVENTS_RXSTARTED = 0;
    if (regs->EVENTS_SUSPENDED) {
        regs->EVENTS_SUSPENDED = 0;
        regs->TASKS_RESUME = 1;
    }

    /* Start a receive transaction */
    hal_i2c_trigger_start(regs, &regs->TASKS_STARTRX);
    //regs->TASKS_STARTRX = 1;

    /* Enable interrupts */
    regs->INTENSET = int_mask;

    /* Pend on semaphore until it is released by the interrupt */
    if (os_sem_pend(&hal_i2c_sync_sem, timo) == OS_TIMEOUT) {
        rc = HAL_I2C_ERR_TIMEOUT;
        console_printf("<>rd timeout: a=%x<>\n",pdata->address);
        goto err;
    }

//    console_printf("<>rd stop a=%x<>\n", pdata->address);
    rc = i2c->last_error;
    if (rc != 0) {
        goto err;
    }
    return 0;
err:
    return hal_i2c_handle_errors(i2c, rc);
}


/*
 * Perform a I2C master write-read repeated start transaction
 * using TWIM/EasyDMA
 */
int
hal_i2c_master_write_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata, uint32_t timo)
{
    int rc;
    NRF_TWIM_Type *regs;
    struct nrf52_hal_i2c *i2c;

    /* Resolve the I2C bus */
    rc = hal_i2c_resolve(i2c_num, &i2c);
    if (rc != 0) {
        console_printf("<>resolve failed<>\n");
        return rc;
    }
    regs = i2c->nhi_regs;

    /*
     * Configure the TXD and RXD registers for EasyDMA access to work with buffers of
     * specific length and address of the slave
     */
    regs->ADDRESS    = pdata->address;
    regs->TXD.MAXCNT = pdata->len;
    regs->TXD.PTR    = (uint32_t)pdata->buffer;
    regs->TXD.LIST   = 0;
    regs->RXD.MAXCNT = pdata->len2;
    regs->RXD.PTR    = (uint32_t)pdata->buffer2;
    regs->RXD.LIST   = 0;

    /* Disable and clear interrupts */
    regs->INTENCLR   = NRF_TWIM_ALL_INTS_MASK;
    regs->INTEN      = 0;

    /* Enable 2 shorts: LASTTX->STARTRX and LASTRX->STOP */
    regs->SHORTS = TWIM_SHORTS_LASTTX_STARTRX_Msk | TWIM_SHORTS_LASTRX_STOP_Msk;

    i2c->last_error = 0;

    if (os_sem_init(&hal_i2c_sync_sem, 0) != OS_OK) {
        // todo: DEBUG_PANIC
        assert(0);
    }

    regs->EVENTS_ERROR = 0;
    regs->EVENTS_STOPPED = 0;
    regs->EVENTS_SUSPENDED = 0;

    /* Start a transmit-receive transaction */
    hal_i2c_trigger_start(regs, &regs->TASKS_STARTTX);
    //regs->TASKS_STARTTX = 1;

    /* Enable interrupts for STOPPED and ERROR */
    regs->INTENSET = NRF_TWIM_INT_STOPPED_MASK | NRF_TWIM_INT_ERROR_MASK;

    /* Pend on semaphore until it is released by the interrupt */
    if (os_sem_pend(&hal_i2c_sync_sem, timo) == OS_TIMEOUT) {
        rc = HAL_I2C_ERR_TIMEOUT;
        console_printf("<>wrrd timeout: a=%x<>\n",pdata->address);
        goto err;
    }

//    console_printf("<>wrrd stop a=%x r=%x<>\n", pdata->address, pdata->buffer[0]);
    rc = i2c->last_error;
    if (rc != 0) {
        goto err;
    }
    return 0;
err:
    return hal_i2c_handle_errors(i2c, rc);
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

/*
 * Interrupt handler for master I2C transactions using TWIM
 */
void
hal_i2c_irq_handler(struct nrf52_hal_i2c *i2c)
{
    NRF_TWIM_Type *regs;
    regs = i2c->nhi_regs;
    if (regs->EVENTS_ERROR) {
        nrf_twim_event_clear(regs, NRF_TWIM_EVENT_ERROR);
        /*
         * If STOP hasn't occurred, trigger one now. The error
         * source will be processed at the end of this handler,
         * when the STOP interrupt takes place.
         */
        if (!regs->EVENTS_STOPPED) {
            /* Enable the STOPPED event interrupt and resume */
            regs->INTENCLR = NRF_TWIM_ALL_INTS_MASK;
            regs->INTENSET = NRF_TWIM_INT_STOPPED_MASK;
            if (regs->EVENTS_SUSPENDED) {
                regs->EVENTS_SUSPENDED = 0;
                regs->TASKS_RESUME = 1;
            }
            regs->TASKS_STOP = 1;
            return;
        }
    }

    if (regs->EVENTS_STOPPED) {
        nrf_twim_event_clear(regs, NRF_TWIM_EVENT_STOPPED);
        nrf_twim_event_clear(regs, NRF_TWIM_EVENT_LASTTX);
        nrf_twim_event_clear(regs, NRF_TWIM_EVENT_LASTRX);
        regs->SHORTS = 0;
    } else {
        if (regs->EVENTS_LASTRX) {
            /* Handle LASTRX event */
            nrf_twim_event_clear(regs, NRF_TWIM_EVENT_LASTRX);
            regs->TASKS_SUSPEND = 1;
        } else {
            nrf_twim_event_clear(regs, NRF_TWIM_EVENT_SUSPENDED);
        }
    }

    /* Read and clear error source register */
    uint32_t errorsrc = regs->ERRORSRC;
    regs->ERRORSRC = errorsrc;
    i2c->last_error = hal_i2c_convert_status(errorsrc);

    if (os_sem_release(&hal_i2c_sync_sem) != OS_OK) {
        // todo: DEBUG_PANIC
        assert(0);
    }
}
