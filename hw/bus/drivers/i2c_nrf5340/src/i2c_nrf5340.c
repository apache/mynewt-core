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
#include <defs/error.h>
#include <hal/hal_gpio.h>
#include <bus/bus.h>
#include <bus/bus_debug.h>
#include <bus/bus_driver.h>
#include <bus/drivers/i2c_common.h>
#include <mcu/nrf5340_hal.h>
#include <nrfx.h>

#define TWIM_GPIO_PIN_CNF \
    ((GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) |  \
     (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |      \
     (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |      \
     (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |   \
     (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos))

struct twim {
    NRF_TWIM_Type *nrf_twim;
    int irqn;
    void (* isr)(void);
};

static void twim0_irq_handler(void);
static void twim1_irq_handler(void);
static void twim2_irq_handler(void);
static void twim3_irq_handler(void);

static const struct twim twims[TWIM_COUNT] = {
    {
        .nrf_twim = NRF_TWIM0_S,
        .irqn = SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn,
        .isr = twim0_irq_handler,
    },
    {
        .nrf_twim = NRF_TWIM1_S,
        .irqn = SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn,
        .isr = twim1_irq_handler,
    },
    {
        .nrf_twim = NRF_TWIM2_S,
        .irqn = SPIM2_SPIS2_TWIM2_TWIS2_UARTE2_IRQn,
        .isr = twim2_irq_handler,
    },
    {
        .nrf_twim = NRF_TWIM3_S,
        .irqn = SPIM3_SPIS3_TWIM3_TWIS3_UARTE3_IRQn,
        .isr = twim3_irq_handler,
    },
};

struct twim_dev_data {
    struct os_sem sem;
    uint32_t errorsrc;
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

static void
twim2_irq_handler(void)
{
    os_trace_isr_enter();

    assert(twim_devs[2]);
    twim_irq_handler(twim_devs[2]);

    os_trace_isr_exit();
}

static void
twim3_irq_handler(void)
{
    os_trace_isr_enter();

    assert(twim_devs[3]);
    twim_irq_handler(twim_devs[3]);

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

static int
bus_i2c_nrf5340_init_node(struct bus_dev *bdev, struct bus_node *bnode, void *arg)
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
bus_i2c_nrf5340_enable(struct bus_dev *bdev)
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
bus_i2c_nrf5340_configure_controller(struct bus_i2c_dev *dev, uint8_t address, uint16_t freq)
{
    NRF_TWIM_Type *nrf_twim;
    int rc = 0;

    if (dev->freq == freq && dev->addr == address) {
        goto end;
    }

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;

    switch (freq) {
    case 100:
        nrf_twim->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K100;
        break;
    case 250:
        nrf_twim->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K250;
        break;
    case 400:
        nrf_twim->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K400;
        break;
    case 1000:
        nrf_twim->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K1000;
        break;
    default:
        rc = SYS_EIO;
    }

    if (rc == 0) {
        nrf_twim->ADDRESS = address;
        dev->addr = address;
        dev->freq = freq;
    }
end:
    return rc;
}

static int
bus_i2c_nrf5340_configure(struct bus_dev *bdev, struct bus_node *bnode)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    struct bus_i2c_node *node = (struct bus_i2c_node *)bnode;

    BUS_DEBUG_VERIFY_DEV(dev);
    BUS_DEBUG_VERIFY_NODE(node);

    return bus_i2c_nrf5340_configure_controller(dev, node->addr, node->freq);
}

static int
bus_i2c_nrf5340_read(struct bus_dev *bdev, struct bus_node *bnode,
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

    if (!nrfx_is_in_ram(buf)) {
        return SYS_EINVAL;
    }

    if (flags & BUS_F_NOSTOP) {
        /*
         * There's no shortcut available for LASTRX->SUSPEND so we can only
         * stop after receiving last byte - return not supported if NOSTOP was
         * requested for this read.
         */
        assert(0);
        return SYS_ENOTSUP;
    }

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;
    dd = &twim_devs_data[dev->cfg.i2c_num];

    nrf_twim->RXD.PTR = (uint32_t)buf;
    nrf_twim->RXD.MAXCNT = length;
    nrf_twim->RXD.LIST = 0;

    nrf_twim->EVENTS_STOPPED = 0;
    nrf_twim->EVENTS_ERROR = 0;
    nrf_twim->EVENTS_SUSPENDED = 0;
    nrf_twim->EVENTS_RXSTARTED = 0;
    nrf_twim->EVENTS_LASTRX = 0;

    nrf_twim->INTEN = TWIM_INTEN_ERROR_Msk | TWIM_INTENCLR_STOPPED_Msk;
    nrf_twim->SHORTS = TWIM_SHORTS_LASTRX_STOP_Msk;

    nrf_twim->TASKS_RESUME = 1;
    nrf_twim->TASKS_STARTRX = 1;

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

    return rc;
}

static int
bus_i2c_nrf5340_write(struct bus_dev *bdev, struct bus_node *bnode,
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

    if (!nrfx_is_in_ram(buf)) {
        return SYS_EINVAL;
    }

    last_op = !(flags & BUS_F_NOSTOP);

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;
    dd = &twim_devs_data[dev->cfg.i2c_num];

    nrf_twim->TXD.MAXCNT = length;
    nrf_twim->INTEN = 0;

    nrf_twim->TXD.PTR = (uint32_t)buf;
    nrf_twim->TXD.LIST = 0;

    nrf_twim->EVENTS_ERROR = 0;
    nrf_twim->EVENTS_STOPPED = 0;
    nrf_twim->EVENTS_SUSPENDED = 0;
    nrf_twim->EVENTS_TXSTARTED = 0;
    nrf_twim->EVENTS_LASTTX = 0;

    nrf_twim->INTEN = TWIM_INTEN_ERROR_Msk;
    if (last_op) {
        nrf_twim->INTENSET = TWIM_INTENSET_STOPPED_Msk;
        nrf_twim->SHORTS = TWIM_SHORTS_LASTTX_STOP_Msk;
    } else {
        nrf_twim->INTENSET = TWIM_INTENSET_SUSPENDED_Msk;
        nrf_twim->SHORTS = TWIM_SHORTS_LASTTX_SUSPEND_Msk;
    }
    nrf_twim->TASKS_RESUME = 1;
    nrf_twim->TASKS_STARTTX = 1;

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

    return rc;
}

static int
bus_i2c_nrf5340_disable(struct bus_dev *bdev)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)bdev;
    NRF_TWIM_Type *nrf_twim;

    BUS_DEBUG_VERIFY_DEV(dev);

    nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;
    nrf_twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;

    return 0;
}

static int
bus_i2c_nrf5340_probe(struct bus_i2c_dev *dev, uint16_t address, os_time_t timeout)
{
    struct twim_dev_data *dd;
    NRF_TWIM_Type *nrf_twim;
    int rc;

    BUS_DEBUG_VERIFY_DEV(dev);

    rc = os_error_to_sys(os_mutex_pend(&dev->bdev.lock, timeout));
    if (rc == 0) {
        rc = bus_i2c_nrf5340_configure_controller(dev, address, 100);
        assert(rc == 0);
        nrf_twim = twims[dev->cfg.i2c_num].nrf_twim;
        dd = &twim_devs_data[dev->cfg.i2c_num];

        nrf_twim->TXD.MAXCNT = 0;
        nrf_twim->TXD.LIST = 0;

        nrf_twim->EVENTS_STOPPED = 0;
        nrf_twim->EVENTS_ERROR = 0;

        nrf_twim->SHORTS = 0;
        nrf_twim->INTEN = TWIM_INTEN_ERROR_Msk;

        nrf_twim->TASKS_STARTTX = 1;

        /* Wait for enough time to have NACK detected */
        os_cputime_delay_usecs(125);

        /* If semaphore is signaled, there was an error. */
        rc = os_sem_pend(&dd->sem, 0);
        /* Stop condition is not automatically send */
        nrf_twim->INTEN = TWIM_INTEN_STOPPED_Msk;
        nrf_twim->TASKS_STOP = 1;

        /*
         * Previous os_sem_pend() returning OS_TIMEOUT means that
         * device ACK'ed address.
         */
        if (rc == OS_TIMEOUT) {
            rc = 0;
        } else {
            rc = SYS_ENOENT;
        }
        /* Wait for STOP condition */
        (void)os_sem_pend(&dd->sem, OS_TICKS_PER_SEC);

        (void)os_mutex_release(&dev->bdev.lock);
    }

    return rc;
}

static const struct i2c_dev_ops bus_i2c_nrf5340_ops = {
    .bus_ops = {
        .init_node = bus_i2c_nrf5340_init_node,
        .enable = bus_i2c_nrf5340_enable,
        .configure = bus_i2c_nrf5340_configure,
        .read = bus_i2c_nrf5340_read,
        .write = bus_i2c_nrf5340_write,
        .disable = bus_i2c_nrf5340_disable,
    },
    .probe = bus_i2c_nrf5340_probe,
};

#include <hal/hal_gpio.h>

int
bus_i2c_nrf5340_dev_init_func(struct os_dev *odev, void *arg)
{
    struct bus_i2c_dev *dev = (struct bus_i2c_dev *)odev;
    struct bus_i2c_dev_cfg *cfg = arg;
    const struct twim *twim;
    struct twim_dev_data *dd;
    NRF_TWIM_Type *nrf_twim;
    NRF_GPIO_Type *nrf_gpio;
    int rc;

    BUS_DEBUG_POISON_DEV(dev);

    if (twim_devs[cfg->i2c_num]) {
        return SYS_EALREADY;
    }

    {
        hal_gpio_init_out(cfg->pin_scl, 1);
        hal_gpio_init_out(cfg->pin_scl, 0);
        hal_gpio_init_out(cfg->pin_scl, 1);
        hal_gpio_init_out(cfg->pin_scl, 0);
        hal_gpio_init_out(cfg->pin_scl, 1);
        hal_gpio_init_out(cfg->pin_scl, 0);

        hal_gpio_init_out(cfg->pin_sda, 1);
        hal_gpio_init_out(cfg->pin_sda, 0);
        hal_gpio_init_out(cfg->pin_sda, 1);
        hal_gpio_init_out(cfg->pin_sda, 0);
        hal_gpio_init_out(cfg->pin_sda, 1);
        hal_gpio_init_out(cfg->pin_sda, 0);
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

    os_sem_init(&dd->sem, 0);

    rc = bus_dev_init_func(odev, (void*)&bus_i2c_nrf5340_ops);
    assert(rc == 0);

    return 0;
}
