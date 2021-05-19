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
#include <assert.h>

#include "os/mynewt.h"
#include <bsp/bsp.h>
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include <spiram/spiram.h>
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/spi_common.h>
#endif

static inline int
spiram_lock(struct spiram_dev *dev)
{
#if MYNEWT_VAL(OS_SCHEDULING)
    return os_mutex_pend(&dev->lock,
                         os_time_ms_to_ticks32(MYNEWT_VAL(SPIRAM_LOCK_TIMEOUT)));
#else
    return 0;
#endif
}

static inline void
spiram_unlock(struct spiram_dev *dev)
{
#if MYNEWT_VAL(OS_SCHEDULING)
    int rc = os_mutex_release(&dev->lock);
    assert(rc == 0);
#endif
}

void
spiram_cs_activate(struct spiram_dev *dev)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node *node = (struct bus_spi_node *)&dev->dev;
    hal_gpio_write(node->pin_cs, 0);
#else
    hal_gpio_write(dev->ss_pin, 0);
#endif
}

void
spiram_cs_deactivate(struct spiram_dev *dev)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node *node = &dev->dev;
    hal_gpio_write(node->pin_cs, 1);
#else
    hal_gpio_write(dev->ss_pin, 1);
#endif
}

int
spiram_write_enable(struct spiram_dev *dev)
{
    int rc;
    bool locked;
    rc = spiram_lock(dev);
    locked = rc == 0;

    if (dev->characteristics->write_enable_cmd) {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
        rc = bus_node_simple_write((struct os_dev *)&dev->dev,
                              &dev->characteristics->write_enable_cmd, 1);
#else
        spiram_cs_activate(dev);

        rc = hal_spi_tx_val(dev->spi_num, dev->characteristics->write_enable_cmd);

        spiram_cs_deactivate(dev);
#endif
    }

    if (locked) {
        spiram_unlock(dev);
    }

    return rc;
}

int
spiram_read(struct spiram_dev *dev, uint32_t addr, void *buf, uint32_t size)
{
    uint8_t cmd[5] = { SPIRAM_READ };
    int i;
    int rc;
    int cmd_size = 1 + dev->characteristics->address_bytes + dev->characteristics->dummy_bytes;
    bool locked;

    i = dev->characteristics->address_bytes;
    while (i) {
        cmd[i] = (uint8_t)addr;
        addr >>= 8;
        --i;
    }
    rc = spiram_lock(dev);
    locked = rc == 0;

    if (size > 0) {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
        rc = bus_node_simple_write_read_transact((struct os_dev *)&dev->dev,
                                                 &cmd, cmd_size, buf, size);
#else
        spiram_cs_activate(dev);

        /* Send command + address */
        rc = hal_spi_txrx(dev->spi_num, cmd, NULL, cmd_size);
        if (rc == 0) {
            /* For security mostly, do not output random data, fill it with FF */
            memset(buf, 0xFF, size);
            /* Tx buf does not matter, for simplicity pass read buffer */
            rc = hal_spi_txrx(dev->spi_num, buf, buf, size);
        }

        spiram_cs_deactivate(dev);
#endif
    }

    if (locked) {
        spiram_unlock(dev);
    }

    return rc;
}

int
spiram_write(struct spiram_dev *dev, uint32_t addr, void *buf, uint32_t size)
{
    uint8_t cmd[4] = { SPIRAM_WRITE };
    int rc;
    int i;
    bool locked;

    rc = spiram_lock(dev);
    locked = rc == 0;

    if (size) {
        i = dev->characteristics->address_bytes;
        while (i) {
            cmd[i] = (uint8_t)addr;
            addr >>= 8;
            --i;
        }
        spiram_write_enable(dev);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
        rc = bus_node_lock((struct os_dev *)&dev->dev,
                           BUS_NODE_LOCK_DEFAULT_TIMEOUT);
        if (rc == 0) {
            rc = bus_node_write((struct os_dev *)&dev->dev,
                                cmd, dev->characteristics->address_bytes + 1,
                                BUS_NODE_LOCK_DEFAULT_TIMEOUT, BUS_F_NOSTOP);
            if (rc == 0) {
                rc = bus_node_simple_write((struct os_dev *)&dev->dev, buf, size);
            }
            (void)bus_node_unlock((struct os_dev *)&dev->dev);
        }
#else
        spiram_cs_activate(dev);
        rc = hal_spi_txrx(dev->spi_num, cmd, NULL, dev->characteristics->address_bytes + 1);
        if (rc == 0) {
            rc = hal_spi_txrx(dev->spi_num, buf, NULL, size);
        }
        spiram_cs_deactivate(dev);
#endif
    }
    if (locked) {
        spiram_unlock(dev);
    }

    return rc;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
}

int
spiram_create_spi_dev(struct spiram_dev *dev, const char *name,
                      const struct spiram_cfg *spi_cfg)
{
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };

    bus_node_set_callbacks((struct os_dev *)dev, &cbs);

    dev->characteristics = spi_cfg->characteristics;

    return bus_spi_node_create(name, &dev->dev, &spi_cfg->node_cfg, NULL);
}

#else

static int
spiram_init(struct os_dev *odev, void *arg)
{
    int rc;
    struct spiram_dev *dev = (struct spiram_dev *)odev;

    hal_gpio_init_out(dev->ss_pin, 1);

    (void)hal_spi_disable(dev->spi_num);

    rc = hal_spi_config(dev->spi_num, &dev->spi_settings);
    if (rc) {
        goto end;
    }

    hal_spi_set_txrx_cb(dev->spi_num, NULL, NULL);
    rc = hal_spi_enable(dev->spi_num);
end:
    return rc;
}

int
spiram_create_spi_dev(struct spiram_dev *dev, const char *name,
                      const struct spiram_cfg *spi_cfg)
{
    int rc;

    dev->spi_num = spi_cfg->spi_num;
    dev->characteristics = spi_cfg->characteristics;
    dev->ss_pin = spi_cfg->ss_pin;
    dev->spi_settings = spi_cfg->spi_settings;

    rc = os_dev_create(&dev->dev, name, OS_DEV_INIT_SECONDARY, 0, spiram_init, NULL);

    return rc;
}

#endif

#if MYNEWT_VAL(SPIRAM_0)
struct spiram_dev spiram_0;
struct spiram_characteristics spiram_0_char = {
    .address_bytes = MYNEWT_VAL(SPIRAM_0_ADDRESS_BYTES),
    .dummy_bytes = MYNEWT_VAL(SPIRAM_0_DUMMY_BYTES),
    .size = MYNEWT_VAL(SPIRAM_0_SIZE),
    .write_enable_cmd = MYNEWT_VAL(SPIRAM_0_WRITE_ENABLE_CMD),
    .hibernate_cmd = MYNEWT_VAL(SPIRAM_0_HIBERNATE_CMD),
};

struct spiram_cfg spiram_0_cfg = {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    .node_cfg = {
        .node_cfg.lock_timeout_ms = 1000,
        .node_cfg.bus_name = MYNEWT_VAL(SPIRAM_0_SPI_BUS),
        .freq = MYNEWT_VAL(SPIRAM_0_BAUDRATE),
        .data_order = BUS_SPI_DATA_ORDER_MSB,
        .mode = BUS_SPI_MODE_0,
        .pin_cs = MYNEWT_VAL(SPIRAM_0_CS_PIN),
    },
#else
    .spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .baudrate = MYNEWT_VAL(SPIRAM_0_BAUDRATE),
        .data_mode = HAL_SPI_MODE0,
        .word_size = HAL_SPI_WORD_SIZE_8BIT,
    },
    .spi_num = MYNEWT_VAL(SPIRAM_0_SPI_NUM),
    .ss_pin = MYNEWT_VAL(SPIRAM_0_CS_PIN),
#endif
    .characteristics = &spiram_0_char,
};

void
spiram_init(void)
{
    int rc;

    rc = spiram_create_spi_dev(&spiram_0, MYNEWT_VAL(SPIRAM_0_NAME), &spiram_0_cfg);

    assert(rc == 0);
}

#endif
