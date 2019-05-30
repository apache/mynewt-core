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
#include "hal/hal_spi.h"
#include "bus/bus.h"
#include "bus/bus_debug.h"
#include "bus/drivers/spi_hal.h"

static int
bus_spi_init_node(struct bus_dev *bdev, struct bus_node *bnode, void *arg)
{
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct bus_spi_node_cfg *cfg = arg;

    BUS_DEBUG_POISON_NODE(node);

    node->pin_cs = cfg->pin_cs;
    node->mode = cfg->mode;
    node->data_order = cfg->data_order;
    node->freq = cfg->freq;
    node->quirks = cfg->quirks;

    hal_gpio_init_out(node->pin_cs, 1);

    return 0;
}

#if MYNEWT_VAL(SPI_HAL_USE_NOBLOCK)

static void
bus_spi_txrx_cb(void *arg, int len)
{
    struct bus_spi_hal_dev *dev = arg;

    os_sem_release(&dev->sem);
}
#endif

static int
bus_spi_enable(struct bus_dev *bdev)
{
    struct bus_spi_hal_dev *dev = (struct bus_spi_hal_dev *)bdev;
    int rc;

    BUS_DEBUG_VERIFY_DEV(&dev->spi_dev);

#if MYNEWT_VAL(SPI_HAL_USE_NOBLOCK)
    rc = hal_spi_set_txrx_cb(dev->spi_dev.cfg.spi_num, bus_spi_txrx_cb, dev);
    if (rc) {
        hal_spi_disable(dev->spi_dev.cfg.spi_num);
        rc = hal_spi_set_txrx_cb(dev->spi_dev.cfg.spi_num, bus_spi_txrx_cb, dev);
        assert(rc == 0);
   }
#endif

    rc = hal_spi_enable(dev->spi_dev.cfg.spi_num);
    if (rc) {
        return SYS_EINVAL;
    }

    return 0;
}

static int
bus_spi_configure(struct bus_dev *bdev, struct bus_node *bnode)
{
    struct bus_spi_dev *spi_dev = (struct bus_spi_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    struct bus_spi_node *current_node = (struct bus_spi_node *)bdev->configured_for;
    struct hal_spi_settings spi_cfg;
    int rc;

    BUS_DEBUG_VERIFY_DEV(spi_dev);
    BUS_DEBUG_VERIFY_NODE(node);

    /* No need to reconfigure if already configured with the same settings */
    if (current_node && (current_node->mode == node->mode) &&
                        (current_node->data_order == node->data_order) &&
                        (current_node->freq == node->freq)) {
        return 0;
    }

    rc = hal_spi_disable(spi_dev->cfg.spi_num);
    if (rc) {
        goto done;
    }

    spi_cfg.data_mode = node->mode;
    spi_cfg.data_order = node->data_order;
    spi_cfg.baudrate = node->freq;
    /* XXX add support for other word sizes */
    spi_cfg.word_size = HAL_SPI_WORD_SIZE_8BIT;

    rc = hal_spi_config(spi_dev->cfg.spi_num, &spi_cfg);
    if (rc) {
        goto done;
    }

    rc = hal_spi_enable(spi_dev->cfg.spi_num);

done:
    if (rc) {
        rc = SYS_EIO;
    }

    return rc;
}

static int
bus_spi_read(struct bus_dev *bdev, struct bus_node *bnode, uint8_t *buf,
             uint16_t length, os_time_t timeout, uint16_t flags)
{
    struct bus_spi_hal_dev *dev = (struct bus_spi_hal_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    int rc;
    (void)timeout;

    BUS_DEBUG_VERIFY_DEV(&dev->spi_dev);
    BUS_DEBUG_VERIFY_NODE(node);

    hal_gpio_write(node->pin_cs, 0);

    /* Use output buffer as input to generate SPI clock.
     * For security mostly, do not output random data, fill it with 0xFF.
     */
    memset(buf, 0xFF, length);

#if MYNEWT_VAL(SPI_HAL_USE_NOBLOCK)
    rc = hal_spi_txrx_noblock(dev->spi_dev.cfg.spi_num, buf, buf, length);
    if (rc == 0) {
        os_sem_pend(&dev->sem, OS_TIMEOUT_NEVER);
    }
#else
    rc = hal_spi_txrx(dev->spi_dev.cfg.spi_num, buf, buf, length);
#endif

    if (rc || !(flags & BUS_F_NOSTOP)) {
        hal_gpio_write(node->pin_cs, 1);
    }

    return rc;
}

static int
bus_spi_write(struct bus_dev *bdev, struct bus_node *bnode, const uint8_t *buf,
              uint16_t length, os_time_t timeout, uint16_t flags)
{
    struct bus_spi_hal_dev *dev = (struct bus_spi_hal_dev *)bdev;
    struct bus_spi_node *node = (struct bus_spi_node *)bnode;
    int rc;

    BUS_DEBUG_VERIFY_DEV(&dev->spi_dev);
    BUS_DEBUG_VERIFY_NODE(node);

    hal_gpio_write(node->pin_cs, 0);

    /* XXX update HAL to accept const instead */

#if MYNEWT_VAL(SPI_HAL_USE_NOBLOCK)
    rc = hal_spi_txrx_noblock(dev->spi_dev.cfg.spi_num, (uint8_t *)buf, NULL, length);
    if (rc == 0) {
        os_sem_pend(&dev->sem, OS_TIMEOUT_NEVER);
    }
#else
    rc = hal_spi_txrx(dev->spi_dev.cfg.spi_num, (uint8_t *)buf, NULL, length);
#endif

    if (!(flags & BUS_F_NOSTOP)) {
        hal_gpio_write(node->pin_cs, 1);
    }

    return rc;
}

static int bus_spi_disable(struct bus_dev *bdev)
{
    struct bus_spi_dev *spi_dev = (struct bus_spi_dev *)bdev;
    int rc;

    BUS_DEBUG_VERIFY_DEV(spi_dev);

    rc = hal_spi_disable(spi_dev->cfg.spi_num);
    if (rc) {
        return SYS_EINVAL;
    }

    return 0;
}

static const struct bus_dev_ops bus_spi_ops = {
    .init_node = bus_spi_init_node,
    .enable = bus_spi_enable,
    .configure = bus_spi_configure,
    .read = bus_spi_read,
    .write = bus_spi_write,
    .disable = bus_spi_disable,
};

int
bus_spi_hal_dev_init_func(struct os_dev *odev, void *arg)
{
    struct bus_spi_hal_dev *dev = (struct bus_spi_hal_dev *)odev;
    struct bus_spi_dev_cfg *cfg = arg;
    struct hal_spi_hw_settings hal_cfg;
    int rc;

    hal_cfg.pin_sck = cfg->pin_sck;
    hal_cfg.pin_mosi = cfg->pin_mosi;
    hal_cfg.pin_miso = cfg->pin_miso;
    hal_cfg.pin_ss = -1;

    /* XXX we support master only! */
    rc = hal_spi_init_hw(cfg->spi_num, HAL_SPI_TYPE_MASTER, &hal_cfg);
    if (rc) {
        return SYS_EINVAL;
    }

    BUS_DEBUG_POISON_DEV(&dev->spi_dev);

    dev->spi_dev.cfg = *cfg;

#if MYNEWT_VAL(SPI_HAL_USE_NOBLOCK)
    rc = os_sem_init(&dev->sem, 0);
    assert(rc == 0);
#endif

    rc = bus_dev_init_func(odev, (void*)&bus_spi_ops);
    assert(rc == 0);

    return 0;
}
