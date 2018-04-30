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

#include "os/mynewt.h"
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <spiflash/spiflash.h>

#if MYNEWT_VAL(SPIFLASH)

#if MYNEWT_VAL(SPIFLASH_MANUFACTURER) == 0
#error SPIFLASH_MANUFACTURER must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_MEMORY_TYPE) == 0
#error SPIFLASH_MEMORY_TYPE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_MEMORY_CAPACITY) == 0
#error SPIFLASH_MEMORY_CAPACITY must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_SPI_CS_PIN) < 0
#error SPIFLASH_SPI_CS_PIN must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_SECTOR_COUNT) == 0
#error SPIFLASH_SECTOR_COUNT must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_SECTOR_SIZE) == 0
#error SPIFLASH_SECTOR_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_PAGE_SIZE) == 0
#error SPIFLASH_PAGE_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_BAUDRATE) == 0
#error SPIFLASH_BAUDRATE must be set to the correct value in bsp syscfg.yml
#endif

static int spiflash_read(const struct hal_flash *hal_flash_dev, uint32_t addr,
        void *buf, uint32_t len);
static int spiflash_write(const struct hal_flash *hal_flash_dev, uint32_t addr,
        const void *buf, uint32_t len);
static int spiflash_erase_sector(const struct hal_flash *hal_flash_dev,
        uint32_t sector_address);
static int spiflash_sector_info(const struct hal_flash *hal_flash_dev, int idx,
        uint32_t *address, uint32_t *sz);

static const struct hal_flash_funcs spiflash_flash_funcs = {
    .hff_read         = spiflash_read,
    .hff_write        = spiflash_write,
    .hff_erase_sector = spiflash_erase_sector,
    .hff_sector_info  = spiflash_sector_info,
    .hff_init         = spiflash_init,
};

struct spiflash_dev spiflash_dev = {
    /* struct hal_flash for compatibility */
    .hal = {
        .hf_itf        = &spiflash_flash_funcs,
        .hf_base_addr  = 0,
        .hf_size       = MYNEWT_VAL(SPIFLASH_SECTOR_COUNT) *
                         MYNEWT_VAL(SPIFLASH_SECTOR_SIZE),
        .hf_sector_cnt = MYNEWT_VAL(SPIFLASH_SECTOR_COUNT),
        .hf_align      = 0,
    },

    /* SPI settings */
    .spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .data_mode  = HAL_SPI_MODE3,
        .baudrate   = MYNEWT_VAL(SPIFLASH_BAUDRATE),
        .word_size  = HAL_SPI_WORD_SIZE_8BIT,
    },

    .sector_size        = MYNEWT_VAL(SPIFLASH_SECTOR_SIZE),
    .page_size          = MYNEWT_VAL(SPIFLASH_PAGE_SIZE),
    .spi_num            = MYNEWT_VAL(SPIFLASH_SPI_NUM),
    .spi_cfg            = NULL,
    .ss_pin             = MYNEWT_VAL(SPIFLASH_SPI_CS_PIN),
};

static inline void
spiflash_cs_activate(struct spiflash_dev *dev)
{
    hal_gpio_write(dev->ss_pin, 0);
}

static inline void
spiflash_cs_deactivate(struct spiflash_dev *dev)
{
    hal_gpio_write(dev->ss_pin, 1);
}

uint8_t
spiflash_release_power_down(struct spiflash_dev *dev, uint8_t *id)
{
    uint8_t cmd[5] = { SPIFLASH_RELEASE_POWER_DOWN, 0xFF, 0xFF, 0xFF, 0 };

    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, cmd, sizeof cmd);

    spiflash_cs_deactivate(dev);

    if (id) {
        *id = cmd[4];
    }

    return 0;
}

uint8_t
spiflash_read_jedec_id(struct spiflash_dev *dev,
        uint8_t *manufacturer, uint8_t *memory_type, uint8_t *capacity)
{
    uint8_t cmd[4] = { SPIFLASH_READ_JEDEC_ID, 0, 0, 0 };

    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, cmd, sizeof cmd);

    spiflash_cs_deactivate(dev);

    if (manufacturer) {
        *manufacturer = cmd[1];
    }

    if (memory_type) {
        *memory_type = cmd[2];
    }

    if (capacity) {
        *capacity = cmd[3];
    }

    return 0;
}

uint8_t
spiflash_read_status(struct spiflash_dev *dev)
{
    uint8_t val;

    spiflash_cs_activate(dev);

    hal_spi_tx_val(dev->spi_num, SPIFLASH_READ_STATUS_REGISTER);
    val = hal_spi_tx_val(dev->spi_num, 0xFF);

    spiflash_cs_deactivate(dev);

    return val;
}

bool
spiflash_device_ready(struct spiflash_dev *dev)
{
    return !(spiflash_read_status(dev) & SPIFLASH_STATUS_BUSY);
}

int
spiflash_wait_ready(struct spiflash_dev *dev, uint32_t timeout_ms)
{
    uint32_t ticks;
    os_time_t exp_time;
    os_time_ms_to_ticks(timeout_ms, &ticks);
    exp_time = os_time_get() + ticks;

    while (!spiflash_device_ready(dev)) {
        if (os_time_get() > exp_time) {
            return -1;
        }
    }
    return 0;
}

int
spiflash_write_enable(struct spiflash_dev *dev)
{
    spiflash_cs_activate(dev);

    hal_spi_tx_val(dev->spi_num, SPIFLASH_WRITE_ENABLE);

    spiflash_cs_deactivate(dev);

    return 0;
}

int
spiflash_read(const struct hal_flash *hal_flash_dev, uint32_t addr, void *buf,
        uint32_t len)
{
    int err = 0;
    uint8_t cmd[] = { SPIFLASH_READ,
        (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)(addr) };
    struct spiflash_dev *dev;

    dev = (struct spiflash_dev *)hal_flash_dev;

    err = spiflash_wait_ready(dev, 100);
    if (!err) {
        spiflash_cs_activate(dev);

        /* Send command + address */
        hal_spi_txrx(dev->spi_num, cmd, NULL, sizeof cmd);
        /* For security mostly, do not output random data, fill it with FF */
        memset(buf, 0xFF, len);
        /* Tx buf does not matter, for simplicity pass read buffer */
        hal_spi_txrx(dev->spi_num, buf, buf, len);

        spiflash_cs_deactivate(dev);
    }

    return 0;
}

int
spiflash_write(const struct hal_flash *hal_flash_dev, uint32_t addr,
        const void *buf, uint32_t len)
{
    uint8_t cmd[4] = { SPIFLASH_PAGE_PROGRAM };
    const uint8_t *u8buf = buf;
    struct spiflash_dev *dev = (struct spiflash_dev *)hal_flash_dev;
    uint32_t page_limit;
    uint32_t to_write;

    u8buf = (uint8_t *)buf;

    while (len) {
        if (spiflash_wait_ready(dev, 100) != 0) {
            return -1;
        }

        spiflash_write_enable(dev);

        cmd[1] = (uint8_t)(addr >> 16);
        cmd[2] = (uint8_t)(addr >> 8);
        cmd[3] = (uint8_t)(addr);

        page_limit = (addr & ~(dev->page_size - 1)) + dev->page_size;
        to_write = page_limit - addr > len ? len :  page_limit - addr;

        spiflash_cs_activate(dev);
        hal_spi_txrx(dev->spi_num, cmd, NULL, sizeof cmd);
        hal_spi_txrx(dev->spi_num, (void *)buf, NULL, to_write);
        spiflash_cs_deactivate(dev);

        addr += to_write;
        u8buf += to_write;
        len -= to_write;

        spiflash_wait_ready(dev, 100);
    }

    return 0;
}

int
spiflash_erase_sector(const struct hal_flash *hal_flash_dev,
        uint32_t addr)
{
    struct spiflash_dev *dev;
    uint8_t cmd[4] = { SPIFLASH_SECTOR_ERASE,
        (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };

    dev = (struct spiflash_dev *)hal_flash_dev;

    if (spiflash_wait_ready(dev, 100) != 0) {
        return -1;
    }

    spiflash_write_enable(dev);

    spiflash_read_status(dev);

    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, NULL, sizeof cmd);

    spiflash_cs_deactivate(dev);

    spiflash_wait_ready(dev, 100);

    return 0;
}

int
spiflash_sector_info(const struct hal_flash *hal_flash_dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    const struct spiflash_dev *dev = (const struct spiflash_dev *)hal_flash_dev;

    *address = idx * dev->sector_size;
    *sz = dev->sector_size;
    return 0;
}

int
spiflash_init(const struct hal_flash *hal_flash_dev)
{
    int rc;
    struct spiflash_dev *dev;
    uint8_t manufacturer;
    uint8_t memory_type;
    uint8_t capacity;

    dev = (struct spiflash_dev *)hal_flash_dev;

    hal_gpio_init_out(dev->ss_pin, 1);

    rc = hal_spi_config(dev->spi_num, &dev->spi_settings);
    if (rc) {
        return (rc);
    }

    hal_spi_set_txrx_cb(dev->spi_num, NULL, NULL);
    hal_spi_enable(dev->spi_num);

    spiflash_release_power_down(dev, &manufacturer);
    spiflash_read_jedec_id(dev, &manufacturer, &memory_type, &capacity);
    /* If BSP defined SpiFlash manufacturer or memory type does not
     * match SpiFlash is most likely not connected, connected to
     * different pins, or of different type.
     * It is unlikely that flash depended packaged will work correctly.
     */
    assert(manufacturer == MYNEWT_VAL(SPIFLASH_MANUFACTURER) ||
            memory_type == MYNEWT_VAL(SPIFLASH_MEMORY_TYPE) ||
            capacity == MYNEWT_VAL(SPIFLASH_MEMORY_CAPACITY));
    if (manufacturer != MYNEWT_VAL(SPIFLASH_MANUFACTURER) ||
        memory_type != MYNEWT_VAL(SPIFLASH_MEMORY_TYPE) ||
        capacity != MYNEWT_VAL(SPIFLASH_MEMORY_CAPACITY)) {
        return -1;
    }

    return 0;
}

#endif

