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

#define AHB_BUFFER_SIZE        MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE)
#define QSPI_HAS_CLR           FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC
#define qspi_tx_buffer_full(x) (QSPI_GetStatusFlags(x) & kQSPI_TxBufferFull)
#define qspi_in_use(x)         (QSPI_GetStatusFlags(x) & (kQSPI_Busy | kQSPI_IPAccess))
#define qspi_is_busy(x)        (QSPI_GetStatusFlags(x) & kQSPI_Busy)

#define SZ32K (32 * 1024)
#define SZ64K (64 * 1024)

#define LUT_CMD_READ                   0
#define LUT_CMD_WRITE_ENABLE           4
#define LUT_CMD_PAGE_PROGRAM           8
#define LUT_CMD_READ_STATUS            12
#define LUT_CMD_WRITE_STATUS           16
#define LUT_CMD_ERASE_SECTOR           24
#define LUT_CMD_ERASE_BLOCK32K         28
#define LUT_CMD_ERASE_BLOCK64K         32
#define LUT_CMD_ERASE_CHIP             36

#define MX25U3235F_CMD_WRSR            0x01
#define MX25U3235F_CMD_PP              0x02
#define MX25U3235F_CMD_RDSR            0x05
#define MX25U3235F_CMD_WREN            0x06
#define MX25U3235F_CMD_SE              0x20
#define MX25U3235F_CMD_BE32K           0x52
#define MX25U3235F_CMD_CE              0x60
#define MX25U3235F_CMD_BE              0xD8
#define MX25U3235F_CMD_4READ           0xEB

static struct os_mutex g_mtx;

uint32_t MX25U3235F_LUT[FSL_FEATURE_QSPI_LUT_DEPTH] = {

    [LUT_CMD_READ + 0] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_4READ, QSPI_ADDR, QSPI_PAD_4, 24),
    [LUT_CMD_READ + 1] = QSPI_LUT_SEQ(QSPI_DUMMY, QSPI_PAD_4, 6, QSPI_READ, QSPI_PAD_4, 128),
    [LUT_CMD_READ + 2] = QSPI_LUT_SEQ(QSPI_JMP_ON_CS, QSPI_PAD_1, 0, 0, 0, 0),

    [LUT_CMD_WRITE_ENABLE] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_WREN, 0, 0, 0),

    [LUT_CMD_PAGE_PROGRAM + 0] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_PP, QSPI_ADDR, QSPI_PAD_1, 24),
    [LUT_CMD_PAGE_PROGRAM + 1] = QSPI_LUT_SEQ(QSPI_WRITE, QSPI_PAD_1, 128, 0, 0, 0),

    [LUT_CMD_READ_STATUS] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_RDSR, QSPI_READ, QSPI_PAD_1, 1),

    [LUT_CMD_WRITE_STATUS] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_WRSR, QSPI_WRITE, QSPI_PAD_1, 1),

    [LUT_CMD_ERASE_SECTOR] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_SE, QSPI_ADDR, QSPI_PAD_1, 24),

    [LUT_CMD_ERASE_BLOCK32K] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_BE32K, QSPI_ADDR, QSPI_PAD_1, 24),

    [LUT_CMD_ERASE_BLOCK64K] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_BE, QSPI_ADDR, QSPI_PAD_1, 24),

    [LUT_CMD_ERASE_CHIP] = QSPI_LUT_SEQ(QSPI_CMD, QSPI_PAD_1, MX25U3235F_CMD_CE, 0, 0, 0),

};

#if MYNEWT_VAL(QSPIB_ENABLE) && !(FSL_FEATURE_QSPI_SUPPORT_PARALLEL_MODE)
#error "This device has no parallel mode support (please disable QSPIB)"
#endif

/*
 * XXX: This driver currently has the following limitations:
 *      * QSPIA and QSPIB must use a QSPI flash of the same size (and model).
 *      * Flashes with dual-die package are not supported.
 */

static qspi_flash_config_t g_qspi_flash_cfg = {
#if MYNEWT_VAL(QSPIA_ENABLE)
    .flashA1Size = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE),
#else
    .flashA1Size = 0,
#endif
    .flashA2Size = 0,
#if (FSL_FEATURE_QSPI_SUPPORT_PARALLEL_MODE)
#if MYNEWT_VAL(QSPIB_ENABLE)
    .flashB1Size = MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE),
#else
    .flashB1Size = 0,
#endif
    .flashB2Size = 0,
#endif /* (FSL_FEATURE_QSPI_SUPPORT_PARALLEL_MODE) */
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
wait_until_finished(void)
{
    uint32_t val;

    do {
        while (qspi_is_busy(QuadSPI0));
        QSPI_ClearFifo(QuadSPI0, kQSPI_RxFifo);
        QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_READ_STATUS);
        while (qspi_is_busy(QuadSPI0));
        /* val & 1 == WIP */
        val = QuadSPI0->RBDR[0];
        /* Clear ARDB area */
        QSPI_ClearErrorFlag(QuadSPI0, kQSPI_RxBufferDrain);
    } while (val & 1);
}

static void
cmd_write_enable()
{
    QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_WRITE_ENABLE);
}

static int
nxp_qspi_read(const struct hal_flash *dev,
              uint32_t address,
              void *dst,
              uint32_t num_bytes)
{
    uint32_t amount;

    while (qspi_in_use(QuadSPI0));

    os_mutex_pend(&g_mtx, OS_TIMEOUT_NEVER);

    /*
     * The RM mentions that a read using AHB can never be bigger than the
     * configured buffer size, but it is not clear if a buffer reset is
     * required; it seems to work without it.
     */
    while (num_bytes) {
        amount = MIN(num_bytes, AHB_BUFFER_SIZE);
        memcpy(dst, (void *)(FSL_FEATURE_QSPI_AMBA_BASE + address), amount);
        dst = (uint8_t *)dst + amount;
        address += amount;
        num_bytes -= amount;
    }

    os_mutex_release(&g_mtx);
    return 0;
}

static int
nxp_qspi_write(const struct hal_flash *dev,
               uint32_t address,
               const void *src,
               uint32_t num_bytes)
{
    uint8_t align;
    uint32_t page_size;
    uint32_t size;
    uint8_t pad[4];
    uint8_t pad_amount;
    uint8_t pad_total;
    int32_t remaining;
    uint32_t *src32;
    int i;
    os_sr_t sr;

    align = dev->hf_align;
    if (address % align) {
        return OS_EINVAL;
    }

    os_mutex_pend(&g_mtx, OS_TIMEOUT_NEVER);

    page_size = MYNEWT_VAL(QSPI_FLASH_PAGE_SIZE);
    while (num_bytes) {
        /*
         * Each iteration of this loop should write to one page in flash
         */
        size = MIN(num_bytes, page_size - (address % page_size));

        while (qspi_is_busy(QuadSPI0));
        QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);

        QSPI_SetIPCommandAddress(QuadSPI0, FSL_FEATURE_QSPI_AMBA_BASE + address);
        cmd_write_enable();
        while (qspi_is_busy(QuadSPI0));

        /*
         * Before starting a write run the TXFIFO must have at least 4 longwords
         * to prevent from underrun, if not enough data is available just pad with
         * 0xff
         */
        pad_total = 0;
        remaining = size;
        for (i = 0; i < 4 && !qspi_tx_buffer_full(QuadSPI0); i++) {
            pad_amount = MIN(remaining, 4);
            memcpy(&pad[0], src, pad_amount);
            memset(&pad[size], 0xff, sizeof(pad) - pad_amount);

            QSPI_WriteData(QuadSPI0, *((uint32_t *)pad));

            src += pad_amount;
            remaining -= pad_amount;
            pad_total += pad_amount;
        }

        QSPI_SetIPCommandSize(QuadSPI0, size);
        QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_PAGE_PROGRAM);

        remaining = size - pad_total;
        src32 = (uint32_t *)src;
        while (remaining > 0) {
            if (qspi_tx_buffer_full(QuadSPI0)) {
                continue;
            }
            QSPI_WriteData(QuadSPI0, *src32++);
            remaining -= 4;
        }

        wait_until_finished();
        while (qspi_in_use(QuadSPI0));

#if QSPI_HAS_CLR
        QSPI_ClearCache(QuadSPI0);
#endif

        src = (uint8_t *)src32;
        address += size;
        num_bytes -= size;
    }

    while (qspi_in_use(QuadSPI0));
    OS_ENTER_CRITICAL(sr);
    QSPI_SoftwareReset(QuadSPI0);
    OS_EXIT_CRITICAL(sr);

    os_mutex_release(&g_mtx);

    return 0;
}

static int
nxp_qspi_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    os_sr_t sr;

    while (qspi_is_busy(QuadSPI0));

    os_mutex_pend(&g_mtx, OS_TIMEOUT_NEVER);

    QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);
    QSPI_SetIPCommandAddress(QuadSPI0, FSL_FEATURE_QSPI_AMBA_BASE + sector_address);
    cmd_write_enable();
    QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_ERASE_SECTOR);
    wait_until_finished();
    while (qspi_in_use(QuadSPI0));

#if QSPI_HAS_CLR
    QSPI_ClearCache(QuadSPI0);
#endif

    OS_ENTER_CRITICAL(sr);
    QSPI_SoftwareReset(QuadSPI0);
    OS_EXIT_CRITICAL(sr);

    os_mutex_release(&g_mtx);

    return 0;
}

void
nxp_qspi_erase_chip(void)
{
    os_sr_t sr;

    int chips = (MYNEWT_VAL(QSPIA_ENABLE) ? 1 : 0) +
                (MYNEWT_VAL(QSPIB_ENABLE) ? 1 : 0);
    uint32_t address;

    while (qspi_in_use(QuadSPI0));

    os_mutex_pend(&g_mtx, OS_TIMEOUT_NEVER);

    address = FSL_FEATURE_QSPI_AMBA_BASE;
    while (chips > 0) {
        QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);
        QSPI_SetIPCommandAddress(QuadSPI0, address);
        cmd_write_enable();
        QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_ERASE_CHIP);
        wait_until_finished();
        while (qspi_in_use(QuadSPI0));

#if QSPI_HAS_CLR
        QSPI_ClearCache(QuadSPI0);
#endif

        OS_ENTER_CRITICAL(sr);
        QSPI_SoftwareReset(QuadSPI0);
        OS_EXIT_CRITICAL(sr);

        address += MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) *
                   MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT);
        chips--;
    }

    os_mutex_release(&g_mtx);
}

static int
nxp_qspi_erase(const struct hal_flash *dev,
               uint32_t address,
               uint32_t size)
{
    uint32_t erased_size;
    os_sr_t sr;

    if (address % MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) ||
        size % MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE)) {
        return -1;
    }

    os_mutex_pend(&g_mtx, OS_TIMEOUT_NEVER);

    while (size) {
        while (qspi_is_busy(QuadSPI0));
        QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);
        QSPI_SetIPCommandAddress(QuadSPI0, FSL_FEATURE_QSPI_AMBA_BASE + address);
        cmd_write_enable();
        if (size >= SZ64K && (address % SZ64K) == 0) {
            QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_ERASE_BLOCK64K);
            erased_size = SZ64K;
        } else if (size >= SZ32K && (address % SZ32K) == 0) {
            QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_ERASE_BLOCK32K);
            erased_size = SZ32K;
        } else {
            QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_ERASE_SECTOR);
            erased_size = MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE);
        }
        wait_until_finished();
        while (qspi_in_use(QuadSPI0));

#if QSPI_HAS_CLR
        QSPI_ClearCache(QuadSPI0);
#endif

        address += erased_size;
        size -= erased_size;
    }

    OS_ENTER_CRITICAL(sr);
    QSPI_SoftwareReset(QuadSPI0);
    OS_EXIT_CRITICAL(sr);

    os_mutex_release(&g_mtx);

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

static void
enable_quad_mode(void)
{
    int chips = (MYNEWT_VAL(QSPIA_ENABLE) ? 1 : 0) +
                (MYNEWT_VAL(QSPIB_ENABLE) ? 1 : 0);
    uint32_t address;

    address = FSL_FEATURE_QSPI_AMBA_BASE;
    while (chips > 0) {
        QSPI_SetIPCommandAddress(QuadSPI0, address);

        QSPI_ClearFifo(QuadSPI0, kQSPI_TxFifo);

        cmd_write_enable();

        /*
         * Set QE bit to enable quad mode.
         *
         * XXX need extra writes to fill the FIFO
         */
        QSPI_WriteData(QuadSPI0, 0xffffff40);
        QSPI_WriteData(QuadSPI0, 0xffffffff);
        QSPI_WriteData(QuadSPI0, 0xffffffff);
        QSPI_WriteData(QuadSPI0, 0xffffffff);

        QSPI_ExecuteIPCommand(QuadSPI0, LUT_CMD_WRITE_STATUS);

        wait_until_finished();
        while (qspi_in_use(QuadSPI0));

        address += MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE) *
                   MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT);
        chips--;
    }
}

static int
nxp_qspi_init(const struct hal_flash *dev)
{
    qspi_config_t qspi_cfg = {0};

#if MYNEWT_VAL(QSPIA_ENABLE)
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPIA_PIN_SCK), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPIA_PIN_SS), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPIA_PIN_DIO0), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPIA_PIN_DIO1), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPIA_PIN_DIO2), MYNEWT_VAL(QSPIA_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIA_PORT), MYNEWT_VAL(QSPIA_PIN_DIO3), MYNEWT_VAL(QSPIA_MUX));
#endif

#if MYNEWT_VAL(QSPIB_ENABLE)
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPIB_PIN_SCK), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPIB_PIN_SS), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPIB_PIN_DIO0), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPIB_PIN_DIO1), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPIB_PIN_DIO2), MYNEWT_VAL(QSPIB_MUX));
    PORT_SetPinMux(MYNEWT_VAL(QSPIB_PORT), MYNEWT_VAL(QSPIB_PIN_DIO3), MYNEWT_VAL(QSPIB_MUX));
#endif

    QSPI_GetDefaultQspiConfig(&qspi_cfg);
    qspi_cfg.baudRate = MYNEWT_VAL(QSPI_SCK_FREQ);

    /* Set AHB buffer size for reading data through AHB bus */
    qspi_cfg.AHBbufferSize[3] = AHB_BUFFER_SIZE;

    QSPI_Init(QuadSPI0, &qspi_cfg, CLOCK_GetFreq(kCLOCK_McgPll0Clk));

    memcpy(g_qspi_flash_cfg.lookuptable, MX25U3235F_LUT, sizeof(MX25U3235F_LUT));
    QSPI_SetFlashConfig(QuadSPI0, &g_qspi_flash_cfg);

    os_mutex_init(&g_mtx);

#if QSPI_HAS_CLR
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
    .hff_erase = nxp_qspi_erase,
};

const struct hal_flash nxp_qspi_dev = {
    .hf_itf = &nxp_qspi_funcs,
    .hf_base_addr = 0,
    .hf_size =
#if MYNEWT_VAL(QSPIA_ENABLE) && MYNEWT_VAL(QSPIB_ENABLE)
        2 *
#endif
        MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT) * MYNEWT_VAL(QSPI_FLASH_SECTOR_SIZE),
    .hf_sector_cnt =
#if MYNEWT_VAL(QSPIA_ENABLE) && MYNEWT_VAL(QSPIB_ENABLE)
        2 *
#endif
        MYNEWT_VAL(QSPI_FLASH_SECTOR_COUNT),
    .hf_align = MYNEWT_VAL(QSPI_FLASH_MIN_WRITE_SIZE),
    .hf_erased_val = 0xff,
};

#endif /* MYNEWT_VAL(QSPI_ENABLE) */
