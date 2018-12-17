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

#ifndef HW_BUS_DRIVERS_SPI_COMMON_H_
#define HW_BUS_DRIVERS_SPI_COMMON_H_

#include <stddef.h>
#include <stdint.h>
#include "bus/bus.h"
#include "bus/bus_driver.h"
#include "bus/bus_debug.h"
#include "hal/hal_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bus_spi_dev_cfg {
    int spi_num;
    int pin_sck;
    int pin_mosi;
    int pin_miso;
};

struct bus_spi_dev {
    struct bus_dev bdev;
    struct bus_spi_dev_cfg cfg;

#if MYNEWT_VAL(BUS_DEBUG_OS_DEV)
    uint32_t devmagic;
#endif
};

#define BUS_SPI_MODE_0              (HAL_SPI_MODE0)
#define BUS_SPI_MODE_1              (HAL_SPI_MODE1)
#define BUS_SPI_MODE_2              (HAL_SPI_MODE2)
#define BUS_SPI_MODE_3              (HAL_SPI_MODE3)

#define BUS_SPI_DATA_ORDER_LSB      (HAL_SPI_LSB_FIRST)
#define BUS_SPI_DATA_ORDER_MSB      (HAL_SPI_MSB_FIRST)

struct bus_spi_node_cfg {
    /** General node configuration */
    struct bus_node_cfg node_cfg;
    /** */
    int pin_cs;
    /** Data mode */
    int mode;
    /** Data order */
    int data_order;
    /** SCK frequency to be used for node */
    uint16_t freq;
    /** Quirks to be applied for device */
    uint16_t quirks;
};

struct bus_spi_node {
    struct bus_node bnode;
    int pin_cs;
    uint8_t mode;
    uint8_t data_order;
    uint16_t freq;
    uint16_t quirks;

#if MYNEWT_VAL(BUS_DEBUG_OS_DEV)
    uint32_t nodemagic;
#endif
};

static inline int
bus_spi_node_create(const char *name, struct bus_spi_node *node,
                    const struct bus_spi_node_cfg *cfg, void *arg)
{
    struct bus_node *bnode = (struct bus_node *)node;
    struct os_dev *odev = (struct os_dev *)node;

    bnode->init_arg = arg;

    return os_dev_create(odev, name, OS_DEV_INIT_PRIMARY, 1,
                         bus_node_init_func, (void *)cfg);
}

#ifdef __cplusplus
}
#endif

#endif /* HW_BUS_DRIVERS_SPI_COMMON_H_ */
