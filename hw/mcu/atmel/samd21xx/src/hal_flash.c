/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Internal flash for STM32F3.
 * Size of the flash depends on the MCU model, flash is memory mapped
 * and is divided to 2k sectors throughout.
 * Programming is done 2 bytes at a time.
 */
#include <string.h>
#include <assert.h>
#include <hal/hal_flash_int.h>
#include <nvm.h>


#define SAMD21_FLASH_START_ADDR         (0x0)

/* The samd all have 4 flash pages per row of flash.
 * Each page is individually writeable. Each page is individually
 * erasable */
#define SAMD21_FLASH_PAGES_PER_ROW   (4)

/* sectors for this flash are really small.  Use 4 pages per sector */
#define SAMD21_FLASH_ROWS_PER_SECTOR   (4)

#define SAMD21_FLASH_PAGES_PER_SECTOR   (SAMD21_FLASH_PAGES_PER_ROW*SAMD21_FLASH_ROWS_PER_SECTOR)

static int samd21_flash_read(const struct hal_flash *dev, uint32_t address,
                             void *dst, uint32_t num_bytes);

static int samd21_flash_write(const struct hal_flash *dev, uint32_t address,
                              const void *src, uint32_t num_bytes);

static int samd21_flash_erase_sector(const struct hal_flash *dev,
                                     uint32_t sector_address);

static int samd21_flash_sector_info(const struct hal_flash *dev, int idx,
                                    uint32_t *addr, uint32_t *sz);

static int samd21_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs samd21_flash_funcs = {
    .hff_read = samd21_flash_read,
    .hff_write = samd21_flash_write,
    .hff_erase_sector = samd21_flash_erase_sector,
    .hff_sector_info = samd21_flash_sector_info,
    .hff_init = samd21_flash_init,
};

/* rest will be filled out at runtime */
struct hal_flash samd21_flash_dev = {
    .hf_itf = &samd21_flash_funcs,
};

static int
samd21_flash_read(const struct hal_flash *dev, uint32_t address,
                  void *dst, uint32_t num_bytes)
{
    int base_address;
    int offset;
    struct nvm_parameters params;
    uint8_t page_buffer[64];
    uint8_t *pdst = dst;

    /* make sure this fits into our stack buffer  */
    nvm_get_parameters(&params);
    assert(params.page_size <= sizeof(page_buffer));

    /* get the page address (this is not a sector, there are 4 pages per
     * row */
    while (num_bytes) {
        int read_len;

        base_address = address & ~(params.page_size - 1);

        offset = address - base_address;
        read_len = params.page_size - offset;

        if (read_len > num_bytes) {
            read_len = num_bytes;
        }

        if (nvm_read_buffer(base_address, page_buffer, params.page_size)
            != STATUS_OK) {
            return -1;
        }

        /* move the pointers since this is a sure thing now */
        num_bytes -= read_len;
        address += read_len;

        /* copy it into the page buffer */
        while (read_len--) {
            *pdst++ = page_buffer[offset++];
        }
    }
    return 0;
}

static int
samd21_flash_write(const struct hal_flash *dev, uint32_t address,
                   const void *src, uint32_t len)
{
    int base_address;
    int offset;
    struct nvm_parameters params;
    uint8_t page_buffer[64];
    const uint8_t *psrc = src;

    /* make sure this fits into our stack buffer  */
    nvm_get_parameters(&params);
    assert(params.page_size <= sizeof(page_buffer));

    /* get the page address (this is not a sector, there are 4 pages per
     * row */
    while (len) {
        int write_len;

        base_address = address & ~(params.page_size - 1);

        offset = address - base_address;
        write_len = params.page_size - offset;

        if (write_len > len) {
            write_len = len;
        }

        if (nvm_read_buffer(base_address, page_buffer, params.page_size)
            != STATUS_OK) {
            return -1;
        }

        /* move the pointers since this is a sure thing */
        len -= write_len;
        address += write_len;

        /* copy it into the page buffer */
        while (write_len--) {
            if (page_buffer[offset] != 0xff) {
                /*
                 * Cannot write to non-erased location.
                 */
                return -1;
            }
            page_buffer[offset++] = *psrc++;
        }

        /* 0x000054a4 */
#if 1
        /* write back the page buffer buffer */
        if (nvm_write_buffer(base_address, page_buffer, params.page_size)
            != STATUS_OK) {
            return -1;
        }
#endif
    }
    return 0;
}

static int
samd21_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    struct nvm_parameters params;
    int rc;
    int i;

    nvm_get_parameters(&params);

    /* erase all rows in the sector */
    for (i = 0; i < SAMD21_FLASH_ROWS_PER_SECTOR; i++) {
        uint32_t row_address = sector_address +
                               i * SAMD21_FLASH_PAGES_PER_ROW * params.page_size;
        rc = nvm_erase_row(row_address);
        if (rc != STATUS_OK) {
            return -1;
        }
    }
    return 0;
}

/* samd21 flash always starts as 0x000000 */
static int
samd21_flash_sector_info(const struct hal_flash *dev, int idx,
                         uint32_t *addr, uint32_t *sz)
{
    struct nvm_parameters params;
    int sector_size;
    int sector_cnt;

    nvm_get_parameters(&params);
    sector_cnt = params.nvm_number_of_pages / SAMD21_FLASH_PAGES_PER_SECTOR;
    sector_size = params.page_size * SAMD21_FLASH_PAGES_PER_SECTOR;

    if ((idx >= sector_cnt) || (idx < 0)) {
        return -1;
    }

    *sz = sector_size;
    *addr = idx * sector_size + SAMD21_FLASH_START_ADDR;
    return 0;
}

static int
samd21_flash_init(const struct hal_flash *dev)
{
    int rc;
    struct nvm_config cfg;
    struct nvm_parameters params;
    nvm_get_config_defaults(&cfg);

    cfg.manual_page_write = false;
    rc = nvm_set_config(&cfg);
    if (rc != STATUS_OK) {
        return -1;
    }

    nvm_get_parameters(&params);

    /* the samd21 flash doesn't use sector terminology. They use Row and
     * page.  A row contains 4 pages. Each pages is a fixed size.  You can
     * only erase based on row. Here I will map the rows to sectors and
     * deal with pages inside this driver.   */
    samd21_flash_dev.hf_itf = &samd21_flash_funcs;
    samd21_flash_dev.hf_base_addr = SAMD21_FLASH_START_ADDR;
    samd21_flash_dev.hf_size = params.nvm_number_of_pages * params.page_size;
    samd21_flash_dev.hf_sector_cnt =
        params.nvm_number_of_pages / SAMD21_FLASH_PAGES_PER_SECTOR;
    samd21_flash_dev.hf_align = 1;
    samd21_flash_dev.hf_erased_val = 0xff;

    return 0;
}
