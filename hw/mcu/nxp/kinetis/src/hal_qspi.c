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

#include <fsl_port.h>
#include <fsl_qspi.h>
#include <fsl_clock.h>

#if MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) < 1
#error QSPI_FLASH_SECTOR_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) < 1
#error QSPI_FLASH_SECTOR_COUNT must be set to the correct value in bsp syscfg.yml
#endif

uint32_t lut[FSL_FEATURE_QSPI_LUT_DEPTH] = {
    /* Seq0 :Quad Read */
    /* CMD:        0xEB - Quad Read, Single pad */
    /* ADDR:       0x18 - 24bit address, Quad pads */
    /* DUMMY:      0x06 - 6 clock cyles, Quad pads */
    /* READ:       0x80 - Read 128 bytes, Quad pads */
    /* JUMP_ON_CS: 0 */
    [0] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, 0xEB, QSPI_ADDR, QSPI_PAD_4, 0x18),
    [1] = QSPI_LUT_SEQ(QSPI_DUMMY, QSPI_PAD_4, 0x06, QSPI_READ, QSPI_PAD_4, 0x80),
    [2] = QSPI_LUT_SEQ(QSPI_JMP_ON_CS, QSPI_PAD_1, 0x0, 0, 0, 0),

    /* Seq1: Write Enable */
    /* CMD:      0x06 - Write Enable, Single pad */
    [4] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, 0x06, 0, 0, 0),

    /* Seq2: Erase All */
    /* CMD:    0x60 - Erase All chip, Single pad */
    [8] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, 0x60, 0, 0, 0),

    /* Seq3: Read Status */
    /* CMD:    0x05 - Read Status, single pad */
    /* READ:   0x01 - Read 1 byte */
    [12] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, 0x05, QSPI_READ, QSPI_PAD_1, 0x1),

    /* Seq4: Page Program */
    /* CMD:    0x02 - Page Program, Single pad */
    /* ADDR:   0x18 - 24bit address, Single pad */
    /* WRITE:  0x80 - Write 128 bytes at one pass, Single pad */
    [16] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, 0x02, QSPI_ADDR, QSPI_PAD_1, 0x18),
    [17] = QSPI_LUT_SEQ(QSPI_WRITE, QSPI_PAD_1, 0x80, 0, 0, 0),

    /* Seq5: Write Register */
    /* CMD:    0x01 - Write Status Register, single pad */
    /* WRITE:  0x01 - Write 1 byte of data, single pad */
    [20] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, 0x01, QSPI_WRITE, QSPI_PAD_1, 0x1),

    /* Seq6: Read Config Register */
    /* CMD:  0x15 - Read Config register, single pad */
    /* READ: 0x01 - Read 1 byte */
    [24] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, 0x15, QSPI_READ, QSPI_PAD_1, 0x1),

    /* Seq7: Erase Sector */
    /* CMD:  0x20 - Sector Erase, single pad */
    /* ADDR: 0x18 - 24 bit address, single pad */
    [28] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, 0x20, QSPI_ADDR, QSPI_PAD_1, 0x18),

    /* Match MISRA rule */
    [63] = 0
};

qspi_flash_config_t g_qspi_flash_cfg = {
    .flashA1Size = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) / 2,
    .flashA2Size = 0,
#if (FSL_FEATURE_QSPI_SUPPORT_PARALLEL_MODE)
    .flashB1Size = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) / 2,
    .flashB2Size = 0,
#endif
#if (!FSL_FEATURE_QSPI_HAS_NO_TDH)
    .dataHoldTime = 0,
#endif
    .CSHoldTime = 0,
    .CSSetupTime = 0,
    .cloumnspace = 0,
    .dataLearnValue = 0,
    .endian = kQSPI_64LittleEndian,
    .enableWordAddress = false
};

static void
check_if_finished(void)
{
    uint32_t val = 0;
    /* Check WIP bit */
    do {
        while (QSPI_GetStatusFlags(QuadSPI0) & kQSPI_Busy);
        QSPI_ClearFifo(QuadSPI0, kQSPI_RxFifo);
        QSPI_ExecuteIPCommand(QuadSPI0, 12U);
        while (QSPI_GetStatusFlags(QuadSPI0) & kQSPI_Busy);
        val = QuadSPI0->RBDR[0];
        /* Clear ARDB area */
        QSPI_ClearErrorFlag(QuadSPI0, kQSPI_RxBufferDrain);
    } while (val & 0x1);
}

static void
cmd_write_enable()
{
    while (QSPI_GetStatusFlags(QuadSPI0) & kQSPI_Busy);
    QSPI_ExecuteIPCommand(QuadSPI0, 4U);
}

static void
read_page(uint32_t address, uint32_t *dst)
{
    int i;
    for (i = 0; i < MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE) / 4; i++) {
        dst[i] = ((uint32_t *)address)[i];
    }
}

static int
nxp_qspi_read(const struct hal_flash *dev,
              uint32_t address,
              void *dst,
              uint32_t num_bytes)
{
    uint32_t npages;
    if ((address % dev->hf_align) != 0) {
        return OS_EINVAL;
    }
    if ((num_bytes % MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE)) != 0) {
        return OS_EINVAL;
    }

    npages = num_bytes / MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE);
    while (npages) {
        read_page(address,  dst);
        npages--;
        address += MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE);
    }
    return 0;
}

static void
write_page(uint32_t dest_addr, uint32_t *src_addr)
{
    uint32_t leftLongWords = 0;

    while (QSPI_GetStatusFlags(QuadSPI0) & kQSPI_Busy);
    QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);

    QSPI_SetIPCommandAddress(QuadSPI0, dest_addr);
    cmd_write_enable();
    while (QSPI_GetStatusFlags(QuadSPI0) & kQSPI_Busy);

    /* First write some data into TXFIFO to prevent from underrun */
    QSPI_WriteBlocking(QuadSPI0, src_addr, FSL_FEATURE_QSPI_TXFIFO_DEPTH * 4);
    src_addr += FSL_FEATURE_QSPI_TXFIFO_DEPTH;

    /* Start the program */
    QSPI_SetIPCommandSize(QuadSPI0, MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE));
    QSPI_ExecuteIPCommand(QuadSPI0, 16U);

    leftLongWords = MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE) - 16 * sizeof(uint32_t);
    QSPI_WriteBlocking(QuadSPI0, src_addr, leftLongWords);

    /* Wait until flash finished program */
    check_if_finished();
    while (QSPI_GetStatusFlags(QuadSPI0) & (kQSPI_Busy | kQSPI_IPAccess));

#if defined(FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC) && (FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC)
    QSPI_ClearCache(QuadSPI0);
#endif
}

static int
nxp_qspi_write(const struct hal_flash *dev,
               uint32_t address,
               const void *src,
               uint32_t num_bytes)
{
    uint32_t npages;
    if ((address % dev->hf_align) != 0) {
        return OS_EINVAL;
    }
    if ((num_bytes % MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE)) != 0) {
        return OS_EINVAL;
    }

    npages = num_bytes / MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE);
    while (npages) {
        write_page(address, (uint32_t *) src);
        npages--;
        address += MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE);
    }
    return 0;
}

static int
nxp_qspi_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    sector_address = sector_address / MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    sector_address *= MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);

    while (QSPI_GetStatusFlags(QuadSPI0) & kQSPI_Busy);

    QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);
    QSPI_SetIPCommandAddress(QuadSPI0, sector_address);
    cmd_write_enable();
    QSPI_ExecuteIPCommand(QuadSPI0, 28U);
    check_if_finished();

#if defined(FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC) && (FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC)
    QSPI_ClearCache(QuadSPI0);
#endif
    return 0;
}

static int
nxp_qspi_erase(const struct hal_flash *dev,
               uint32_t address,
               uint32_t size)
{
    uint32_t nsects;

    address = address / MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    address *= MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    nsects = size / MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    nsects += size % MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);

    while (nsects) {
        while (QSPI_GetStatusFlags(QuadSPI0) & kQSPI_Busy);
        QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);
        QSPI_SetIPCommandAddress(QuadSPI0, address);
        cmd_write_enable();
        QSPI_ExecuteIPCommand(QuadSPI0, 28U);
        check_if_finished();

#if defined(FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC) &&  \
        (FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC)
        QSPI_ClearCache(QuadSPI0);
#endif
        nsects--;
        address += MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    }
    return 0;
}

static int
nxp_qspi_sector_info(const struct hal_flash *dev,
                     int idx,
                     uint32_t *address,
                     uint32_t *sz)
{
    *address = idx * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
    *sz = MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);

    return 0;
}

/* Enable Quad mode */
static void
enable_quad_mode(void)
{
    uint32_t val[4] = {0x40U, 0, 0, 0};

    while (QSPI_GetStatusFlags(QuadSPI0) & kQSPI_Busy);
    QSPI_SetIPCommandAddress(QuadSPI0, FSL_FEATURE_QSPI_AMBA_BASE);

    /* Clear Tx FIFO */
    QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);

    /* Write enable */
    cmd_write_enable();

    /* Write data into TX FIFO, needs to write at least 16 bytes of data */
    QSPI_WriteBlocking(QuadSPI0, val, 16U);

    /* Set seq id, write register */
    QSPI_ExecuteIPCommand(QuadSPI0, 20);

    /* Wait until finished */
    check_if_finished();
}

static int
nxp_qspi_init(const struct hal_flash *dev)
{
    qspi_config_t qspi_cfg = {0};

    /*Get QSPI default settings and configure the qspi */
    QSPI_GetDefaultQspiConfig(&qspi_cfg);

    /*Set AHB buffer size for reading data through AHB bus */
    qspi_cfg.AHBbufferSize[3] = MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE);

    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPI_PIN_SCKA), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPI_PIN_SSA), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPI_PIN_DIOA0), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPI_PIN_DIOA1), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPI_PIN_DIOA2), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPI_PIN_DIO3A), MYNEWT_VAL(QSPIA_MUX));

    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPI_PIN_SCKB), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPI_PIN_SSB), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPI_PIN_DIOB0), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPI_PIN_DIOB1), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPI_PIN_DIOB2), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPI_PIN_DIOBA), MYNEWT_VAL(QSPIB_MUX));

    qspi_cfg.baudRate = MYNEWT_VAL(QSPI_SCK_FREQ);
    QSPI_Init(QuadSPI0, &qspi_cfg, CLOCK_GetFreq(kCLOCK_McgPll0Clk));

    memcpy(g_qspi_flash_cfg.lookuptable, lut, sizeof(lut));
    QSPI_SetFlashConfig(QuadSPI0, &g_qspi_flash_cfg);

#if FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC
    QSPI_ClearCache(QuadSPI0);
#endif

    enable_quad_mode();
    return 0;
}

static const struct hal_flash_funcs nxp_qspi_funcs = {
    .hff_read = nxp_qspi_read,
    .hff_write = nxp_qspi_write,
    .hff_erase_sector = nxp_qspi_erase_sector,
    .hff_sector_info = nxp_qspi_sector_info,
    .hff_init = nxp_qspi_init,
    .hff_erase = nxp_qspi_erase
};

const struct hal_flash nxp_qspi_dev = {
    .hf_itf = &nxp_qspi_funcs,
    .hf_base_addr = FSL_FEATURE_QSPI_AMBA_BASE,
    .hf_size = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE),
    .hf_sector_cnt = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT),
    .hf_align = 8,
    .hf_erased_val = 0xff,
};

#endif
