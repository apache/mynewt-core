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
#include "defs/error.h"
#include "hal/hal_gpio.h"
#include "bus/bus.h"
#include "bus/bus_debug.h"
#include "bus/bus_driver.h"
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/i2c_nrf52_twim.h"
#include "mcu/nrf52_hal.h"
#include "nrfx.h"
#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
#include "stats/stats.h"
#endif

#define TWIM_GPIO_PIN_CNF \
    ((GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) |  \
     (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |      \
     (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |      \
     (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |   \
     (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos))

#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
STATS_SECT_START(twim_stats_section)
    STATS_SECT_ENTRY(sda_lo_err)        /* SDA pulled low on r/w */
    STATS_SECT_ENTRY(sda_lo_err_nrecov) /* SDA pulled low on r/w (not recovered)*/
    STATS_SECT_ENTRY(scl_hi_err)        /* SCL unresponsive */
    STATS_SECT_ENTRY(scl_hi_err_nrecov) /* SCL unresponsive (not recovered)*/
STATS_SECT_END

STATS_NAME_START(twim_stats_section)
    STATS_NAME(twim_stats_section, sda_lo_err)
    STATS_NAME(twim_stats_section, sda_lo_err_nrecov)
    STATS_NAME(twim_stats_section, scl_hi_err)
    STATS_NAME(twim_stats_section, scl_hi_err_nrecov)
STATS_NAME_END(twim_stats_section)
#endif

struct twim {
    NRF_TWIM_Type *nrf_twim;
    int irqn;
    void (* isr)(void);
};

static void twim0_irq_handler(void);
static void twim1_irq_handler(void);

static const struct twim twims[TWIM_COUNT] = {
    {
        .nrf_twim = NRF_TWIM0,
        .irqn = SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQn,
        .isr = twim0_irq_handler,
    },
    {
        .nrf_twim = NRF_TWIM1,
        .irqn = SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn,
        .isr = twim1_irq_handler,
    },
};

struct twim_dev_data {
    struct os_sem sem;
    uint32_t errorsrc;
    bool suspended;

#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
    STATS_SECT_DECL(twim_stats_section) stats;
#endif
};

static struct bus_i2c_dev *twim_devs[TWIM_COUNT];
static struct twim_dev_data twim_devs_data[TWIM_COUNT];

static void
twim_irq_handler(struct bus_i2c_dev *dev)
{
    NRF_TWIM_Type *nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;
    struct twim_dev_data *dd = &twim_devs_data[dev->cfg.i2c_num];

    nrf_twim->INTEN = 0;

    if (nrf_twim->EVENTS_STOPPED) {
        nrf_twim->EVENTS_STOPPED = 0;
    }

    if (nrf_twim->EVENTS_SUSPENDED) {
        nrf_twim->EVENTS_SUSPENDED = 0;
    }

    if (nrf_twim->EVENTS_ERROR) {
        nrf_twim->EVENTS_ERROR = 0;
    }

    dd->errorsrc = nrf_twim->ERRORSRC;
    nrf_twim->ERRORSRC = dd->errorsrc;

    os_sem_release(&dd->sem);
}

static void
twim0_irq_handler(void)
{
    os_trace_isr_enter();

    assert(twim_devs[0]);
    twim_irq_handler(twim_devs[0]);

    os_trace_isr_exit();
}

static void
twim1_irq_handler(void)
{
    os_trace_isr_enter();

    assert(twim_devs[1]);
    twim_irq_handler(twim_devs[1]);

    os_trace_isr_exit();
}

static int
nrf_twim_translate_twim(int twim_err)
{
    if (twim_err & TWIM_ERRORSRC_DNACK_Msk) {
        return SYS_EREMOTEIO;
    } else if (twim_err & TWIM_ERRORSRC_ANACK_Msk) {
        return SYS_ENOENT;
    } else if (twim_err & TWIM_ERRORSRC_OVERRUN_Msk) {
        return SYS_EIO;
    }

    return SYS_EUNKNOWN;
}

static inline NRF_TWIM_Type *
nrf_twim_resolve(int twim_no)
{
    if ((twim_no < 0) || (twim_no >= TWIM_COUNT)) {
        return NULL;
    }

    return twims[twim_no].nrf_twim;
}

static void
nrf_twim_delay_us(uint32_t us)
{
    asm volatile (".syntax unified          \n"
                  "loop: subs %0, %0, #1    \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      nop                \n"
                  "      bne loop           \n"
                  : "+r" (us));
}

static void
nrf_twim_fix_sda(NRF_TWIM_Type *nrf_twim, struct twim_dev_data *dd)
{
    NRF_GPIO_Type *nrf_gpio;
    int pin_scl;
    int pin_sda;
    int i;

    /*
     * TWIM controller won't start when SDA line is pulled low. While unlikely
     * to happen, it has been observed that if MCU is reset while transmission
     * from other device is in progress, such device may be stuck with SDA pulled
     * low as it is waiting for clock pulses on SCL line. To avoid this we just
     * check SDA line and add extra SCL pulses in sw if needed.
     */

    pin_scl = nrf_twim->PSEL.SCL;
    pin_sda = nrf_twim->PSEL.SDA;

    if (hal_gpio_read(pin_sda)) {
        return;
    }

#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
    STATS_INC(dd->stats, sda_lo_err);
#endif

    nrf_twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;

    nrf_gpio = HAL_GPIO_PORT(pin_scl);
    nrf_gpio->DIRSET = 1 << HAL_GPIO_INDEX(pin_scl);

    /* Toggle SCL until SDA is cleared */
    for (i = 0; i < 8; i++) {
        hal_gpio_write(pin_scl, 0);
        nrf_twim_delay_us(4);
        hal_gpio_write(pin_scl, 1);
        nrf_twim_delay_us(4);

        if (hal_gpio_read(pin_sda)) {
            break;
        }
    }

    nrf_gpio->DIRCLR = 1 << HAL_GPIO_INDEX(pin_scl);

    nrf_gpio = HAL_GPIO_PORT(pin_sda);
    nrf_gpio->DIRSET = 1 << HAL_GPIO_INDEX(pin_sda);

    /* Make proper STOP condition */
    hal_gpio_write(pin_sda, 0);
    nrf_twim_delay_us(4);
    hal_gpio_write(pin_sda, 1);

    nrf_gpio->DIRCLR = 1 << HAL_GPIO_INDEX(pin_sda);

    nrf_twim->ENABLE = TWIM_ENABLE_ENABLE_Enabled;

#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
    if (!hal_gpio_read(pin_sda)) {
        STATS_INC(dd->stats, sda_lo_err_nrecov);
    }
#endif
}

static void
nrf_twim_start_task(NRF_TWIM_Type *nrf_twim, struct twim_dev_data *dd,
                    __O uint32_t *task_start, __IO uint32_t *event_last)
{
    const int max_attempt = 2;
    uint32_t end_ticks;
    int attempt = 1;
    int pin_scl;

    pin_scl = nrf_twim->PSEL.SCL;

    /*
     * TWIM controller seems to have the same issue as TWI controller which makes
     * it unresponsive on glitch on I2C bus. It has been observed that LP5523
     * releases SDA line mid-ack which looks like premature STOP condition on bus
     * followed by STOP condition from TWIM controller. After this sequence,
     * TWIM controller needs to be reset in order to work properly.
     *
     * To workaround this problem we can just check if SCL line is pulled low
     * after triggering start task as this indicates some activity and means
     * that controller is responsive.
     */

    while (1) {
        *event_last = 0;
        *task_start = 1;

        end_ticks = os_cputime_get32() +
                    os_cputime_usecs_to_ticks(MYNEWT_VAL(I2C_NRF52_TWIM_SCL_RECOVERY_DELAY_USEC));

        do {
            /*
             * Wait for either low state on SCL or last byte event, in case
             * we started polling after activity on bus has already finished.
             */
            if (!hal_gpio_read(pin_scl) || *event_last) {
                return;
            }
        } while (CPUTIME_LT(os_cputime_get32(), end_ticks));

#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
        if (attempt == 1) {
            STATS_INC(dd->stats, scl_hi_err);
        }
#endif

        attempt++;

        if (attempt > max_attempt) {
            break;
        }

        /*
         * Need to explicitly stop here as otherwise TWIM will send address on
         * the bus just after being enabled again.
         */
        nrf_twim->TASKS_STOP = 1;
        nrf_twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
        nrf_twim->ENABLE = TWIM_ENABLE_ENABLE_Enabled;
    };

#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
    STATS_INC(dd->stats, scl_hi_err_nrecov);
#endif
}

static int
bus_i2c_nrf52_twim_init_node(struct bus_dev *bdev, struct bus_node *bnode, void *arg)
{
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;
    struct bus_i2c_node_cfg *cfg = arg;

    BUS_DEBUG_POISON_NODE(node);

    node->freq = cfg->freq;
    node->addr = cfg->addr;
    node->quirks = cfg->quirks;

    return 0;
}

static int
bus_i2c_nrf52_twim_enable(struct bus_dev *bdev)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    NRF_TWIM_Type *nrf_twim;

    BUS_DEBUG_VERIFY_DEV(dev);

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;

    nrf_twim->ENABLE = TWIM_ENABLE_ENABLE_Enabled;
    nrf_twim->INTEN = 0;

    return 0;
}

static int
bus_i2c_nrf52_twim_configure(struct bus_dev *bdev, struct bus_node *bnode)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;
    struct bus_i2c_node *current_node = (struct bus_i2c_node *)bdev->configured_for;
    NRF_TWIM_Type *nrf_twim;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;

    nrf_twim->ADDRESS = node->addr;

    if (current_node && (current_node->freq == node->freq)) {
        return 0;
    }

    rc = 0;

    switch (node->freq) {
    case 100:
        nrf_twim->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K100;
        break;
    case 250:
        nrf_twim->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K250;
        break;
    case 380:
        nrf_twim->FREQUENCY = TWIM_CUSTOM_FREQUENCY_FREQUENCY_K380;
        break;
    case 400:
        nrf_twim->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K400;
        break;
    default:
        rc = SYS_EIO;
    }

    return rc;
}


static int
bus_i2c_nrf52_twim_read(struct bus_dev *bdev, struct bus_node *bnode,
                        uint8_t *buf, uint16_t length, os_time_t timeout,
                        uint16_t flags)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;
    struct twim_dev_data *dd;
    NRF_TWIM_Type *nrf_twim;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    if (flags & BUS_F_NOSTOP) {
        /*
         * There's no shortcut available for LASTRX->SUSPEND so we can only
         * stop after receiving last byte - return not supported if NOSTOP was
         * requested for this read.
         *
         * XXX we may use PPI to workaround for missing shortcut but it's
         * probably not really that useful and not worth the effort.
         */
        return SYS_ENOTSUP;
    }

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;
    dd = &twim_devs_data[dev->cfg.i2c_num];

    if (!dd->suspended) {
        nrf_twim_fix_sda(nrf_twim, dd);
    }

    nrf_twim->RXD.PTR = (uint32_t)buf;
    nrf_twim->RXD.MAXCNT = length;
    nrf_twim->RXD.LIST = 0;
    nrf_twim->INTEN = TWIM_INTEN_ERROR_Msk | TWIM_INTENCLR_STOPPED_Msk;
    nrf_twim->SHORTS = TWIM_SHORTS_LASTRX_STOP_Msk;

    nrf_twim->EVENTS_STOPPED = 0;
    nrf_twim->EVENTS_ERROR = 0;
    nrf_twim->EVENTS_SUSPENDED = 0;
    nrf_twim->EVENTS_RXSTARTED = 0;
    nrf_twim->TASKS_RESUME = 1;

    nrf_twim_start_task(nrf_twim, dd, &nrf_twim->TASKS_STARTRX,
                        &nrf_twim->EVENTS_LASTRX);

    rc = os_sem_pend(&dd->sem, timeout);
    nrf_twim->INTEN = 0;
    if (rc == OS_TIMEOUT) {
        rc = SYS_ETIMEOUT;
    } else if (rc) {
        rc = SYS_EUNKNOWN;
    } else if (dd->errorsrc) {
        rc = nrf_twim_translate_twim(dd->errorsrc);
    }

    if (rc) {
        nrf_twim->TASKS_RESUME = 1;
        nrf_twim->TASKS_STOP = 1;
    }

    dd->suspended = false;

    return rc;
}

static int
bus_i2c_nrf52_twim_write(struct bus_dev *bdev, struct bus_node *bnode,
                         const uint8_t *buf, uint16_t length, os_time_t timeout,
                         uint16_t flags)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;
    struct twim_dev_data *dd;
    NRF_TWIM_Type *nrf_twim;
    uint8_t last_op;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    last_op = !(flags & BUS_F_NOSTOP);

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;
    dd = &twim_devs_data[dev->cfg.i2c_num];

    if (!dd->suspended) {
        nrf_twim_fix_sda(nrf_twim, dd);
    }

    nrf_twim->TXD.MAXCNT = length;
    nrf_twim->INTEN = 0;

    nrf_twim->TXD.PTR = (uint32_t)buf;
    nrf_twim->TXD.LIST = 0;
    nrf_twim->INTENSET = TWIM_INTEN_ERROR_Msk;
    if (last_op) {
        nrf_twim->INTENSET = TWIM_INTENSET_STOPPED_Msk;
        nrf_twim->SHORTS = TWIM_SHORTS_LASTTX_STOP_Msk;
    } else {
        nrf_twim->INTENSET = TWIM_INTENSET_SUSPENDED_Msk;
        nrf_twim->SHORTS = TWIM_SHORTS_LASTTX_SUSPEND_Msk;
    }

    nrf_twim->EVENTS_ERROR = 0;
    nrf_twim->EVENTS_STOPPED = 0;
    nrf_twim->EVENTS_SUSPENDED = 0;
    nrf_twim->EVENTS_TXSTARTED = 0;
    nrf_twim->TASKS_RESUME = 1;

    nrf_twim_start_task(nrf_twim, dd, &nrf_twim->TASKS_STARTTX,
                        &nrf_twim->EVENTS_LASTTX);

    rc = os_sem_pend(&dd->sem, timeout);
    nrf_twim->INTEN = 0;
    if (rc == OS_TIMEOUT) {
        rc = SYS_ETIMEOUT;
    } else if (rc) {
        rc = SYS_EUNKNOWN;
    } else if (dd->errorsrc) {
        rc = nrf_twim_translate_twim(dd->errorsrc);
    }

    if (rc) {
        nrf_twim->TASKS_RESUME = 1;
        nrf_twim->TASKS_STOP = 1;
    }

    dd->suspended = !rc && !last_op;

    return rc;
}

static int bus_i2c_nrf52_twim_disable(struct bus_dev *bdev)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    NRF_TWIM_Type *nrf_twim;

    BUS_DEBUG_VERIFY_DEV(dev);

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;
    nrf_twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;

    return 0;
}

static const struct bus_dev_ops bus_i2c_nrf52_twim_ops = {
    .init_node = bus_i2c_nrf52_twim_init_node,
    .enable = bus_i2c_nrf52_twim_enable,
    .configure = bus_i2c_nrf52_twim_configure,
    .read = bus_i2c_nrf52_twim_read,
    .write = bus_i2c_nrf52_twim_write,
    .disable = bus_i2c_nrf52_twim_disable,
};

int
bus_i2c_nrf52_twim_dev_init_func(struct os_dev *odev, void *arg)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)odev;
    struct bus_i2c_dev_cfg *cfg = arg;
    const struct twim *twim;
    struct twim_dev_data *dd;
    NRF_TWIM_Type *nrf_twim;
    NRF_GPIO_Type *nrf_gpio;
#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
    char *stats_name;
#endif
    int rc;

    BUS_DEBUG_POISON_DEV(dev);

    if (twim_devs[cfg->i2c_num]) {
        return SYS_EALREADY;
    }

    nrf_twim = nrf_twim_resolve(cfg->i2c_num);
    if (!nrf_twim) {
        return SYS_ENODEV;
    }

    twim = &twims[cfg->i2c_num];
    dd = &twim_devs_data[cfg->i2c_num];

    dev->cfg = *cfg;
    twim_devs[cfg->i2c_num] = dev;

    /*
     * Setup GPIOs for SCL and SDA so they are in proper state when TWIM
     * controller is disabled.
     */
    nrf_gpio = HAL_GPIO_PORT(cfg->pin_scl);
    nrf_gpio->PIN_CNF[HAL_GPIO_INDEX(cfg->pin_scl)] = TWIM_GPIO_PIN_CNF;
    nrf_gpio = HAL_GPIO_PORT(cfg->pin_sda);
    nrf_gpio->PIN_CNF[HAL_GPIO_INDEX(cfg->pin_sda)] = TWIM_GPIO_PIN_CNF;
    hal_gpio_write(cfg->pin_scl, 1);
    hal_gpio_write(cfg->pin_sda, 1);

    NVIC_DisableIRQ(twim->irqn);
    NVIC_SetVector(twim->irqn, (uint32_t)twim->isr);
    NVIC_SetPriority(twim->irqn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_ClearPendingIRQ(twim->irqn);
    NVIC_EnableIRQ(twim->irqn);

    nrf_twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
    nrf_twim->PSEL.SCL = cfg->pin_scl;
    nrf_twim->PSEL.SDA = cfg->pin_sda;
    nrf_twim->FREQUENCY = 0;

    nrf_twim_fix_sda(nrf_twim, dd);

    os_sem_init(&dd->sem, 0);

#if MYNEWT_VAL(I2C_NRF52_TWIM_STAT)
    asprintf(&stats_name, "i2c_nrf52_twim%d", cfg->i2c_num);
    /* XXX should we assert or return error on failure? */
    stats_init_and_reg(STATS_HDR(dd->stats),
                       STATS_SIZE_INIT_PARMS(dd->stats, STATS_SIZE_32),
                       STATS_NAME_INIT_PARMS(twim_stats_section),
                       stats_name);
#endif

    rc = bus_dev_init_func(odev, (void*)&bus_i2c_nrf52_twim_ops);
    assert(rc == 0);

    return 0;
}
