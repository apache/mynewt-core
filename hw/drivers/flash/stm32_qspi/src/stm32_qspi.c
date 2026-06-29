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

#include <os/mynewt.h>
#include <os/link_tables.h>
#include <arm_mpu.h>
#include <bsp/bsp.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include <mcu/cmsis_nvic.h>
#include <hal/hal_flash_int.h>
#include <mcu/stm32_hal.h>
#include <stm32_common/mcu.h>

#if MYNEWT_VAL(STM32_QSPI_FLASH_SECTOR_SIZE) < 1
#error STM32_QSPI_FLASH_SECTOR_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(STM32_QSPI_FLASH_PAGE_SIZE) < 1
#error STM32_QSPI_FLASH_PAGE_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(STM32_QSPI_FLASH_SECTOR_COUNT) < 1
#error STM32_QSPI_FLASH_SECTOR_COUNT must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(STM32_QSPI_PIN_CS) < 0
#error STM32_QSPI_PIN_CS must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(STM32_QSPI_PIN_SCK) < 0
#error STM32_QSPI_PIN_SCK must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(STM32_QSPI_PIN_DIO0) < 0
#error STM32_QSPI_PIN_DIO0 must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(STM32_QSPI_PIN_DIO1) < 0
#error STM32_QSPI_PIN_DIO1 must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(STM32_QSPI_PIN_DIO2) < 0 &&                                    \
    (MYNEWT_VAL(STM32_QSPI_READOC) > 2 || MYNEWT_VAL(STM32_QSPI_WRITEOC) > 1)
#error STM32_QSPI_PIN_DIO2 must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(STM32_QSPI_PIN_DIO3) < 0 &&                                    \
    (MYNEWT_VAL(STM32_QSPI_READOC) > 2 || MYNEWT_VAL(STM32_QSPI_WRITEOC) > 1)
#error STM32_QSPI_PIN_DIO3 must be set to the correct value in bsp syscfg.yml
#endif

#define BUSY_MASK (1 << MYNEWT_VAL_STM32_QSPI_FLASH_BUSY_BIT)
#define WE_MASK   (1 << MYNEWT_VAL_STM32_QSPI_FLASH_WE_BIT)
#define QE_BIT    (MYNEWT_VAL_STM32_QSPI_FLASH_QE_BIT)
#define QE_MASK   (1 << (QE_BIT))

#define QSPI_READ     MYNEWT_VAL_STM32_QSPI_FLASH_READ_COMMAND
#define QSPI_ERASE64K MYNEWT_VAL_STM32_QSPI_FLASH_ERASE_64K_COMMAND

#define QSPI_ADDR_SIZE    MYNEWT_VAL_STM32_QSPI_FLASH_ADDRESS_LENGTH
#define QSPI_SECTOR_SIZE  MYNEWT_VAL_STM32_QSPI_FLASH_SECTOR_SIZE
#define QSPI_SECTOR_COUNT MYNEWT_VAL_STM32_QSPI_FLASH_SECTOR_COUNT
#define QSPI_BASE_ADDRESS MYNEWT_VAL_STM32_QSPI_FLASH_BASE_ADDRESS
#define QSPI_SIZE         ((QSPI_SECTOR_COUNT) * (QSPI_SECTOR_SIZE))

static int stm32_qspi_read(const struct hal_flash *dev, uint32_t address,
                           void *dst, uint32_t num_bytes);
static int stm32_qspi_write(const struct hal_flash *dev, uint32_t address,
                            const void *src, uint32_t num_bytes);
static int stm32_qspi_erase_sector(const struct hal_flash *dev, uint32_t sector_address);
static int stm32_qspi_sector_info(const struct hal_flash *dev, int idx,
                                  uint32_t *address, uint32_t *sz);
static int stm32_qspi_init(const struct hal_flash *dev);
static int stm32_qspi_erase(const struct hal_flash *dev, uint32_t address,
                            uint32_t size);

static const struct hal_flash_funcs stm32_qspi_funcs = {
    .hff_read = stm32_qspi_read,
    .hff_write = stm32_qspi_write,
    .hff_erase_sector = stm32_qspi_erase_sector,
    .hff_sector_info = stm32_qspi_sector_info,
    .hff_init = stm32_qspi_init,
    .hff_erase = stm32_qspi_erase
};

struct stm32_qspi_dev_data {
    QSPI_HandleTypeDef hqspi;
    bool memory_mapped;
    bool ready;
    bool quad_enabled;
#if MYNEWT_VAL(OS_SCHEDULING)
    struct os_mutex lock;
#endif
};

#ifndef __UNUSED
#define __UNUSED __attribute__((unused))
#endif

struct stm32_qspi_dev_data stm32_qspi_data = {
    .hqspi = {
        .Instance = QUADSPI,
        .Init = {
            .ClockPrescaler = 5,
            .FifoThreshold = 4,
            .SampleShifting = QSPI_SAMPLE_SHIFTING_NONE,
            .FlashSize = 20,
            .ChipSelectHighTime = QSPI_CS_HIGH_TIME_4_CYCLE,
            .ClockMode = QSPI_CLOCK_MODE_3,
#ifdef QUADSPI_CR_DFM
            .FlashID = QSPI_FLASH_ID_1,
            .DualFlash = QSPI_DUALFLASH_DISABLE,
#endif
        },
    },
};

struct stm32_qspi_dev {
    struct hal_flash hal_flash;
    uint32_t limit;
    struct stm32_qspi_dev_data *data;
};

const struct stm32_qspi_dev stm32_qspi_dev = {
    .hal_flash = {
        .hf_itf = &stm32_qspi_funcs,
        .hf_base_addr = QSPI_BASE_ADDRESS,
        .hf_size = QSPI_SIZE,
        .hf_sector_cnt = QSPI_SECTOR_COUNT,
        .hf_align = 1,
        .hf_erased_val = 0xff,
    },
    .limit = QSPI_BASE_ADDRESS + QSPI_SIZE,
    .data = &stm32_qspi_data,
};

const struct hal_flash *const qspi_flash_0 = &stm32_qspi_dev.hal_flash;

typedef uint32_t qspi_cmd_t;

#define FLD(v, msk, pos) (((v) & (msk)) << (pos))

#define LMOD(ln, pos) FLD((ln == 4) ? 3 : (ln), 3, pos)

#define CMD(ins, cl, adl, altl, dl, ab, dmyc)                                 \
    (ins | LMOD(cl, 8) | LMOD(adl, 10) | LMOD(altl, 12) | LMOD(dl, 14) |      \
     FLD(ab, 0xFF, 16) | FLD(dmyc, 0x1F, 24))

#define CMD_NO_ARG(ins, cl)        CMD(ins, 1, 0, 0, 0, 0, 0)
#define CMD_WITH_DATA(ins, cl, dl) CMD(ins, cl, 0, 0, dl, 0, 0)
#define CMD_WITH_ADDR(ins, cl, al) CMD(ins, cl, al, 0, 0, 0, 0)

#define CMD_INS(cmd)      ((cmd) & 0xFF)
#define CMD_INS_MOD(cmd)  ((((cmd) >> 8) & 3) << QUADSPI_CCR_IMODE_Pos)
#define CMD_ADR_MOD(cmd)  ((((cmd) >> 10) & 3) << QUADSPI_CCR_ADMODE_Pos)
#define CMD_ALT_MOD(cmd)  ((((cmd) >> 12) & 3) << QUADSPI_CCR_ABMODE_Pos)
#define CMD_DAT_MOD(cmd)  ((((cmd) >> 14) & 3) << QUADSPI_CCR_DMODE_Pos)
#define CMD_ALT(cmd)      (((cmd) >> 16) & 0xFF)
#define CMD_DCYC(cmd)     (((cmd) >> 24) & 0x1F)
#define CMD_ADR_SIZE(cmd) ((QSPI_ADDR_SIZE - 1) << (QUADSPI_CCR_ADSIZE_Pos))

#define COMMAND_ADDR_DATA(cmd, adr, cnt)                                      \
    {                                                                         \
        .Instruction = CMD_INS(cmd),                                          \
        .AlternateBytes = CMD_ALT(cmd),                                       \
        .AddressSize = CMD_ADR_SIZE(cmd),                                     \
        .DummyCycles = CMD_DCYC(cmd),                                         \
        .InstructionMode = CMD_INS_MOD(cmd),                                  \
        .AddressMode = CMD_ADR_MOD(cmd),                                      \
        .AlternateByteMode = CMD_ALT_MOD(cmd),                                \
        .DataMode = CMD_DAT_MOD(cmd),                                         \
        .NbData = cnt,                                                        \
    }

#define COMMAND_DATA(cmd, cnt) COMMAND_ADDR_DATA(cmd, 0, cnt)
#define COMMAND(cmd)           COMMAND_DATA(cmd, 0)

/* Write status register 1 */
#define CMD_01h CMD_WITH_DATA(0x01, 1, 1)
/* Write status register 2 */
#define CMD_31h CMD_WITH_DATA(0x31, 1, 1)
/* Write status register 3 */
#define CMD_11h CMD_WITH_DATA(0x11, 1, 1)
/* Read status register 1 */
#define CMD_05h CMD_WITH_DATA(0x05, 1, 1)
/* Read status register 2 */
#define CMD_35h CMD_WITH_DATA(0x35, 1, 1)
/* Read status register 3 */
#define CMD_15h CMD_WITH_DATA(0x15, 1, 1)
/* Read SFDP register */
#define CMD_5Ah CMD(0x5A, 1, 1, 0, 1, 0, 8)
/* Read security register */
#define CMD_58h CMD(0x58, 1, 1, 0, 1, 0, 8)
/* Read block lock */
#define CMD_3Dh CMD(0x3D, 1, 1, 0, 1, 0, 0)
/* Write enable */
#define CMD_06h CMD_NO_ARG(0x06, 1)
/* Write disable */
#define CMD_04h CMD_NO_ARG(0x04, 1)
/* Read JEDEC IO */
#define CMD_9Fh CMD_WITH_DATA(0x9F, 1, 1)
/* Read data */
#define CMD_03h CMD(0x03, 1, 1, 0, 1, 0, 0)
/* Fast read */
#define CMD_0Bh CMD(0x0B, 1, 1, 0, 1, 0, 8)
/* Fast read dual I/O */
/* Alternate byte 0xFx should be sent at 2 lines, however
 * it may be not enough to switch to input hence 1 byte
 * written at 4 lines and 2 extra dummy cycles are provided
 */
#define CMD_BBh CMD(0xBB, 1, 2, 4, 2, 0xFF, 2)
/* Fast read quad I/O */
#define CMD_EBh CMD(0xEB, 1, 4, 4, 4, 0xFF, 4)
/* Page program */
#define CMD_02h CMD(0x02, 1, 1, 0, 1, 0, 0)
/* Quad input page program */
#define CMD_32h CMD(0x32, 1, 1, 0, 4, 0, 0)
/* Sector erase */
#define CMD_20h CMD_WITH_ADDR(0x20, 1, 1)
/* Block 32K erase */
#define CMD_52h CMD_WITH_ADDR(0x52, 1, 1)
/* Block 64K erase */
#define CMD_D8h CMD_WITH_ADDR(0xD8, 1, 1)
/* Chip erase */
#define CMD_C7h CMD_NO_ARG(0xC7, 1)
/* Read Unique ID */
#define CMD_4Bh CMD(0x4B, 1, 0, 1, 1, 0xFF, 24)

#if MYNEWT_VAL(OS_SCHEDULING)
static void
stm32_qspi_lock(struct stm32_qspi_dev *qspi)
{
    os_mutex_pend(&qspi->data->lock, OS_WAIT_FOREVER);
}

static void
stm32_qspi_unlock(struct stm32_qspi_dev *qspi)
{
    os_mutex_release(&qspi->data->lock);
}
#else
#define stm32_qspi_lock(qspi)
#define stm32_qspi_unlock(qspi)
#endif

int
stm32_qspi_memory_map(struct stm32_qspi_dev *qspi)
{
    QSPI_HandleTypeDef *hqspi = &qspi->data->hqspi;
    QSPI_CommandTypeDef mmcmd = COMMAND(QSPI_READ);
    QSPI_MemoryMappedTypeDef mmcfg = {
        .TimeOutPeriod = 10,
        .TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE,
    };
    int rc;

    rc = HAL_QSPI_MemoryMapped(hqspi, &mmcmd, &mmcfg);
    qspi->data->memory_mapped = rc == HAL_OK;

    __DSB();
    __ISB();

    return rc;
}

int
stm32_qspi_write_enable(struct stm32_qspi_dev *qspi)
{
    QSPI_HandleTypeDef *hqspi = &qspi->data->hqspi;
    QSPI_CommandTypeDef cmd = COMMAND(CMD_06h);
    int ret;

    stm32_qspi_lock(qspi);

    ret = HAL_QSPI_Command(hqspi, &cmd, 1000);

    stm32_qspi_unlock(qspi);

    return ret;
}

int
stm32_qspi_command_with_data_out(struct stm32_qspi_dev *qspi,
                                 QSPI_CommandTypeDef *cmd, const uint8_t *data)
{
    QSPI_HandleTypeDef *hqspi = &qspi->data->hqspi;
    int ret;

    stm32_qspi_lock(qspi);

    ret = HAL_QSPI_Command(hqspi, cmd, 1000);
    if (HAL_OK == ret) {
        ret = HAL_QSPI_Transmit(hqspi, (void *)data, 1000);
    }

    stm32_qspi_unlock(qspi);

    return ret;
}

int
stm32_qspi_command_with_data_in(struct stm32_qspi_dev *qspi,
                                QSPI_CommandTypeDef *cmd, uint8_t *data)
{
    QSPI_HandleTypeDef *hqspi = &qspi->data->hqspi;
    int ret;

    stm32_qspi_lock(qspi);

    ret = HAL_QSPI_Command(hqspi, cmd, 1000);
    if (HAL_OK == ret) {
        ret = HAL_QSPI_Receive(hqspi, data, 1000);
    }

    stm32_qspi_unlock(qspi);

    return ret;
}

int
stm32_qspi_write_status_register(struct stm32_qspi_dev *qspi, qspi_cmd_t cmd,
                                 const uint8_t *status, uint8_t count)
{
    int ret;
    QSPI_CommandTypeDef c = COMMAND_DATA(cmd, count);

    stm32_qspi_lock(qspi);

    stm32_qspi_write_enable(qspi);

    ret = stm32_qspi_command_with_data_out(qspi, &c, status);

    stm32_qspi_unlock(qspi);

    return ret;
}

int
stm32_qspi_write_status_register1(struct stm32_qspi_dev *qspi, uint8_t *status,
                                  uint8_t count)
{
    return stm32_qspi_write_status_register(qspi, CMD_01h, status, count);
}

int
stm32_qspi_write_status_register2(struct stm32_qspi_dev *qspi, uint8_t status2)
{
    return stm32_qspi_write_status_register(qspi, CMD_31h, &status2, 1);
}

int
stm32_qspi_write_status_register3(struct stm32_qspi_dev *qspi, uint8_t status3)
{
    return stm32_qspi_write_status_register(qspi, CMD_11h, &status3, 1);
}

int
stm32_qspi_read_status_register(struct stm32_qspi_dev *qspi, qspi_cmd_t cmd,
                                uint8_t *status)
{
    int ret;
    QSPI_CommandTypeDef c = COMMAND_DATA(cmd, 1);

    ret = stm32_qspi_command_with_data_in(qspi, &c, status);

    return ret;
}

int
stm32_qspi_read_status_register1(struct stm32_qspi_dev *qspi, uint8_t *status)
{
    return stm32_qspi_read_status_register(qspi, CMD_05h, status);
}

int
stm32_qspi_read_status_register2(struct stm32_qspi_dev *qspi, uint8_t *status)
{
    return stm32_qspi_read_status_register(qspi, CMD_35h, status);
}

int
stm32_qspi_read_status_register3(struct stm32_qspi_dev *qspi, uint8_t *status)
{
    return stm32_qspi_read_status_register(qspi, CMD_15h, status);
}

int
stm32_qspi_quad_enable(struct stm32_qspi_dev *qspi)
{
    uint8_t status[2];
    int ret = 0;

    if (!qspi->data->quad_enabled) {
        ret = stm32_qspi_read_status_register1(qspi, &status[0]);
        if (ret) {
            goto err;
        }
        ret = stm32_qspi_read_status_register2(qspi, &status[1]);
        if (ret) {
            goto err;
        }
        if (((status[0] | (status[1] << 8)) & QE_MASK) == 0) {
            status[0] |= QE_MASK;
            status[1] |= QE_MASK >> 8;
            ret = stm32_qspi_write_status_register1(qspi, status, QE_BIT > 7 ? 2 : 1);
        }
        qspi->data->quad_enabled = ret == 0;
    }
err:
    return ret;
}

int
stm32_qspi_read_unique_id(struct stm32_qspi_dev *qspi, uint8_t *id)
{
    QSPI_CommandTypeDef c = COMMAND_DATA(CMD_4Bh, 8);

    return stm32_qspi_command_with_data_in(qspi, &c, id);
}

int
stm32_qspi_read_sfdp(struct stm32_qspi_dev *qspi, uint8_t address,
                     uint8_t *data, uint8_t count)
{
    QSPI_CommandTypeDef c = COMMAND_ADDR_DATA(CMD_5Ah, address, count);

    return stm32_qspi_command_with_data_in(qspi, &c, data);
}

bool
stm32_qspi_check_ready(struct stm32_qspi_dev *qspi)
{
    uint8_t status[1];

    if (!qspi->data->ready) {
        stm32_qspi_read_status_register1(qspi, status);
        qspi->data->ready = (status[0] & BUSY_MASK) == 0;
    }

    return qspi->data->ready;
}

void
stm32_qspi_wait_for_ready(struct stm32_qspi_dev *qspi)
{
    while (!stm32_qspi_check_ready(qspi)) {
    }
}

int
stm32_qspi_read_jedec_id(struct stm32_qspi_dev *qspi, uint8_t id[3])
{
    int ret;
    QSPI_CommandTypeDef cmd = COMMAND_DATA(CMD_9Fh, 3);

    ret = stm32_qspi_command_with_data_in(qspi, &cmd, id);

    return ret;
}

static int
stm32_qspi_read(const struct hal_flash *dev, uint32_t address, void *dst,
                uint32_t num_bytes)
{
    struct stm32_qspi_dev *qspi = (struct stm32_qspi_dev *)dev;

    stm32_qspi_lock(qspi);

    stm32_qspi_wait_for_ready(qspi);

    if (address >= dev->hf_base_addr && address + num_bytes <= qspi->limit) {
        if (qspi->data->memory_mapped) {
            memcpy(dst, (void *)address, num_bytes);
        } else {
            QSPI_CommandTypeDef cmd = COMMAND_DATA(QSPI_READ, num_bytes);

            cmd.Address = address - dev->hf_base_addr;

            stm32_qspi_command_with_data_in(qspi, &cmd, dst);
        }
    }

    stm32_qspi_unlock(qspi);

    return 0;
}

static int
stm32_qspi_write(const struct hal_flash *dev, uint32_t address,
                 const void *src, uint32_t num_bytes)
{
    struct stm32_qspi_dev *qspi = (struct stm32_qspi_dev *)dev;
    QSPI_CommandTypeDef cmd = COMMAND(CMD_02h);
    int rc = HAL_OK;
    uint32_t write_limit;
    uint32_t page_limit;
    uint32_t chunk;
    uint32_t flash_address;
    uint32_t left = num_bytes;
    int sr = 0;

    if (address < dev->hf_base_addr || address >= qspi->limit) {
        return OS_INVALID_PARM;
    }

    stm32_qspi_lock(qspi);

    flash_address = address - dev->hf_base_addr;
    write_limit = flash_address + left;

    /*
     * For memory mapped device, first disable interrupts so no code will try
     * to access region that is not accessible for a while.
     */
    if (qspi->data->memory_mapped) {
        OS_ENTER_CRITICAL(sr);
        HAL_QSPI_Abort(&qspi->data->hqspi);
    }

    while (rc == HAL_OK && left != 0) {
        page_limit = (flash_address | 0xFF) + 1;
        chunk = min(page_limit, write_limit) - flash_address;

        stm32_qspi_wait_for_ready(qspi);

        stm32_qspi_write_enable(qspi);

        cmd.Address = flash_address;
        cmd.NbData = chunk;

        rc = stm32_qspi_command_with_data_out(qspi, &cmd, src);

        qspi->data->ready = false;

        flash_address += chunk;
        src += chunk;
        left -= chunk;
    }

    /* Restore memory mapped mode if was enabled */
    if (qspi->data->memory_mapped) {
        stm32_qspi_wait_for_ready(qspi);
#if MYNEWT_VAL_STM32_ENABLE_DCACHE
        SCB_InvalidateDCache_by_Addr((void *)address, num_bytes);
#endif
        stm32_qspi_memory_map(qspi);
        OS_EXIT_CRITICAL(sr);
    }

    stm32_qspi_unlock(qspi);

    return rc;
}

static int
stm32_qspi_erase_cmd(struct stm32_qspi_dev *qspi, uint32_t erase_cmd,
                     uint32_t sector_address, uint32_t size)
{
    QSPI_HandleTypeDef *hqspi = &qspi->data->hqspi;
    QSPI_CommandTypeDef cmd = COMMAND(erase_cmd);
    int rc;
    int sr = 0;

    stm32_qspi_lock(qspi);

    /*
     * For memory mapped device, first disable interrupts so no code will try
     * to access region that is not accessible for a while.
     */
    if (qspi->data->memory_mapped) {
        OS_ENTER_CRITICAL(sr);
        HAL_QSPI_Abort(&qspi->data->hqspi);
    }

    stm32_qspi_wait_for_ready(qspi);

    stm32_qspi_write_enable(qspi);

    cmd.Address = sector_address;
    rc = HAL_QSPI_Command(hqspi, &cmd, 1000);

    qspi->data->ready = false;

    /* Restore memory mapped mode if was enabled */
    if (qspi->data->memory_mapped) {
        stm32_qspi_wait_for_ready(qspi);
#if MYNEWT_VAL_STM32_ENABLE_DCACHE
        SCB_InvalidateDCache_by_Addr((void *)sector_address, size);
#endif
        stm32_qspi_memory_map(qspi);
        OS_EXIT_CRITICAL(sr);
    }

    stm32_qspi_unlock(qspi);

    return rc == HAL_OK ? 0 : -1;
}

static int
stm32_qspi_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    struct stm32_qspi_dev *qspi = (struct stm32_qspi_dev *)dev;

    return stm32_qspi_erase_cmd(qspi, CMD_20h, sector_address, QSPI_SECTOR_SIZE);
}

static int
stm32_qspi_chip_erase(struct stm32_qspi_dev *qspi)
{
    QSPI_HandleTypeDef *hqspi = &qspi->data->hqspi;
    QSPI_CommandTypeDef cmd = COMMAND(CMD_C7h);
    int rc;

    stm32_qspi_lock(qspi);

    stm32_qspi_wait_for_ready(qspi);

    stm32_qspi_write_enable(qspi);

    rc = HAL_QSPI_Command(hqspi, &cmd, 1000);

    qspi->data->ready = false;

    stm32_qspi_unlock(qspi);

    return rc == HAL_OK ? 0 : -1;
}

static int
stm32_qspi_erase(const struct hal_flash *dev, uint32_t address, uint32_t size)
{
    struct stm32_qspi_dev *qspi = (struct stm32_qspi_dev *)dev;
    uint32_t end;

    address &= ~0xFFFU;
    end = address + size;

    if (address == qspi->hal_flash.hf_base_addr && end == qspi->limit) {
        return stm32_qspi_chip_erase(qspi);
    }
    address -= qspi->hal_flash.hf_base_addr;

    while (size) {
        if ((address & 0xFFFFU) == 0 && (size >= 0x10000) && QSPI_ERASE64K != 0) {
            /* 64 KB erase if possible */
            stm32_qspi_erase_cmd(qspi, QSPI_ERASE64K, address, 0x10000);
            address += 0x10000;
            size -= 0x10000;
        } else {
            stm32_qspi_erase_cmd(qspi, CMD_20h, address, QSPI_SECTOR_SIZE);
            address += 0x1000;
            if (size > 0x1000) {
                size -= 0x1000;
            } else {
                size = 0;
            }
        }
    }

    return 0;
}

static int
stm32_qspi_sector_info(const struct hal_flash *dev, int idx, uint32_t *address,
                       uint32_t *sz)
{
    *address = dev->hf_base_addr + idx * QSPI_SECTOR_SIZE;
    *sz = QSPI_SECTOR_SIZE;

    return 0;
}

#ifdef ARM_MPU_ARMV7_H

/* Configure MPU so QSPI region is cachable and executable */
MPU_ARM_V7_REGION(qspi_region, 0x90000000,
                  ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 1, 0, 1, 1, 0,
                               ARM_MPU_REGION_SIZE_16MB));

#endif

static int
stm32_qspi_init(const struct hal_flash *dev)
{
    uint8_t buf[32];
    struct stm32_qspi_dev *qspi = (struct stm32_qspi_dev *)dev;
    QSPI_HandleTypeDef *hqspi = &qspi->data->hqspi;
    uint32_t ratio = (HAL_RCC_GetHCLKFreq() + MYNEWT_VAL_STM32_QSPI_FLASH_FREQ - 1) /
                     MYNEWT_VAL_STM32_QSPI_FLASH_FREQ;

    qspi->data->hqspi.Init.ClockPrescaler = ratio > 0 ? ratio - 1 : 0;

    __HAL_RCC_QSPI_CLK_ENABLE();
    __HAL_RCC_QSPI_FORCE_RESET();
    __HAL_RCC_QSPI_RELEASE_RESET();
    hal_gpio_init_fun(MYNEWT_VAL_STM32_QSPI_FLASH_PIN_CS);
    hal_gpio_init_fun(MYNEWT_VAL_STM32_QSPI_FLASH_PIN_SCK);
    hal_gpio_init_fun(MYNEWT_VAL_STM32_QSPI_FLASH_PIN_DIO0);
    hal_gpio_init_fun(MYNEWT_VAL_STM32_QSPI_FLASH_PIN_DIO1);
    hal_gpio_init_fun(MYNEWT_VAL_STM32_QSPI_FLASH_PIN_DIO2);
    hal_gpio_init_fun(MYNEWT_VAL_STM32_QSPI_FLASH_PIN_DIO3);
    HAL_QSPI_Init(hqspi);

    stm32_qspi_read_jedec_id(qspi, buf);
    /* Enable QUAD SPI mode in flash if we are going to use CMD_EBh */
    if (QSPI_READ == CMD_EBh) {
        stm32_qspi_quad_enable(qspi);
    }

    if (MYNEWT_VAL_STM32_QSPI_FLASH_MEMORY_MAPPED) {
        stm32_qspi_memory_map(qspi);
    }

    return 0;
}
