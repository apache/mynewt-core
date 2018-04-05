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
#include <at45db/at45db.h>

/**
 * Memory Architecture:
 *
 * Device can be addressed using pages, blocks or sectors.
 *
 * 1) Page
 *    - device has 8192 pages of 512 (or 528) bytes.
 *
 * 2) Block
 *    - device has 1024 blocks of 4K (or 4K + 128) bytes.
 *    - Each block contains 8 pages, eg, Block 0 == Page 0 to 7, etc.
 *
 * 3) Sector
 *    - Sector 0 == Block 0.
 *    - Sector 1 == Blocks 1 to 63 (252K + 8064).
 *    - Sector 2 to 16 contain 64 blocks (256K + 8192).
 */

#define MEM_READ                    0x52  /* Read page bypassing buffer */
#define BUF1_READ                   0x54
#define BUF2_READ                   0x56
#define MEM_TO_BUF1_TRANSFER        0x53
#define MEM_TO_BUF2_TRANSFER        0x55
#define MEM_TO_BUF1_CMP             0x60
#define MEM_TO_BUF2_CMP             0x61
#define BUF1_WRITE                  0x84
#define BUF2_WRITE                  0x87
#define BUF1_TO_MEM_ERASE           0x83
#define BUF2_TO_MEM_ERASE           0x86
#define BUF1_TO_MEM_NO_ERASE        0x88
#define BUF2_TO_MEM_NO_ERASE        0x89
#define PAGE_ERASE                  0x81
#define BLOCK_ERASE                 0x50

#define STATUS_REGISTER             0x57

#define STATUS_BUSY                 (1 << 7)
#define STATUS_CMP                  (1 << 6)

#define MAX_PAGE_SIZE               528

static const struct hal_flash_funcs at45db_flash_funcs = {
    .hff_read         = at45db_read,
    .hff_write        = at45db_write,
    .hff_erase_sector = at45db_erase_sector,
    .hff_sector_info  = at45db_sector_info,
    .hff_init         = at45db_init,
};

static struct at45db_dev at45db_default_dev = {
    /* struct hal_flash for compatibility */
    .hal = {
        .hf_itf        = &at45db_flash_funcs,
        .hf_base_addr  = 0,
        .hf_size       = 8192 * 512,  /* FIXME: depends on page size */
        .hf_sector_cnt = 8192,
        .hf_align      = 0,
    },

    /* SPI settings + updates baudrate on _init */
    .settings           = NULL,

    /* Configurable fields that must be populated by user app */
    .spi_num            = 0,
    .spi_cfg            = NULL,
    .ss_pin             = 0,
    .baudrate           = 100,
    .page_size          = 512,
    .disable_auto_erase = 0,
};

static struct hal_spi_settings at45db_default_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 100,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

static uint8_t g_page_buffer[MAX_PAGE_SIZE];

static uint8_t
at45db_read_status(struct at45db_dev *dev)
{
    uint8_t val;

    hal_gpio_write(dev->ss_pin, 0);

    hal_spi_tx_val(dev->spi_num, STATUS_REGISTER);
    val = hal_spi_tx_val(dev->spi_num, 0xff);

    hal_gpio_write(dev->ss_pin, 1);

    return val;
}

static inline bool
at45db_device_ready(struct at45db_dev *dev)
{
    return ((at45db_read_status(dev) & STATUS_BUSY) != 0);
}

static inline bool
at45db_buffer_equal(struct at45db_dev *dev)
{
    return ((at45db_read_status(dev) & STATUS_CMP) == 0);
}

/* FIXME: add timeout */
static inline void
at45db_wait_ready(struct at45db_dev *dev)
{
    while (!at45db_device_ready(dev)) {
        os_time_delay(OS_TICKS_PER_SEC / 10000);
    }
}

static inline uint16_t
at45db_calc_page_count(struct at45db_dev *dev, uint32_t addr, size_t len)
{
    uint16_t page_count;
    uint16_t page_size = dev->page_size;

    page_count = 1 + (len / (page_size + 1));
    if ((addr % page_size) + len > page_size * page_count) {
        page_count++;
    }

    return page_count;
}

static inline uint32_t
at45db_page_start_address(struct at45db_dev *dev, uint32_t addr)
{
    /* FIXME: works only for 512? (powers of 2) */
    return (addr & ~(dev->page_size - 1));
}

static inline uint32_t
at45db_page_next_addr(struct at45db_dev *dev, uint32_t addr)
{
    return (at45db_page_start_address(dev, addr) + dev->page_size);
}

/* FIXME: assume buf has enough space? */
static uint16_t
at45db_read_page(struct at45db_dev *dev, uint32_t addr,
                 uint16_t len, uint8_t *buf)
{
    uint16_t amount;
    uint16_t pa;
    uint16_t ba;
    uint16_t n;
    uint8_t val;
    uint16_t page_size;

    hal_gpio_write(dev->ss_pin, 0);

    hal_spi_tx_val(dev->spi_num, MEM_READ);

    page_size = dev->page_size;
    pa = addr / page_size;
    ba = addr % page_size;

    val = (pa >> 6) & ~0x80;
    hal_spi_tx_val(dev->spi_num, val);

    if (page_size <= 512) {
        val = (pa << 2) | ((ba >> 8) & 1);
    } else {
        val = (pa << 2) | ((ba >> 8) & 3);
    }
    hal_spi_tx_val(dev->spi_num, val);

    hal_spi_tx_val(dev->spi_num, ba);

    hal_spi_tx_val(dev->spi_num, 0xff);
    hal_spi_tx_val(dev->spi_num, 0xff);
    hal_spi_tx_val(dev->spi_num, 0xff);
    hal_spi_tx_val(dev->spi_num, 0xff);

    if (len + ba <= page_size) {
        amount = len;
    } else {
        amount = page_size - ba;
    }

    for (n = 0; n < amount; n++) {
        buf[n] = hal_spi_tx_val(dev->spi_num, 0xff);
    }

    hal_gpio_write(dev->ss_pin, 1);

    return amount;
}

int
at45db_read(const struct hal_flash *hal_flash_dev, uint32_t addr, void *buf,
        uint32_t len)
{
    uint16_t page_count;
    uint16_t amount;
    uint16_t index;
    uint8_t *u8buf;
    struct at45db_dev *dev;

    dev = (struct at45db_dev *) hal_flash_dev;

    page_count = at45db_calc_page_count(dev, addr, len);
    u8buf = (uint8_t *) buf;
    index = 0;

    while (page_count--) {
        at45db_wait_ready(dev);

        amount = at45db_read_page(dev, addr, len, &u8buf[index]);

        addr = at45db_page_next_addr(dev, addr);
        index += amount;
        len -= amount;
    }

    return 0;
}

int
at45db_write(const struct hal_flash *hal_flash_dev, uint32_t addr,
        const void *buf, uint32_t len)
{
    uint16_t pa;
    uint16_t bfa;
    uint32_t n;
    uint32_t start_addr;
    uint16_t index;
    uint16_t amount;
    int page_count;
    uint8_t *u8buf;
    uint16_t page_size;
    struct at45db_dev *dev;

    dev = (struct at45db_dev *) hal_flash_dev;

    page_size = dev->page_size;
    page_count = at45db_calc_page_count(dev, addr, len);
    u8buf = (uint8_t *) buf;
    index = 0;

    while (page_count--) {
        at45db_wait_ready(dev);

        bfa = addr % page_size;
        start_addr = at45db_page_start_address(dev, addr);

        /**
         * If the page is not being written from the beginning,
         * read first the current data to rewrite back.
         *
         * The whole page is read here for the case of some the
         * real data ending before the end of the page, data must
         * be written back again.
         */
        if (bfa || len < page_size) {
            at45db_read_page(dev, start_addr, page_size, g_page_buffer);
            at45db_wait_ready(dev);
        }

        hal_gpio_write(dev->ss_pin, 0);

        /* TODO: ping-pong between page 1 and 2? */
        hal_spi_tx_val(dev->spi_num, BUF1_WRITE);

        hal_spi_tx_val(dev->spi_num, 0xff);

        /* Always write at offset 0 of internal buffer */
        hal_spi_tx_val(dev->spi_num, 0);
        hal_spi_tx_val(dev->spi_num, 0);

        /**
         * Write back extra stuff at the beginning of page.
         */
        if (bfa) {
            amount = addr - start_addr;
            for (n = 0; n < amount; n++) {
                hal_spi_tx_val(dev->spi_num, g_page_buffer[n]);
            }
        }

        if (len + bfa <= page_size) {
            amount = len;
        } else {
            amount = page_size - bfa;
        }

        /**
         * Write the stuff we're really want to write!
         */
        for (n = 0; n < amount; n++) {
            hal_spi_tx_val(dev->spi_num, u8buf[index++]);
        }

        /**
         * Write back extra stuff at the ending of page.
         */
        if (bfa + len < page_size) {
            for (n = len + bfa; n < page_size; n++) {
                hal_spi_tx_val(dev->spi_num, g_page_buffer[n]);
            }
        }

        hal_gpio_write(dev->ss_pin, 1);

        at45db_wait_ready(dev);

        hal_gpio_write(dev->ss_pin, 0);

        if (dev->disable_auto_erase) {
            hal_spi_tx_val(dev->spi_num, BUF1_TO_MEM_NO_ERASE);
        } else {
            hal_spi_tx_val(dev->spi_num, BUF1_TO_MEM_ERASE);
        }

        /* FIXME: check that pa doesn't overflow capacity */
        pa = addr / page_size;

        hal_spi_tx_val(dev->spi_num, (pa >> 6) & ~0x80);
        hal_spi_tx_val(dev->spi_num, pa << 2);
        hal_spi_tx_val(dev->spi_num, 0xff);

        hal_gpio_write(dev->ss_pin, 1);

        addr = at45db_page_next_addr(dev, addr);
        len -= amount;
    }

    return 0;
}

int
at45db_erase_sector(const struct hal_flash *hal_flash_dev,
        uint32_t sector_address)
{
    struct at45db_dev *dev;
    uint16_t pa;

    dev = (struct at45db_dev *) hal_flash_dev;
    pa = sector_address / dev->page_size;

    at45db_wait_ready(dev);

    hal_gpio_write(dev->ss_pin, 0);

    hal_spi_tx_val(dev->spi_num, PAGE_ERASE);
    hal_spi_tx_val(dev->spi_num, (pa >> 6) & ~0x80);
    hal_spi_tx_val(dev->spi_num, pa << 2);
    hal_spi_tx_val(dev->spi_num, 0xff);

    hal_gpio_write(dev->ss_pin, 1);

    return 0;
}

int
at45db_sector_info(const struct hal_flash *hal_flash_dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    struct at45db_dev *dev = (struct at45db_dev *) hal_flash_dev;

    *address = idx * dev->page_size;
    *sz = dev->page_size;
    return 0;
}

struct at45db_dev *
at45db_default_config(void)
{
    struct at45db_dev *dev;

    dev = malloc(sizeof(at45db_default_dev));
    if (!dev) {
        return NULL;
    }

    memcpy(dev, &at45db_default_dev, sizeof(at45db_default_dev));
    return dev;
}

int
at45db_init(const struct hal_flash *hal_flash_dev)
{
    int rc;
    struct hal_spi_settings *settings;
    struct at45db_dev *dev;

    dev = (struct at45db_dev *) hal_flash_dev;

    /* only alloc new settings if using non-default */
    if (dev->baudrate == at45db_default_settings.baudrate) {
        dev->settings = &at45db_default_settings;
    } else {
        settings = malloc(sizeof(at45db_default_settings));
        if (!settings) {
            return -1;
        }
        memcpy(settings, &at45db_default_settings, sizeof(at45db_default_settings));
        at45db_default_settings.baudrate = dev->baudrate;
    }

    hal_gpio_init_out(dev->ss_pin, 1);

    rc = hal_spi_init(dev->spi_num, dev->spi_cfg, HAL_SPI_TYPE_MASTER);
    if (rc) {
        return (rc);
    }

    rc = hal_spi_config(dev->spi_num, dev->settings);
    if (rc) {
        return (rc);
    }

    hal_spi_set_txrx_cb(dev->spi_num, NULL, NULL);
    hal_spi_enable(dev->spi_num);

    return 0;
}
