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

#ifndef __SPIRAM_H__
#define __SPIRAM_H__

#include <os/os_mutex.h>
#include <hal/hal_spi.h>
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/spi_common.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct spiram_characteristics {
    /**< Address size in bytes */
    uint8_t address_bytes;
    /**< Dummy bytes to send after address */
    uint8_t dummy_bytes;
    /**< Write enable command (0 not needed) */
    uint8_t write_enable_cmd;
    /**< Hibernate command (0 not needed) */
    uint8_t hibernate_cmd;
    /**< RAM size in bytes */
    uint32_t size;
};

struct spiram_dev {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node dev;
#else
    struct os_dev dev;
    struct hal_spi_settings spi_settings;
    int spi_num;
    int ss_pin;
#endif
    const struct spiram_characteristics *characteristics;
#if MYNEWT_VAL(OS_SCHEDULING)
    struct os_mutex lock;
#endif
};

struct spiram_cfg {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node_cfg node_cfg;
#else
    struct hal_spi_settings spi_settings;
    uint8_t spi_num;
    int16_t ss_pin;
#endif
    const struct spiram_characteristics *characteristics;
};

#define SPIRAM_WRITE                        0x02
#define SPIRAM_READ                         0x03

int spiram_read(struct spiram_dev *dev, uint32_t addr, void *buf, uint32_t size);
int spiram_write(struct spiram_dev *dev, uint32_t addr, void *buf, uint32_t size);

int spiram_create_spi_dev(struct spiram_dev *dev, const char *name,
                          const struct spiram_cfg *spi_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __SPIRAM_H__ */
