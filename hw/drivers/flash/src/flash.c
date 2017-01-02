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

#define MIN(n, m) (((n) < (m)) ? (n) : (m))

#define PAGE_SIZE                   512
#define PAGE_BITS                   9

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

    //printf("STATUS=[%02x]\n", val);

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
    uint16_t pa;
    uint16_t ba;
    uint8_t val;
    uint32_t n;
    uint16_t page_count;
    uint16_t page_index;
    uint16_t offset;
    uint16_t amount;

    dev = cfg_dev(flash_id);
    if (!dev) {
        return -1;
    }

    page_count = (len / (PAGE_SIZE + 1)) + 1;
    offset = addr % PAGE_SIZE;
    if (len - offset >= PAGE_SIZE) {
        page_count++;
    }

    while (page_count--) {
        wait_ready(dev);

        hal_gpio_write(dev->ss_pin, 0);

        hal_spi_tx_val(dev->spi_num, MEM_READ);

        pa = addr / PAGE_SIZE;
        ba = addr % PAGE_SIZE;

        /* < r, PA12-6 > */
        val = (pa >> 6) & ~0x80;
        hal_spi_tx_val(dev->spi_num, val);

        /* FIXME: not using BA9 */

        /* < PA5-0, BA9-8 > */
        val = (pa << 2) | ((ba >> 8) & 1);
        hal_spi_tx_val(dev->spi_num, val);

        /* < BA7-0 > */
        hal_spi_tx_val(dev->spi_num, ba);

        /* send 32 don't care bits */
        hal_spi_tx_val(dev->spi_num, 0xff);
        hal_spi_tx_val(dev->spi_num, 0xff);
        hal_spi_tx_val(dev->spi_num, 0xff);
        hal_spi_tx_val(dev->spi_num, 0xff);

        amount = MIN
        for (n = 0; n < amount; n++) {
            *((uint8_t *) buf + n) = hal_spi_tx_val(dev->spi_num, 0xff);
        }

        hal_gpio_write(dev->ss_pin, 1);

        page_index += PAGE_SIZE;
        addr = (addr + offset) & (PAGE_SIZE - 1);
    }

    return 0;
}

int
flash_write(uint8_t flash_id, uint32_t addr, const void *buf, size_t len)
{
    struct flash_dev *dev;
    uint16_t pa;
    //uint16_t bfa;
    uint32_t n;
    int page_count;
    int offset;

    dev = cfg_dev(flash_id);
    if (!dev) {
        return -1;
    }

    /* FIXME: calculation is assuming that addr starts at offset 0 in page */
    addr % PAGE_SIZE
    page_count = (len / (PAGE_SIZE + 1)) + 1;

    if (!page_count) {
        return 0;
    }

    offset = 0;
    while (page_count-- >= 0) {
        wait_ready(dev);

        hal_gpio_write(dev->ss_pin, 0);

        /* FIXME: ping-pong between page 1 and 2 */
        hal_spi_tx_val(dev->spi_num, BUF1_WRITE);

        /* FIXME: always writing at 0 on buffer */
        hal_spi_tx_val(dev->spi_num, 0xff);
        hal_spi_tx_val(dev->spi_num, 0xfc);
        hal_spi_tx_val(dev->spi_num, 0);

        for (n = 0; n < len; n++) {
             hal_spi_tx_val(dev->spi_num, *((uint8_t *) buf + n));
        }

        hal_gpio_write(dev->ss_pin, 1);

        wait_ready(dev);

        hal_gpio_write(dev->ss_pin, 0);

        hal_spi_tx_val(dev->spi_num, BUF1_TO_MEM_ERASE);

        /* FIXME: check that pa doesn't overflow capacity */
        pa = addr / PAGE_SIZE;

        hal_spi_tx_val(dev->spi_num, (pa >> 6) & ~0x80);
        hal_spi_tx_val(dev->spi_num, (pa << 2) | 0x3);
        hal_spi_tx_val(dev->spi_num, 0xff);

        hal_gpio_write(dev->ss_pin, 1);

        offset += PAGE_SIZE;
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
