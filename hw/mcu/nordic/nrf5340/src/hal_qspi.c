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
#include <assert.h>
#include <stdint.h>
#if MYNEWT_VAL(QSPI_ENABLE)
#include <hal/hal_flash_int.h>
#include "mcu/nrf5340_hal.h"
#include "nrf.h"
#include <nrfx/hal/nrf_qspi.h>

#if MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) < 1
#error QSPI_FLASH_SECTOR_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE) < 1
#error QSPI_FLASH_PAGE_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) < 1
#error QSPI_FLASH_SECTOR_COUNT must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_PIN_CS) < 0
#error QSPI_PIN_CS must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_PIN_SCK) < 0
#error QSPI_PIN_SCK must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_PIN_DIO0) < 0
#error QSPI_PIN_DIO0 must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_PIN_DIO1) < 0
#error QSPI_PIN_DIO1 must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_PIN_DIO2) < 0 && (MYNEWT_VAL(QSPI_READOC) > 2 || MYNEWT_VAL(QSPI_WRITEOC) > 1)
#error QSPI_PIN_DIO2 must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_PIN_DIO3) < 0 && (MYNEWT_VAL(QSPI_READOC) > 2 || MYNEWT_VAL(QSPI_WRITEOC) > 1)
#error QSPI_PIN_DIO3 must be set to the correct value in bsp syscfg.yml
#endif

#ifndef USE_MEMCPY_FOR_FLASH_READ
/*
 * QSPI peripheral can be access for read just like internal flash in XIP.
 * It is possible to read this flash with just memcpy, however mynewt implementation
 * of memcpy() copies data from last byte to the first. For normal flash and RAM
 * it may not be a problem but QSPI controller will not be able to do it optimally.
 * Enable this only for testing or if flash size is image is critical.
 */
#define USE_MEMCPY_FOR_FLASH_READ 0
#endif

static int
nrf5340_qspi_read(const struct hal_flash *dev, uint32_t address,
                  void *dst, uint32_t num_bytes);
static int
nrf5340_qspi_write(const struct hal_flash *dev, uint32_t address,
                   const void *src, uint32_t num_bytes);
static int
nrf5340_qspi_erase_sector(const struct hal_flash *dev,
                          uint32_t sector_address);
static int
nrf5340_qspi_sector_info(const struct hal_flash *dev, int idx,
                         uint32_t *address, uint32_t *sz);
static int
nrf5340_qspi_init(const struct hal_flash *dev);
static int
nrf5340_qspi_erase(const struct hal_flash *dev, uint32_t address,
                   uint32_t size);

static const struct hal_flash_funcs nrf5340_qspi_funcs = {
    .hff_read = nrf5340_qspi_read,
    .hff_write = nrf5340_qspi_write,
    .hff_erase_sector = nrf5340_qspi_erase_sector,
    .hff_sector_info = nrf5340_qspi_sector_info,
    .hff_init = nrf5340_qspi_init,
    .hff_erase = nrf5340_qspi_erase
};

const struct hal_flash nrf5340_qspi_dev = {
    .hf_itf = &nrf5340_qspi_funcs,
    .hf_base_addr = MYNEWT_VAL(QSPI_XIP_OFFSET),
    .hf_size = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE),
    .hf_sector_cnt = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT),
    .hf_align = 1,
    .hf_erased_val = 0xff,
};

static int
nrf5340_qspi_read(const struct hal_flash *dev, uint32_t address,
                  void *dst, uint32_t num_bytes)
{
#if USE_MEMCPY_FOR_FLASH_READ
    (void)dev;
    memcpy(dst, (const void *)address, num_bytes);
#else
    uint32_t ram_buffer[4];
    uint8_t *ram_ptr = NULL;
    uint32_t read_bytes;
    address -= dev->hf_base_addr;

    while (num_bytes != 0) {
        /*
         * If address or destination pointer are unaligned or
         * number of bytes to read is less then 4 read using ram buffer.
         */
        if (((address & 3) != 0) ||
            ((((uint32_t)dst) & 3) != 0) ||
            num_bytes < 4) {
            uint32_t to_read = (num_bytes + (address & 3) + 3) & ~3;
            if (to_read > sizeof(ram_buffer)) {
                to_read = sizeof(ram_buffer);
            }
            ram_ptr = (uint8_t *)((uint32_t)ram_buffer + (address & 3));
            read_bytes = to_read - (address & 3);
            if (read_bytes > num_bytes) {
                read_bytes = num_bytes;
            }
            NRF_QSPI->READ.DST = (uint32_t)ram_buffer;
            NRF_QSPI->READ.SRC = address & ~3;
            NRF_QSPI->READ.CNT = to_read;
        } else {
            read_bytes = num_bytes & ~3;
            NRF_QSPI->READ.DST = (uint32_t)dst;
            NRF_QSPI->READ.SRC = address;
            NRF_QSPI->READ.CNT = read_bytes;
        }
        NRF_QSPI->EVENTS_READY = 0;
        NRF_QSPI->TASKS_READSTART = 1;
        while (NRF_QSPI->EVENTS_READY == 0)
            ;
        if (ram_ptr != NULL) {
            memcpy(dst, ram_ptr, read_bytes);
            ram_ptr = NULL;
        }
        address += read_bytes;
        dst = (void *)((uint32_t)dst + read_bytes);
        num_bytes -= read_bytes;
    }
#endif

    return 0;
}

static int
nrf5340_qspi_write(const struct hal_flash *dev, uint32_t address,
                   const void *src, uint32_t num_bytes)
{
    uint32_t ram_buffer[4];
    uint8_t *ram_ptr = NULL;
    uint32_t written_bytes;
    const char src_not_in_ram = (((uint32_t)src) & 0xE0000000) != 0x20000000;
    uint32_t page_limit;
    uint32_t to_write;

    (void)dev;

    address -= MYNEWT_VAL(QSPI_XIP_OFFSET);

    while ((NRF_QSPI->STATUS & QSPI_STATUS_READY_Msk) == 0)
        ;

    while (num_bytes != 0) {
        page_limit = (address & ~(MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE) - 1)) +
                     MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE);
        /*
         * Use RAM buffer if src or address is not 4 bytes aligned,
         * or number of bytes to write is less then 4
         * or src pointer is not from RAM.
         */
        if (((address & 3) != 0) ||
            ((((uint32_t)src) & 3) != 0) ||
            num_bytes < 4 ||
            src_not_in_ram) {
            if (address + num_bytes > page_limit) {
                to_write = ((page_limit - address) + 3) & ~3;
            } else {
                to_write = (num_bytes + (address & 3) + 3) & ~3;
            }
            if (to_write > sizeof(ram_buffer)) {
                to_write = sizeof(ram_buffer);
            }
            memset(ram_buffer, 0xFF, sizeof(ram_buffer));
            ram_ptr = (uint8_t *)((uint32_t)ram_buffer + (address & 3));

            written_bytes = to_write - (address & 3);
            if (written_bytes > num_bytes) {
                written_bytes = num_bytes;
            }
            memcpy(ram_ptr, src, written_bytes);

            NRF_QSPI->WRITE.SRC = (uint32_t)ram_buffer;
            NRF_QSPI->WRITE.DST = address & ~3;
            NRF_QSPI->WRITE.CNT = to_write;
        } else {
            NRF_QSPI->WRITE.SRC = (uint32_t)src;
            NRF_QSPI->WRITE.DST = address;
            /*
             * Limit write to single page.
             */
            if (address + num_bytes > page_limit) {
                written_bytes = page_limit - address;
            } else {
                written_bytes = num_bytes & ~3;
            }
            NRF_QSPI->WRITE.CNT = written_bytes;
        }
        NRF_QSPI->EVENTS_READY = 0;
        NRF_QSPI->TASKS_WRITESTART = 1;
        while (NRF_QSPI->EVENTS_READY == 0)
            ;

        address += written_bytes;
        src = (void *)((uint32_t)src + written_bytes);
        num_bytes -= written_bytes;
    }
    return 0;
}

static void
erase_block(uint32_t starting_address,
            nrf_qspi_erase_len_t block_size_type)
{
    starting_address -= MYNEWT_VAL(QSPI_XIP_OFFSET);

    while ((NRF_QSPI->STATUS & QSPI_STATUS_READY_Msk) == 0)
        ;

    NRF_QSPI->EVENTS_READY = 0;
    NRF_QSPI->ERASE.PTR = starting_address;
    NRF_QSPI->ERASE.LEN = block_size_type;
    NRF_QSPI->TASKS_ERASESTART = 1;
    while (NRF_QSPI->EVENTS_READY == 0)
        ;
}

static int
nrf5340_qspi_erase_sector(const struct hal_flash *dev,
                          uint32_t sector_address)
{
    int8_t erases;

    (void)dev;

    erases = MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) / 4096;
    while (erases-- > 0) {
        erase_block(sector_address, NRF_QSPI_ERASE_LEN_4KB);
        sector_address += 4096;
    }

    return 0;
}

static int
nrf5340_qspi_erase(const struct hal_flash *dev, uint32_t address,
                   uint32_t size)
{
    uint32_t end;

    (void)dev;

    address &= ~0xFFFU;
    end = address + size;

    if (end == MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) *
        MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE)) {
        erase_block(0, NRF_QSPI_ERASE_LEN_ALL);
        return 0;
    }

    while (size) {
        if ((address & 0xFFFFU) == 0 && (size >= 0x10000)) {
            /* 64 KB erase if possible */
            erase_block(address, NRF_QSPI_ERASE_LEN_64KB);
            address += 0x10000;
            size -= 0x10000;
            continue;
        }

        erase_block(address, NRF_QSPI_ERASE_LEN_4KB);
        address += 0x1000;
        if (size > 0x1000) {
            size -= 0x1000;
        } else {
            size = 0;
        }
    }

    return 0;
}

static int
nrf5340_qspi_sector_info(const struct hal_flash *dev, int idx,
                         uint32_t *address, uint32_t *sz)
{
    (void)dev;

    *address = idx * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    *sz = MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);

    return 0;
}

static void
nrf5340_set_pin_strength(int pin)
{
    NRF_GPIO_Type *port;
    port = HAL_GPIO_PORT(pin);

    port->PIN_CNF[pin & 31] = (port->PIN_CNF[pin & 31] & ~GPIO_PIN_CNF_DRIVE_Msk) |
                              (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
}

static int
nrf5340_qspi_init(const struct hal_flash *dev)
{
    const nrf_qspi_prot_conf_t config0 = {
        .readoc = MYNEWT_VAL(QSPI_READOC),
        .writeoc = MYNEWT_VAL(QSPI_WRITEOC),
        .addrmode = MYNEWT_VAL(QSPI_ADDRMODE),
        .dpmconfig = MYNEWT_VAL(QSPI_DPMCONFIG)
    };
    const nrf_qspi_phy_conf_t config1 = {
        .sck_delay = MYNEWT_VAL(QSPI_SCK_DELAY),
        .dpmen = 0,
        .spi_mode = MYNEWT_VAL(QSPI_SPI_MODE),
        .sck_freq = MYNEWT_VAL(QSPI_SCK_FREQ),
    };

    (void)dev;

    /*
     * Configure pins.
     */
    NRF_QSPI->PSEL.CSN = MYNEWT_VAL(QSPI_PIN_CS);
    NRF_QSPI->PSEL.SCK = MYNEWT_VAL(QSPI_PIN_SCK);
    NRF_QSPI->PSEL.IO0 = MYNEWT_VAL(QSPI_PIN_DIO0);
    NRF_QSPI->PSEL.IO1 = MYNEWT_VAL(QSPI_PIN_DIO1);

    /*
     * QSPI pins need high drive. See 9.1 section of product specification.
     */
    nrf5340_set_pin_strength(MYNEWT_VAL(QSPI_PIN_SCK));
    nrf5340_set_pin_strength(MYNEWT_VAL(QSPI_PIN_DIO0));
    nrf5340_set_pin_strength(MYNEWT_VAL(QSPI_PIN_DIO1));

    /*
     * Setup only known fields of IFCONFIG0. Other bits may be set by erratas code.
     */
    nrf_qspi_ifconfig0_set(NRF_QSPI, &config0);
    nrf_qspi_ifconfig1_set(NRF_QSPI, &config1);

    NRF_QSPI->XIPOFFSET = MYNEWT_VAL(QSPI_XIP_OFFSET);

#if !defined(NRF_TRUSTZONE_NONSECURE)
    /* Workaround for Errata 121: QSPI: Configuration of peripheral requires additional steps */
    if (nrf53_errata_121()) {
        NRF_QSPI->IFTIMING = (NRF_QSPI->IFTIMING & ~(7 << 8)) | (6 << 8);
        if (192000000u / MYNEWT_VAL(MCU_HFCLCK192_DIV) / (2u * (MYNEWT_VAL(QSPI_SCK_FREQ) + 1)) == 96000000u) {
            NRF_QSPI->IFCONFIG0 |= (3 << 16);
        } else if (192000000u / MYNEWT_VAL(MCU_HFCLCK192_DIV) / (2u * (MYNEWT_VAL(QSPI_SCK_FREQ) + 1)) >= 6000000u) {
            NRF_QSPI->IFCONFIG0 &= ~(1 << 17);
            NRF_QSPI->IFCONFIG0 |= (1 << 16);
        }
    }
#endif

#if (MYNEWT_VAL(QSPI_READOC) > 2) || (MYNEWT_VAL(QSPI_WRITEOC) > 1)
    NRF_QSPI->PSEL.IO2 = MYNEWT_VAL(QSPI_PIN_DIO2);
    NRF_QSPI->PSEL.IO3 = MYNEWT_VAL(QSPI_PIN_DIO3);
    nrf5340_set_pin_strength(MYNEWT_VAL(QSPI_PIN_DIO2));
    nrf5340_set_pin_strength(MYNEWT_VAL(QSPI_PIN_DIO3));
#endif

    NRF_QSPI->ENABLE = 1;
    NRF_QSPI->TASKS_ACTIVATE = 1;
    while (NRF_QSPI->EVENTS_READY == 0)
        ;

    return 0;
}

#endif
