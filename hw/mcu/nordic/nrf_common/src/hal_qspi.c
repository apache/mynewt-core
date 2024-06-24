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
#include "os/mynewt.h"
#if MYNEWT_VAL(QSPI_ENABLE)
#include <mcu/cmsis_nvic.h>
#include <hal/hal_flash_int.h>
#include "nrf.h"
#include <nrfx_qspi.h>

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

static int nrf_qspi_read(const struct hal_flash *dev, uint32_t address,
                         void *dst, uint32_t num_bytes);
static int nrf_qspi_write(const struct hal_flash *dev, uint32_t address,
                          const void *src, uint32_t num_bytes);
static int nrf_qspi_erase_sector(const struct hal_flash *dev,
                                 uint32_t sector_address);
static int nrf_qspi_sector_info(const struct hal_flash *dev, int idx,
                                uint32_t *address, uint32_t *sz);
static int nrf_qspi_init(const struct hal_flash *dev);
static int nrf_qspi_erase(const struct hal_flash *dev, uint32_t address,
                          uint32_t size);

static const struct hal_flash_funcs nrf_qspi_funcs = {
    .hff_read = nrf_qspi_read,
    .hff_write = nrf_qspi_write,
    .hff_erase_sector = nrf_qspi_erase_sector,
    .hff_sector_info = nrf_qspi_sector_info,
    .hff_init = nrf_qspi_init,
    .hff_erase = nrf_qspi_erase
};

const struct hal_flash nrf_qspi_dev = {
    .hf_itf = &nrf_qspi_funcs,
    .hf_base_addr = 0x00000000,
    .hf_size = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE),
    .hf_sector_cnt = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT),
    .hf_align = 1,
    .hf_erased_val = 0xff,
};

static int
nrf_qspi_read(const struct hal_flash *dev, uint32_t address,
              void *dst, uint32_t num_bytes)
{
    uint32_t ram_buffer[4];
    uint8_t *ram_ptr = NULL;
    uint32_t read_bytes;

    while (num_bytes != 0) {
        /*
         * If address or destination pointer are unaligned or
         * number of bytes to read is less then 4 read using ram buffer.
         */
        if (((address & 3) != 0) ||
            ((((uint32_t) dst) & 3) != 0) ||
            num_bytes < 4) {
            uint32_t to_read = (num_bytes + (address & 3) + 3) & ~3;
            if (to_read > sizeof(ram_buffer)) {
                to_read = sizeof(ram_buffer);
            }
            ram_ptr = (uint8_t *)((uint32_t) ram_buffer + (address & 3));
            read_bytes = to_read - (address & 3);
            if (read_bytes > num_bytes) {
                read_bytes = num_bytes;
            }
            nrfx_qspi_read(ram_buffer, to_read, address & ~3);
        } else {
            read_bytes = num_bytes & ~3;
            nrfx_qspi_read(dst, read_bytes, address);
        }
        if (ram_ptr != NULL) {
            memcpy(dst, ram_ptr, read_bytes);
            ram_ptr = NULL;
        }
        address += read_bytes;
        dst = (void *) ((uint32_t) dst + read_bytes);
        num_bytes -= read_bytes;
    }
    return 0;
}

static int
nrf_qspi_write(const struct hal_flash *dev, uint32_t address,
               const void *src, uint32_t num_bytes)
{
    uint32_t ram_buffer[4];
    uint8_t *ram_ptr = NULL;
    uint32_t written_bytes;

    while (num_bytes != 0) {
        /*
         * Use RAM buffer if src or address is not 4 bytes aligned,
         * or number of bytes to write is less then 4
         * or src pointer is not from RAM.
         */
        if (((address & 3) != 0) ||
            ((((uint32_t) src) & 3) != 0) ||
            num_bytes < 4 ||
            !nrfx_is_in_ram(src)) {
            uint32_t to_write;

            to_write = (num_bytes + (address & 3) + 3) & ~3;
            if (to_write > sizeof(ram_buffer)) {
                to_write = sizeof(ram_buffer);
            }
            memset(ram_buffer, 0xFF, sizeof(ram_buffer));
            ram_ptr = (uint8_t *)((uint32_t) ram_buffer + (address & 3));

            written_bytes = to_write - (address & 3);
            if (written_bytes > num_bytes) {
                written_bytes = num_bytes;
            }
            memcpy(ram_ptr, src, written_bytes);
            nrfx_qspi_write(ram_buffer, to_write, address & ~3);
        } else {
            written_bytes = num_bytes & ~3;
            nrfx_qspi_write(src, written_bytes, address);
        }

        address += written_bytes;
        src = (void *) ((uint32_t) src + written_bytes);
        num_bytes -= written_bytes;
    }
    return 0;
}

static int
nrf_qspi_erase_sector(const struct hal_flash *dev,
                      uint32_t sector_address)
{
    int8_t erases;

    erases = MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) / 4096;
    while (erases-- > 0) {
        nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, sector_address);
        sector_address += 4096;
    }

    return 0;
}

static int
nrf_qspi_erase(const struct hal_flash *dev, uint32_t address,
               uint32_t size)
{
    uint32_t end;

    address &= ~0xFFFU;
    end = address + size;

    if (end == MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) *
        MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE)) {
        nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_ALL, 0);
        return 0;
    }

    while (size) {
        if ((address & 0xFFFFU) == 0 && (size >= 0x10000)) {
            /* 64 KB erase if possible */
            nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_64KB, address);
            address += 0x10000;
            size -= 0x10000;
            continue;
        }

        nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, address);
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
nrf_qspi_sector_info(const struct hal_flash *dev, int idx,
                     uint32_t *address, uint32_t *sz)
{
    *address = idx * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    *sz = MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);

    return 0;
}

static int
nrf_qspi_init(const struct hal_flash *dev)
{
    int rc;

    nrfx_qspi_config_t config = {
        .pins = {
            .csn_pin = MYNEWT_VAL(QSPI_PIN_CS),
            .sck_pin = MYNEWT_VAL(QSPI_PIN_SCK),
            .io0_pin = MYNEWT_VAL(QSPI_PIN_DIO0),
            .io1_pin = MYNEWT_VAL(QSPI_PIN_DIO1),
#if (MYNEWT_VAL(QSPI_READOC) > 2) || (MYNEWT_VAL(QSPI_WRITEOC) > 1)
            .io2_pin = MYNEWT_VAL(QSPI_PIN_DIO2),
            .io3_pin = MYNEWT_VAL(QSPI_PIN_DIO3),
#else
            .io2_pin = NRF_QSPI_PIN_NOT_CONNECTED,
            .io3_pin = NRF_QSPI_PIN_NOT_CONNECTED,
#endif
        },
        .prot_if = {
            .readoc = MYNEWT_VAL(QSPI_READOC),
            .writeoc = MYNEWT_VAL(QSPI_WRITEOC),
            .addrmode = MYNEWT_VAL(QSPI_ADDRMODE),
            .dpmconfig = MYNEWT_VAL(QSPI_DPMCONFIG)
        },
        .phy_if = {
            .sck_delay = MYNEWT_VAL(QSPI_SCK_DELAY),
            .dpmen = 0,
            .spi_mode = MYNEWT_VAL(QSPI_SPI_MODE),
            .sck_freq = MYNEWT_VAL(QSPI_SCK_FREQ),
        },
        .xip_offset = MYNEWT_VAL(QSPI_XIP_OFFSET),
        .timeout = 0,
        .skip_gpio_cfg = true,
        .skip_psel_cfg = false,
    };

    rc = nrfx_qspi_init(&config, NULL, NULL);
    if (rc != NRFX_SUCCESS) {
        return -1;
    }

    rc = nrfx_qspi_activate(true);
    if (rc != NRFX_SUCCESS) {
        return -1;
    }

    return 0;
}

#endif
