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

#ifndef __AT45DB_H__
#define __AT45DB_H__

#include <hal/hal_flash_int.h>
#include <hal/hal_spi.h>

#ifdef __cplusplus
extern "C" {
#endif

struct at45db_dev {
    struct hal_flash hal;
    struct hal_spi_settings *settings;
    int spi_num;
    void *spi_cfg;                  /** Low-level MCU SPI config */
    int ss_pin;
    uint32_t baudrate;
    uint16_t page_size;             /** Page size to be used, valid: 512 and 528 */
    uint8_t disable_auto_erase;     /** Reads and writes auto-erase by default */
};

struct at45db_dev * at45db_default_config(void);
int at45db_read(const struct hal_flash *dev, uint32_t addr, void *buf,
                uint32_t len);
int at45db_write(const struct hal_flash *dev, uint32_t addr, const void *buf,
                 uint32_t len);
int at45db_erase_sector(const struct hal_flash *dev, uint32_t sector_address);
int at45db_sector_info(const struct hal_flash *dev, int idx, uint32_t *address,
                       uint32_t *sz);
int at45db_init(const struct hal_flash *dev);

#ifdef __cplusplus
}
#endif

#endif /* __AT45DB_H__ */
