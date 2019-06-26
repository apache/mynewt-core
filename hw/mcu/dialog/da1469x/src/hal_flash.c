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

#include <assert.h>
#include <string.h>
#include "mcu/mcu.h"
#include "defs/sections.h"
#include "mcu/da1469x_hal.h"
#include "hal/hal_flash_int.h"
#include "DA1469xAB.h"

#define CODE_QSPI_INLINE    __attribute__((always_inline)) inline

union da1469x_qspi_data_reg {
    uint32_t d32;
    uint16_t d16;
    uint8_t  d8;
};

static int da1469x_hff_read(const struct hal_flash *dev, uint32_t address,
                            void *dst, uint32_t num_bytes);
static int da1469x_hff_write(const struct hal_flash *dev, uint32_t address,
                             const void *src, uint32_t num_bytes);
static int da1469x_hff_erase_sector(const struct hal_flash *dev,
                                    uint32_t sector_address);
static int da1469x_hff_sector_info(const struct hal_flash *dev, int idx,
                                   uint32_t *address, uint32_t *sz);
static int da1469x_hff_init(const struct hal_flash *dev);

static const struct hal_flash_funcs da1469x_flash_funcs = {
    .hff_read = da1469x_hff_read,
    .hff_write = da1469x_hff_write,
    .hff_erase_sector = da1469x_hff_erase_sector,
    .hff_sector_info = da1469x_hff_sector_info,
    .hff_init = da1469x_hff_init
};

const struct hal_flash da1469x_flash_dev = {
    .hf_itf = &da1469x_flash_funcs,
    .hf_base_addr = 0,
    .hf_size = MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) * MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT),
    .hf_sector_cnt = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT),
    .hf_align = 1,
    .hf_erased_val = 0xff,
};

CODE_QSPI_INLINE static uint8_t
da1469x_qspi_read8(const struct hal_flash *dev)
{
    volatile union da1469x_qspi_data_reg *reg = (union da1469x_qspi_data_reg *)&QSPIC->QSPIC_READDATA_REG;

    return reg->d8;
}

CODE_QSPI_INLINE static void
da1469x_qspi_write8(const struct hal_flash *dev, uint8_t data)
{
    volatile union da1469x_qspi_data_reg *reg = (union da1469x_qspi_data_reg *)&QSPIC->QSPIC_WRITEDATA_REG;

    reg->d8 = data;
}

CODE_QSPI_INLINE static void
da1469x_qspi_write32(const struct hal_flash *dev, uint32_t data)
{
    volatile union da1469x_qspi_data_reg *reg = (union da1469x_qspi_data_reg *)&QSPIC->QSPIC_WRITEDATA_REG;

    reg->d32 = data;
}

CODE_QSPI_INLINE static void
da1469x_qspi_mode_single(const struct hal_flash *dev)
{
    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_SET_SINGLE_Msk;
    QSPIC->QSPIC_CTRLMODE_REG |= QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO2_OEN_Msk |
                                 QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO2_DAT_Msk |
                                 QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO3_OEN_Msk |
                                 QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO3_DAT_Msk;

    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_EN_CS_Msk;
    da1469x_qspi_write8(dev, 0xff);
    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_DIS_CS_Msk;
}

CODE_QSPI_INLINE static void
da1469x_qspi_mode_quad(const struct hal_flash *dev)
{
    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_SET_QUAD_Msk;
    QSPIC->QSPIC_CTRLMODE_REG &= ~(QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO2_OEN_Msk |
                                   QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO3_OEN_Msk);
}

CODE_QSPI_INLINE static void
da1469x_qspi_mode_dual(const struct hal_flash *dev)
{
    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_SET_DUAL_Msk;
    QSPIC->QSPIC_CTRLMODE_REG |= QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO2_OEN_Msk |
                                 QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO2_DAT_Msk |
                                 QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO3_OEN_Msk |
                                 QSPIC_QSPIC_CTRLMODE_REG_QSPIC_IO3_DAT_Msk;

    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_EN_CS_Msk;
    da1469x_qspi_write8(dev, 0xff);
    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_DIS_CS_Msk;
}

CODE_QSPI_INLINE static void
da1469x_qspi_mode_manual(const struct hal_flash *dev)
{
    QSPIC->QSPIC_CTRLMODE_REG &= ~QSPIC_QSPIC_CTRLMODE_REG_QSPIC_AUTO_MD_Msk;
}

CODE_QSPI_INLINE static void
da1469x_qspi_mode_auto(const struct hal_flash *dev)
{
    QSPIC->QSPIC_CTRLMODE_REG |= QSPIC_QSPIC_CTRLMODE_REG_QSPIC_AUTO_MD_Msk;
}

static sec_text_ram_core uint8_t
da1469x_qspi_cmd_read_status(const struct hal_flash *dev)
{
    uint8_t status;

    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_EN_CS_Msk;
    da1469x_qspi_write8(dev, 0x05);
    status = da1469x_qspi_read8(dev);
    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_DIS_CS_Msk;

    return status;
}

static sec_text_ram_core void
da1469x_qspi_cmd_enable_write(const struct hal_flash *dev)
{
    uint8_t status;

    do {
        QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_EN_CS_Msk;
        da1469x_qspi_write8(dev, 0x06);
        QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_DIS_CS_Msk;

        do {
            status = da1469x_qspi_cmd_read_status(dev);
        } while (status & 0x01);
    } while (!(status & 0x02));
}

static sec_text_ram_core void
da1469x_qspi_cmd_erase_sector(const struct hal_flash *dev, uint32_t address)
{
    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_EN_CS_Msk;
    address = __builtin_bswap32(address) & 0xffffff00;
    da1469x_qspi_write32(dev, address | 0x20);
    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_DIS_CS_Msk;
}

static sec_text_ram_core uint32_t
da1469x_qspi_cmd_write_page(const struct hal_flash *dev, uint32_t address,
                            const uint8_t *buf, uint32_t length)
{
    uint32_t written;
    uint32_t max_length;

    /* Make sure write does not cross page boundary */
    max_length = MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE) -
                 (address & (MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE) - 1));
    if (length > max_length) {
        length = max_length;
    }
    written = length;

    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_EN_CS_Msk;

    if (MYNEWT_VAL(QSPI_FLASH_CMD_QUAD_IO_PAGE_PROGRAM) > 0) {
        /* Only command in single mode address and data in quad mode */
        da1469x_qspi_write8(dev, MYNEWT_VAL(QSPI_FLASH_CMD_QUAD_IO_PAGE_PROGRAM));
        da1469x_qspi_mode_quad(dev);
        da1469x_qspi_write8(dev, (uint8_t)(address >> 16U));
        da1469x_qspi_write8(dev, (uint8_t)(address >> 8U));
        da1469x_qspi_write8(dev, (uint8_t)(address));
    } else if (MYNEWT_VAL(QSPI_FLASH_CMD_QUAD_INPUT_PAGE_PROGRAM) > 0) {
        /* Command and data in single mode, data in quad mode */
        address = __builtin_bswap32(address) & 0xffffff00;
        da1469x_qspi_write32(dev, address |
                                  MYNEWT_VAL(QSPI_FLASH_CMD_QUAD_INPUT_PAGE_PROGRAM));
        da1469x_qspi_mode_quad(dev);
    } else  if (MYNEWT_VAL(QSPI_FLASH_CMD_DUAL_INPUT_PAGE_PROGRAM) > 0) {
        /* Command and data in single mode, data in dual mode */
        address = __builtin_bswap32(address) & 0xffffff00;
        da1469x_qspi_write32(dev, address |
                                  MYNEWT_VAL(QSPI_FLASH_CMD_DUAL_INPUT_PAGE_PROGRAM));
        da1469x_qspi_mode_dual(dev);
    } else {
        /* Standard page program command in single mode */
        address = __builtin_bswap32(address) & 0xffffff00;
        da1469x_qspi_write32(dev, address | 0x02);
    }

    while (length >= 4) {
        da1469x_qspi_write32(dev, *(uint32_t *)buf);
        buf += 4;
        length -= 4;
    }

    while (length) {
        da1469x_qspi_write8(dev, *buf);
        buf++;
        length--;
    }

    QSPIC->QSPIC_CTRLBUS_REG = QSPIC_QSPIC_CTRLBUS_REG_QSPIC_DIS_CS_Msk;

    if ((MYNEWT_VAL(QSPI_FLASH_CMD_QUAD_IO_PAGE_PROGRAM) > 0) ||
        (MYNEWT_VAL(QSPI_FLASH_CMD_QUAD_INPUT_PAGE_PROGRAM) > 0) ||
        (MYNEWT_VAL(QSPI_FLASH_CMD_DUAL_INPUT_PAGE_PROGRAM) > 0)) {
        da1469x_qspi_mode_single(dev);
    }

    return written;
}

static sec_text_ram_core void
da1469x_qspi_wait_busy(const struct hal_flash *dev)
{
    uint8_t status;

    do {
        status = da1469x_qspi_cmd_read_status(dev);
    } while (status & 0x01);
}

static sec_text_ram_core int
da1469x_qspi_write(const struct hal_flash *dev, uint32_t address,
                   const void *src, uint32_t num_bytes)
{
    uint32_t primask;
    uint32_t written;

    __HAL_DISABLE_INTERRUPTS(primask);

    da1469x_qspi_mode_manual(dev);
    da1469x_qspi_mode_single(dev);

    da1469x_qspi_wait_busy(dev);

    while (num_bytes) {
        da1469x_qspi_cmd_enable_write(dev);

        written = da1469x_qspi_cmd_write_page(dev, address, src, num_bytes);
        address += written;
        src += written;
        num_bytes -= written;

        da1469x_qspi_wait_busy(dev);
    }

    da1469x_qspi_mode_quad(dev);
    da1469x_qspi_mode_auto(dev);

    /* XXX Should check if region was cached before flushing cache */
    CACHE->CACHE_CTRL1_REG |= CACHE_CACHE_CTRL1_REG_CACHE_FLUSH_Msk;

    __HAL_ENABLE_INTERRUPTS(primask);

    return 0;
}

static sec_text_ram_core int
da1469x_qspi_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    uint32_t primask;

    __HAL_DISABLE_INTERRUPTS(primask);

    da1469x_qspi_mode_manual(dev);
    da1469x_qspi_mode_single(dev);

    da1469x_qspi_wait_busy(dev);
    da1469x_qspi_cmd_enable_write(dev);
    da1469x_qspi_cmd_erase_sector(dev, sector_address);
    da1469x_qspi_wait_busy(dev);

    da1469x_qspi_mode_quad(dev);
    da1469x_qspi_mode_auto(dev);

    /* XXX Should check if region was cached before flushing cache */
    CACHE->CACHE_CTRL1_REG |= CACHE_CACHE_CTRL1_REG_CACHE_FLUSH_Msk;

    __HAL_ENABLE_INTERRUPTS(primask);

    return 0;
}

static int
da1469x_hff_read(const struct hal_flash *dev, uint32_t address, void *dst,
                 uint32_t num_bytes)
{
    if (address + num_bytes >= dev->hf_size) {
        return -1;
    }

    address += MCU_MEM_QSPIF_M_START_ADDRESS;

    memcpy(dst, (void *)address, num_bytes);

    return 0;
}

static int
da1469x_hff_write(const struct hal_flash *dev, uint32_t address,
                  const void *src, uint32_t num_bytes)
{
    uint8_t buf[ MYNEWT_VAL(QSPI_FLASH_READ_BUFFER_SIZE) ];
    uint32_t chunk_len;

    /*
     * We can write directly if 'src' is outside flash memory, otherwise we
     * need to read data to RAM first and then write to flash.
     */
    if (((uint32_t)src < MCU_MEM_QSPIF_M_START_ADDRESS) ||
        ((uint32_t)src >= MCU_MEM_QSPIF_M_END_ADDRESS)) {
        return da1469x_qspi_write(dev, address, src, num_bytes);
    }

    while (num_bytes) {
        if (num_bytes > MYNEWT_VAL(QSPI_FLASH_READ_BUFFER_SIZE)) {
            chunk_len = MYNEWT_VAL(QSPI_FLASH_READ_BUFFER_SIZE);
        } else {
            chunk_len = num_bytes;
        }

        memcpy(buf, src, chunk_len);

        /*
         * XXX need to improve this since each write will leave automode and
         *     also flush cache at the end
         */
        da1469x_qspi_write(dev, address, buf, chunk_len);
        address += chunk_len;
        src += chunk_len;
        num_bytes -= chunk_len;
    }

    return 0;
}

static int
da1469x_hff_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    return da1469x_qspi_erase_sector(dev, sector_address);
}

static int
da1469x_hff_sector_info(const struct hal_flash *dev, int idx,
                        uint32_t *address, uint32_t *sz)
{
    assert(idx < dev->hf_sector_cnt);

    /* XXX assume uniform sector size */
    *sz = MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    *address = idx * (*sz);

    return 0;
}

static int
da1469x_hff_init(const struct hal_flash *dev)
{
    return 0;
}
