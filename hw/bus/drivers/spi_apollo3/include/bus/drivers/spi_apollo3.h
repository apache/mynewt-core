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

#ifndef HW_BUS_DRIVERS_SPI_APOLLO3_H_
#define HW_BUS_DRIVERS_SPI_APOLLO3_H_

#include <stddef.h>
#include <stdint.h>
#include "os/os_dev.h"
#include "bus/drivers/spi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bus_spi_apollo3_dev {
    struct bus_spi_dev spi_dev;
#if MYNEWT_VAL(SPI_HAL_USE_NOBLOCK)
    struct os_sem sem;
#endif
};

/**
 * Initialize os_dev as SPI bus device the Apollo3 IOM SPI driver
 *
 * This can be passed as a parameter to os_dev_create() when creating os_dev
 * object for SPI device, however it's recommended to create devices using helper
 * like bus_spi_hal_dev_create().
 *
 * @param node  Node device object
 * @param arg   Node configuration struct (struct bus_node_cfg)
 */
int
bus_spi_apollo3_dev_init_func(struct os_dev *odev, void *arg);

/**
 * Create SPI bus device for the Apollo3 IOM SPI driver
 *
 * This is a convenient helper and recommended way to create os_dev for bus SPI
 * device instead of calling os_dev_create() directly.
 *
 * @param name  Name of device
 * @param dev   Device state object
 * @param cfg   Configuration
 */
static inline int
bus_spi_apollo3_dev_create(const char *name, struct bus_spi_apollo3_dev *dev,
                           struct bus_spi_dev_cfg *cfg)
{
    struct os_dev *odev = (struct os_dev *)dev;

    return os_dev_create(odev, name, OS_DEV_INIT_PRIMARY, 0,
                         bus_spi_apollo3_dev_init_func, cfg);
}

#ifdef __cplusplus
}
#endif

#endif /* HW_BUS_DRIVERS_SPI_APOLLO3_H_ */
