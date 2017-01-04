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

#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>

#include <flash/flash.h>

#include <stdio.h>

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


#define PAGE_ERASE                  0x81
#define BLOCK_ERASE                 0x50

#define STATUS_REGISTER             0x57

#define STATUS_BUSY                 (1 << 7)
#define STATUS_CMP                  (1 << 6)

/* TODO: make configurable to 512/528 */
#define PAGE_SIZE                   512

/**
 * Reading memory (MEM_READ):
 * < r, PA12-6 >
 * < PA5-0, BA9-8 >
 * < BA7-0 >
 * < 8 don't care bits >
 * < 8 don't care bits >
 * < 8 don't care bits >
 * < 8 don't care bits >
 *
 * Reading a buffer (BUFx_READ):
 * < 8 don't care bits >
 * < 6 don't care bits, A9-8 >
 * < A7-0 >
 * < 8 don't care bits >
 *
 * Memory to buffer copy (MEM_TO_BUFx_TRANSFER):
 * < r, PA12-PA6 >
 * < PA5-0, 2 don't care bits >
 * < 8 don't care bits >
 *
 * Memory to buffer compare (MEM_TO_BUFx_CMP):
 * < r, PA12-PA6 >
 * < PA5-0, 2 don't care bits >
 * < 8 don't care bits >
 *
 * Buffer write (BUFx_WRITE):
 * < 8 don't care bits >
 * < 6 don't care bits, BFA9-8 >
 * < BFA7-0 >
 *
 * Buffer to memory program with erase (BUFx_TO_MEM_ERASE):
 * < r, PA12-PA6 >
 * < PA5-0, 2 don't care bits >
 * < 8 don't care bits >
 *
 * Buffer to memory program without erase (BUFx_TO_MEM_NO_ERASE):
 * < r, PA12-PA6 >
 * < PA5-0, 2 don't care bits >
 * < 8 don't care bits >
 *
 * Page erase (PAGE_ERASE):
 * < r, PA12-PA6 >
 * < PA5-0, 2 don't care bits >
 * < 8 don't care bits >
 *
 * Block erase (BLOCK_ERASE):
 * < r, PA12-PA6 >
 * < PA5-PA3, 5 don't care bits >
 * < 8 don't care bits >
 */

static struct flash_dev {
    int                      spi_num;
    int                      ss_pin;
    void                     *spi_cfg;
    struct hal_spi_settings  *settings;
} g_flash_dev;

static struct hal_spi_settings at45db_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 100,  /* NOTE: default clock to be overwritten by init */
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

static uint8_t g_page_buffer[PAGE_SIZE];

static struct flash_dev *
cfg_dev(uint8_t id)
{
    if (id != 0) {
        return NULL;
    }

    return &g_flash_dev;
}

static uint8_t read_status(struct flash_dev *flash)
{
    uint8_t val;

    hal_gpio_write(flash->ss_pin, 0);

    hal_spi_tx_val(flash->spi_num, STATUS_REGISTER);
    val = hal_spi_tx_val(flash->spi_num, 0xff);

    hal_gpio_write(flash->ss_pin, 1);

    return val;
}

static inline bool
device_ready(struct flash_dev *dev)
{
    return ((read_status(dev) & STATUS_BUSY) != 0);
}

static inline bool
buffer_equal(struct flash_dev *dev)
{
    return ((read_status(dev) & STATUS_CMP) == 0);
}

/* FIXME: add timeout */
static inline void
wait_ready(struct flash_dev *dev)
{
    while (!device_ready(dev)) {
        os_time_delay(OS_TICKS_PER_SEC / 10000);
    }
}

static inline uint16_t
calc_page_count(uint32_t addr, size_t len)
{
    uint16_t page_count;

    page_count = 1 + (len / (PAGE_SIZE + 1));
    if ((addr % PAGE_SIZE) + len > PAGE_SIZE * page_count) {
        page_count++;
    }

    return page_count;
}

static inline uint32_t
page_start_address(uint32_t addr)
{
    /* FIXME: works only for 512? (powers of 2) */
    return (addr & ~(PAGE_SIZE - 1));
}

static inline uint32_t
page_next_addr(uint32_t addr)
{
    return (page_start_address(addr) + PAGE_SIZE);
}

/* FIXME: assume buf has enough space? */
static uint16_t
read_page(struct flash_dev *dev, uint32_t addr, uint16_t len, uint8_t *buf)
{
    uint16_t amount;
    uint16_t pa;
    uint16_t ba;
    uint16_t n;
    uint8_t val;

    hal_gpio_write(dev->ss_pin, 0);

    hal_spi_tx_val(dev->spi_num, MEM_READ);

    pa = addr / PAGE_SIZE;
    ba = addr % PAGE_SIZE;

    val = (pa >> 6) & ~0x80;
    hal_spi_tx_val(dev->spi_num, val);

#if (PAGE_SIZE == 512)
    val = (pa << 2) | ((ba >> 8) & 1);
#else
    val = (pa << 2) | ((ba >> 8) & 3);
#endif
    hal_spi_tx_val(dev->spi_num, val);

    hal_spi_tx_val(dev->spi_num, ba);

    hal_spi_tx_val(dev->spi_num, 0xff);
    hal_spi_tx_val(dev->spi_num, 0xff);
    hal_spi_tx_val(dev->spi_num, 0xff);
    hal_spi_tx_val(dev->spi_num, 0xff);

    if (len + ba <= PAGE_SIZE) {
        amount = len;
    } else {
        amount = PAGE_SIZE - ba;
    }

    for (n = 0; n < amount; n++) {
        buf[n] = hal_spi_tx_val(dev->spi_num, 0xff);
    }

    hal_gpio_write(dev->ss_pin, 1);

    return amount;
}

int
flash_init(int spi_num, void *spi_cfg, int ss_pin, uint32_t baudrate)
{
    int rc;
    struct flash_dev *dev;

    at45db_settings.baudrate = baudrate;

    dev = &g_flash_dev;
    dev->spi_num = spi_num;
    dev->ss_pin = ss_pin;
    dev->spi_cfg = spi_cfg;
    dev->settings = &at45db_settings;

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

int
flash_read(uint8_t flash_id, uint32_t addr, void *buf, size_t len)
{
    struct flash_dev *dev;
    uint16_t page_count;
    uint16_t amount;
    uint16_t index;
    uint8_t *u8buf;

    dev = cfg_dev(flash_id);
    if (!dev) {
        return -1;
    }

    page_count = calc_page_count(addr, len);
    u8buf = (uint8_t *) buf;
    index = 0;

    while (page_count--) {
        wait_ready(dev);

        amount = read_page(dev, addr, len, &u8buf[index]);

        addr = page_next_addr(addr);
        index += amount;
        len -= amount;
    }

    return 0;
}

int
flash_write(uint8_t flash_id, uint32_t addr, const void *buf, size_t len)
{
    struct flash_dev *dev;
    uint16_t pa;
    uint16_t bfa;
    uint32_t n;
    uint32_t start_addr;
    uint16_t index;
    uint16_t amount;
    int page_count;
    uint8_t *u8buf;

    dev = cfg_dev(flash_id);
    if (!dev) {
        return -1;
    }

    page_count = calc_page_count(addr, len);
    u8buf = (uint8_t *) buf;
    index = 0;

    while (page_count--) {
        wait_ready(dev);

        bfa = addr % PAGE_SIZE;

        /**
         * If the page is not being written from the beginning,
         * read first the current data to rewrite back.
         *
         * The whole page is read here for the case of some the
         * real data ending before the end of the page, data must
         * be written back again.
         */
        if (bfa || len < PAGE_SIZE) {
            read_page(dev, page_start_address(addr), PAGE_SIZE, g_page_buffer);
            wait_ready(dev);
        }

        hal_gpio_write(dev->ss_pin, 0);

        /* TODO: ping-pong between page 1 and 2? */
        hal_spi_tx_val(dev->spi_num, BUF1_WRITE);

        hal_spi_tx_val(dev->spi_num, 0xff);

        start_addr = page_start_address(addr);

        /* FIXME */
#if (PAGE_SIZE == 512)
        hal_spi_tx_val(dev->spi_num, (start_addr >> 8) & 1);
#else
        hal_spi_tx_val(dev->spi_num, (start_addr >> 8) & 3);
#endif

        hal_spi_tx_val(dev->spi_num, start_addr);

        /**
         * Write back extra stuff at the beginning of page.
         */
        if (bfa) {
            amount = addr - start_addr;
            for (n = 0; n < amount; n++) {
                hal_spi_tx_val(dev->spi_num, g_page_buffer[n]);
            }
        }

        if (len + bfa <= PAGE_SIZE) {
            amount = len;
        } else {
            amount = PAGE_SIZE - bfa;
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
        if (len < PAGE_SIZE) {
            for (n = len; n < PAGE_SIZE; n++) {
                hal_spi_tx_val(dev->spi_num, g_page_buffer[n]);
            }
        }

        hal_gpio_write(dev->ss_pin, 1);

        wait_ready(dev);

        hal_gpio_write(dev->ss_pin, 0);

        /* TODO: make command configurable (accept non erasing) */
        hal_spi_tx_val(dev->spi_num, BUF1_TO_MEM_ERASE);

        /* FIXME: check that pa doesn't overflow capacity */
        pa = addr / PAGE_SIZE;

        hal_spi_tx_val(dev->spi_num, (pa >> 6) & ~0x80);
        hal_spi_tx_val(dev->spi_num, (pa << 2) | 0x3);
        hal_spi_tx_val(dev->spi_num, 0xff);

        hal_gpio_write(dev->ss_pin, 1);

        addr = page_next_addr(addr);
        len -= amount;
    }

    return 0;
}

int
flash_erase(uint8_t flash_id, uint32_t addr, size_t len)
{
    struct flash_dev *dev;

    dev = cfg_dev(flash_id);
    if (!dev) {
        return -1;
    }

    return 0;
}
