/**
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

#include <xc.h>
#include <assert.h>
#include <hal/hal_flash_int.h>
#include <mcu/mips_hal.h>
#include <string.h>
#include <sys/kmem.h>

#define VIRT_TO_PHY(ADDRESS)   (unsigned int)((int)(ADDRESS) & 0x1FFFFFFF)
#define PHY_TO_VIRT(ADDRESS)   (unsigned int)((int)(ADDRESS) | 0x80000000)
#define PIC32MZ_FLASH_SECTOR_SZ     (16 * 1024)
#define WORD_SIZE           (4)
#define QUADWORD_SIZE       (4 * WORD_SIZE)
#define QUADWORD_PROGRAM    (0b0010)
#define WORD_PROGRAM        (0b0001)
#define ERASE_PAGE          (0b0100)

static int pic32mz_flash_read(const struct hal_flash *dev, uint32_t address,
                              void *dst, uint32_t num_bytes);
static int pic32mz_flash_write(const struct hal_flash *dev, uint32_t address,
                               const void *src, uint32_t num_bytes);
static int pic32mz_flash_erase_sector(const struct hal_flash *dev,
                                      uint32_t sector_address);
static int pic32mz_flash_sector_info(const struct hal_flash *dev, int idx,
                                     uint32_t *address, uint32_t *sz);
static int pic32mz_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs pic32mz_flash_funcs = {
    .hff_read = pic32mz_flash_read,
    .hff_write = pic32mz_flash_write,
    .hff_erase_sector = pic32mz_flash_erase_sector,
    .hff_sector_info = pic32mz_flash_sector_info,
    .hff_init = pic32mz_flash_init
};

const struct hal_flash pic32mz_flash_dev = {
    .hf_itf = &pic32mz_flash_funcs,
    .hf_base_addr = 0x1D000000,
    .hf_size = __PIC32_FLASH_SIZE * 1024,
    .hf_sector_cnt = __PIC32_FLASH_SIZE / 16,
    .hf_align = 4,     /* num bytes must be a multiple of 4 as writes can only
                        * be done on word boundary. This also assumes that
                        * ECC memory is disabled (default on Wi-Fire board).
                        */
    .hf_erased_val = 0xff,
};

static int
flash_do_op(uint32_t op)
{
    uint32_t ctx;

    __HAL_DISABLE_INTERRUPTS(ctx);

    NVMCON = _NVMCON_WREN_MASK | (op & _NVMCON_NVMOP_MASK);

    /*
     * Disable core timer by setting the DC flag in CP0 Cause register.
     * If the core timer is not disabled, the kernel would miss the core timer
     * interrupt while the CPU is stalling.
     */
    _CP0_SET_CAUSE(_CP0_GET_CAUSE() | _CP0_CAUSE_DC_MASK);

    NVMKEY = 0x0;
    NVMKEY = 0xAA996655;
    NVMKEY = 0x556699AA;
    NVMCONSET = _NVMCON_WR_MASK;

    while (NVMCON & _NVMCON_WR_MASK) {}

    /* Re-enable core timer  */
    _CP0_SET_CAUSE(_CP0_GET_CAUSE() & ~_CP0_CAUSE_DC_MASK);

    __HAL_ENABLE_INTERRUPTS(ctx);

    NVMCONCLR = _NVMCON_WREN_MASK;

    return (NVMCON & (_NVMCON_WRERR_MASK | _NVMCON_LVDERR_MASK)) ? -1 : 0;
}

static int
pic32mz_flash_read(const struct hal_flash *dev, uint32_t address,
                   void *dst, uint32_t num_bytes)
{
    (void)dev;
    memcpy(dst, (void *)PA_TO_KVA1(address), num_bytes);
    return 0;
}

/**
 * Function returns address to 4 byte aligned data
 * @param src      pointer to source data
 * @param aligned_buffer address of aligned buffer to use in case src in unaligned
 * @param aligned_buffer_size size of aligned buffer in bytes
 * @param num_bytes number of bytes to provide.
 * @return src if it is 4 byte aligned otherwise aligned_buffer
 */
const uint32_t *
aligned_ptr(const void *src, uint32_t *aligned_buffer, uint32_t aligned_buffer_size, uint32_t num_bytes)
{
    if (((uint32_t)src & 3) == 0) {
        /* Source is aligned, there is no need to copy data */
        return (const uint32_t *)src;
    }
    if (num_bytes > aligned_buffer_size) {
        num_bytes = aligned_buffer_size;
    }
    memcpy(aligned_buffer, src, num_bytes);
    return aligned_buffer;
}

static int
pic32mz_flash_write(const struct hal_flash *dev, uint32_t address,
                    const void *src, uint32_t num_bytes)
{
    (void)dev;
    const uint32_t *data;
    uint32_t aligned_data[4];

    if ((address & 3)) {
        return -1;
    }

    /* Write flash by word until the next quadword boundary is reached */
    while (address & (QUADWORD_SIZE -1) && num_bytes >= WORD_SIZE) {
        data = aligned_ptr(src, aligned_data, sizeof(aligned_data), num_bytes);
        NVMADDR = address;
        address += WORD_SIZE;
        src += WORD_SIZE;
        NVMDATA0 = *data;

        if (flash_do_op(WORD_PROGRAM)) {
            return -1;
        }
        num_bytes -= WORD_SIZE;
    }

    while (num_bytes >= QUADWORD_SIZE) {
        data = aligned_ptr(src, aligned_data, sizeof(aligned_data), num_bytes);
        NVMADDR = address;
        address += QUADWORD_SIZE;
        src += QUADWORD_SIZE;

        NVMDATA0 = data[0];
        NVMDATA1 = data[1];
        NVMDATA2 = data[2];
        NVMDATA3 = data[3];

        if (flash_do_op(QUADWORD_PROGRAM)) {
            return -1;
        }
        num_bytes -= QUADWORD_SIZE;
    }

    while (num_bytes >= WORD_SIZE) {
        data = aligned_ptr(src, aligned_data, sizeof(aligned_data), num_bytes);
        NVMADDR = address;
        address += WORD_SIZE;
        src += WORD_SIZE;
        NVMDATA0 = *data;

        if (flash_do_op(WORD_PROGRAM)) {
            return -1;
        }
        num_bytes -= WORD_SIZE;
    }
    if (num_bytes > 0) {
        aligned_data[0] = 0xFFFFFFFF;
        memcpy(aligned_data, src, num_bytes);

        NVMADDR = address;
        NVMDATA0 = aligned_data[0];

        if (flash_do_op(WORD_PROGRAM)) {
            return -1;
        }
    }

    return 0;
}

static int
pic32mz_flash_erase_sector(const struct hal_flash *dev,
                           uint32_t sector_address)
{
    (void)dev;
    NVMADDR = sector_address;
    return flash_do_op(ERASE_PAGE);
}

static int
pic32mz_flash_sector_info(const struct hal_flash *dev, int idx,
                          uint32_t *address, uint32_t *sz)
{
    assert(idx < pic32mz_flash_dev.hf_sector_cnt);
    *address = pic32mz_flash_dev.hf_base_addr + idx * PIC32MZ_FLASH_SECTOR_SZ;
    *sz = PIC32MZ_FLASH_SECTOR_SZ;
    return 0;
}

static int
pic32mz_flash_init(const struct hal_flash *dev)
{
    (void)dev;
    return 0;
}
