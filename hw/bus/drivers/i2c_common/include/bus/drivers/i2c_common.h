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

#ifndef HW_BUS_DRIVERS_I2C_COMMON_H_
#define HW_BUS_DRIVERS_I2C_COMMON_H_

#include <stddef.h>
#include <stdint.h>
#include "bus/bus.h"
#include "bus/bus_driver.h"
#include "bus/bus_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUS_I2C_QUIRK_NEED_RESET_ON_TMO     0x0001

/**
 * Bus I2C device configuration
 */
struct bus_i2c_dev_cfg {
    /** I2C interface number */
    int i2c_num;
    /** GPIO number of SDA line */
    int pin_sda;
    /** GPIO number of SCL line */
    int pin_scl;
};

/**
 * Bus I2C device object state
 *
 * Contents of these objects are managed internally by bus driver and shall not
 * be accessed directly.
 */
struct bus_i2c_dev {
    struct bus_dev bdev;
    struct bus_i2c_dev_cfg cfg;
    /** I2C address */
    uint16_t addr;
    /** I2C frequency in kHz */
    uint16_t freq;

#if MYNEWT_VAL(BUS_DEBUG_OS_DEV)
    uint32_t devmagic;
#endif
};

/**
 * Bus I2C node configuration
 */
struct bus_i2c_node_cfg {
    /** General node configuration */
    struct bus_node_cfg node_cfg;
    /** I2C address of node */
    uint8_t addr;
    /** I2C frequency to be used for node */
    uint16_t freq;
    /** Quirks to be applied for device */
    uint16_t quirks;
};

/**
 * Bus I2C node object state
 *
 * Contents of these objects are managed internally by bus driver and shall not
 * be accessed directly.
 */
struct bus_i2c_node {
    struct bus_node bnode;
    uint16_t freq;
    uint16_t quirks;
    uint8_t addr;

#if MYNEWT_VAL(BUS_DEBUG_OS_DEV)
    uint32_t nodemagic;
#endif
};

struct i2c_dev_ops {
    struct bus_dev_ops bus_ops;
    int (*probe)(struct bus_i2c_dev *dev, uint16_t address, os_time_t timeout);
};

/**
 * Create bus I2C node
 *
 * This is a convenient helper and recommended way to create os_dev for bus I2C
 * node instead of calling os_dev_create() directly.
 *
 * @param name  Name of device
 * @param node  Node state object
 * @param cfg   Configuration
 */
static inline int
bus_i2c_node_create(const char *name, struct bus_i2c_node *node,
                    const struct bus_i2c_node_cfg *cfg, void *arg)
{
    struct bus_node *bnode = (struct bus_node *)node;
    struct os_dev *odev = (struct os_dev *)node;

    bnode->init_arg = arg;

    return os_dev_create(odev, name, OS_DEV_INIT_PRIMARY, 1,
                         bus_node_init_func, (void *)cfg);
}

static inline int
bus_i2c_probe(struct bus_i2c_dev *dev, uint16_t address, int timeout)
{
    struct i2c_dev_ops *i2c_ops = (struct i2c_dev_ops *)dev->bdev.dops;

    return i2c_ops->probe ? i2c_ops->probe(dev, address, timeout) : SYS_ENOTSUP;
}

#ifdef __cplusplus
}
#endif

#endif /* HW_BUS_DRIVERS_I2C_COMMON_H_ */
