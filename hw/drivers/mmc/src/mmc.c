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
#include <bsp/bsp.h>
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include <disk/disk.h>
#include <mmc/mmc.h>
#include <stdio.h>
#include <bit_set/bit_set.h>

#define MIN(n, m) (((n) < (m)) ? (n) : (m))

typedef enum {
    FORMAT_R1,
    FORMAT_R1B,
    FORMAT_R2,
    FORMAT_R3,
    FORMAT_R4,
    FORMAT_R5,
    FORMAT_R6,
    FORMAT_R7,
} reponse_format_t;

typedef uint8_t sd_cmd_t;

/* MMC commands used by the driver */
#define CMD0                (0)            /* GO_IDLE_STATE */
#define CMD1                (1)            /* SEND_OP_COND (MMC) */
#define CMD6                (8)            /* SEND_IF_COND */
#define CMD8                (8)            /* SEND_IF_COND */
#define CMD9                (9)            /* SEND_CSD */
#define CMD10               (10)           /* SEND_CID */
#define CMD12               (12)           /* STOP_TRANSMISSION */
#define CMD16               (16)           /* SET_BLOCKLEN */
#define CMD17               (17)           /* READ_SINGLE_BLOCK */
#define CMD18               (18)           /* READ_MULTIPLE_BLOCK */
#define CMD24               (24)           /* WRITE_BLOCK */
#define CMD25               (25)           /* WRITE_MULTIPLE_BLOCK */
#define CMD55               (55)           /* APP_CMD */
#define CMD58               (58)           /* READ_OCR */

#define ACMD13              (13)           /*  */
#define ACMD41              (41)           /* SEND_OP_COND (SDC) */
#define ACMD51              (51)           /* SEND_SCR */

#define HCS                 ((uint32_t) 1 << 30)

/* Response types */
#define R1                  (0)
#define R1b                 (1)
#define R2                  (2)
#define R3                  (3)            /* CMD58 */
#define R7                  (4)            /* CMD8 */

/* R1 response status */
#define R_IDLE              (0x01)
#define R_ERASE_RESET       (0x02)
#define R_ILLEGAL_COMMAND   (0x04)
#define R_CRC_ERROR         (0x08)
#define R_ERASE_ERROR       (0x10)
#define R_ADDR_ERROR        (0x20)
#define R_PARAM_ERROR       (0x40)

#define CMD_OK(status)      (((status) & 0x7E) == 0)
#define CMD_FAILED(status)  (((status) & 0x7E) != 0)

#define START_BLOCK         (0xFE)
#define START_BLOCK_TOKEN   (0xFC)
#define STOP_TRAN_TOKEN     (0xFD)

#define BLOCK_LEN           (512)

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

static struct mmc_spi_cfg mmc0_config = {
    .initial_freq_khz = 500,
    .freq_khz = MYNEWT_VAL_MMC_SPI_FREQ,
    .clock_mode = MYNEWT_VAL_MMC_SPI_CLOCK_MODE,
    .bus_name = MYNEWT_VAL_MMC_SPI_BUS_NAME,
    .pin_cs = MYNEWT_VAL_MMC_SPI_CS_PIN,
};

#endif

const char *
mmc_error_str(char *buf, uint8_t status)
{
    sprintf(buf, "0x%X", status);
    if (status == 0xff) {
        strcat(buf, " MMC_CARD_ERROR");
    } else if (status == 0x7F) {
        strcat(buf, " Unsupported");
    } else {
        if (status & R_IDLE) {
            strcat(buf, " R_IDLE");
        }
        if (status & R_ERASE_RESET) {
            strcat(buf, " R_ERASE_RESET");
        }
        if (status & R_ILLEGAL_COMMAND) {
            strcat(buf, " R_ILLEGAL_COMMAND");
        } else if (status & R_CRC_ERROR) {
            strcat(buf, " R_CRC_ERROR");
        } else if (status & R_ERASE_ERROR) {
            strcat(buf, " R_ERASE_ERROR");
        } else if (status & R_ADDR_ERROR) {
            strcat(buf, " R_ADDR_ERROR");
        } else if (status & R_PARAM_ERROR) {
            strcat(buf, " R_PARAM_ERROR");
        }
    }
    return buf;
}

static int
error_by_response(uint8_t status)
{
    int rc = MMC_CARD_ERROR;

    if (status == 0) {
        rc = MMC_OK;
    } else if (status == 0xff) {
        rc = MMC_CARD_ERROR;
    } else if (status & R_IDLE) {
        rc = MMC_TIMEOUT;
    } else if (status & R_ERASE_RESET) {
        rc = MMC_ERASE_ERROR;
    } else if (status & R_ILLEGAL_COMMAND) {
        rc = MMC_INVALID_COMMAND;
    } else if (status & R_CRC_ERROR) {
        rc = MMC_CRC_ERROR;
    } else if (status & R_ERASE_ERROR) {
        rc = MMC_ERASE_ERROR;
    } else if (status & R_ADDR_ERROR) {
        rc = MMC_ADDR_ERROR;
    } else if (status & R_PARAM_ERROR) {
        rc = MMC_PARAM_ERROR;
    }

    return (rc);
}

static inline void
mmc_led_on(void)
{
    if (MYNEWT_VAL_MMC_LED_PIN >= 0) {
        hal_gpio_write(MYNEWT_VAL_MMC_LED_PIN, MYNEWT_VAL_MMC_LED_ON_VALUE);
    }
}

static inline void
mmc_led_off(void)
{
    if (MYNEWT_VAL_MMC_LED_PIN >= 0) {
        hal_gpio_write(MYNEWT_VAL_MMC_LED_PIN, !MYNEWT_VAL_MMC_LED_ON_VALUE);
    }
}

static void
mmc_spi_set_cs(mmc_disk_t *mmc, int cs)
{
    hal_gpio_write(mmc->spi.mmc_spi_cfg.pin_cs, cs);
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

static int
mmc_spi_tx(mmc_disk_t *mmc, const uint8_t *buf, uint16_t count)
{
    return bus_node_write(&mmc->spi.node.bnode.odev, buf, count, 1000, BUS_F_NOSTOP);
}

static int
mmc_spi_rx(mmc_disk_t *mmc, uint8_t *buf, uint16_t count)
{
    memset(buf, 0xFF, count);
    return bus_node_duplex_write_read(&mmc->spi.node.bnode.odev, buf, buf,
                                      count, 1000, BUS_F_NOSTOP);
}

#else

static int
mmc_spi_tx(mmc_disk_t *mmc, const uint8_t *buf, uint16_t count)
{
    mmc_led_on();

    for (int i = 0; i < count; i++) {
        hal_spi_tx_val(mmc->spi.spi_num, buf[i]);
    }

    mmc_led_off();
    return 0;
}

static int
mmc_spi_rx(mmc_disk_t *mmc, uint8_t *buf, uint16_t count)
{
    mmc_led_on();

    for (int i = 0; i < count; i++) {
        buf[i] = hal_spi_tx_val(mmc->spi.spi_num, 0xFF);
    }

    mmc_led_off();

    return 0;
}

#endif

static int
mmc_spi_tx_byte(mmc_disk_t *mmc, uint8_t b)
{
    return mmc_spi_tx(mmc, &b, 1);
}

static inline void
mmc_send_extra_clocks(mmc_disk_t *mmc)
{
    static const uint8_t extra_bytes[] = { 0xFF, 0xFF, 0xFF };

    if (MYNEWT_VAL_MMC_SEND_EXTRA_CLOCKS) {
        mmc_spi_tx(mmc, extra_bytes, sizeof(extra_bytes));
    }
}

static uint8_t
send_mmc_cmd(mmc_disk_t *mmc, sd_cmd_t cmd, uint32_t payload)
{
    int n;
    uint16_t status;
    uint8_t crc;
    uint8_t buf[6];
    char strbuf[30];

    mmc_led_on();

    /* 4.7.2: Command Format */
    buf[0] = 0x40 | cmd;
    buf[1] = (uint8_t)(payload >> 24);
    buf[2] = (uint8_t)(payload >> 16);
    buf[3] = (uint8_t)(payload >> 8);
    buf[4] = (uint8_t)(payload);

    /**
     * 7.2.2 Bus Transfer Protection
     *   NOTE: SD is in CRC off mode by default but CMD0 and CMD8 always
     *   require a valid CRC.
     *   NOTE: CRC can be turned on with CMD59 (CRC_ON_OFF).
     */
    crc = 0x01;
    switch (cmd) {
        case CMD0:
            crc = 0x95;
            break;
        case CMD8:
            crc = 0x87;
            break;
    default:
        break;
    }
    buf[5] = crc;
    mmc_spi_tx(mmc, buf, 6);

    for (n = 10; n > 0; n--) {
        mmc_spi_rx(mmc, buf, 1);
        status = buf[0];
        if (status != 0xFF) {
            if (status & 0x80) {
                mmc_spi_rx(mmc, buf, 1);
                status = status << 8 | buf[0];
                while (status & 0x8000) {
                    status <<= 1;
                }
                status >>= 8;
            }
            if (status != 0x7F) {
                break;
            }
        }
    }
    if (CMD_FAILED(status)) {
        MMC_LOG_ERROR("%sCMD%d status %s", mmc->app_cmd ? "A" : "", cmd,
                      mmc_error_str(strbuf, status));
    } else {
        MMC_LOG_DEBUG("%sCMD%d status %s", mmc->app_cmd ? "A" : "", cmd,
                      mmc_error_str(strbuf, status));
    }
    mmc->app_cmd = 0;

    mmc_led_off();

    return (uint8_t)status;
}

static uint8_t
mmc_cmd_rx(mmc_disk_t *mmc, uint8_t cmd, uint32_t payload, uint8_t response[],
           size_t response_size)
{
    uint8_t status;

    mmc_spi_set_cs(mmc, 0);
    status = send_mmc_cmd(mmc, cmd, payload);

    if (response_size && response != NULL && CMD_OK(status)) {
        mmc_spi_rx(mmc, response, response_size);
    }

    mmc_spi_set_cs(mmc, 1);

    mmc_send_extra_clocks(mmc);

    return status;
}

static uint8_t
mmc_cmd_r1(mmc_disk_t *mmc, uint8_t cmd, uint32_t payload)
{
    return mmc_cmd_rx(mmc, cmd, payload, NULL, 0);
}

static uint8_t
mmc_wait_for_data(mmc_disk_t *mmc, uint32_t timeout_ms)
{
    os_time_t timeout = os_time_get() + os_time_ms_to_ticks32(timeout_ms);
    uint8_t res;

    do {
        mmc_spi_rx(mmc, &res, 1);
        if (res != 0xFF) {
            break;
        }
    } while (OS_TIME_TICK_LT(os_time_get(), timeout));

    return res;
}

static uint8_t
mmc_cmd_receive_data(mmc_disk_t *mmc, uint8_t cmd, uint32_t payload,
                     uint8_t *buffer, int count)
{
    int status;
    uint8_t res;
    uint8_t crc[2];

    mmc_spi_set_cs(mmc, 0);

    status = send_mmc_cmd(mmc, cmd, payload);

    if (CMD_OK(status)) {
        res = mmc_wait_for_data(mmc, 20);
        if (res == START_BLOCK) {
            mmc_spi_rx(mmc, buffer, count);
        }
    }

    mmc_spi_rx(mmc, crc, 2);

    mmc_spi_set_cs(mmc, 1);

    mmc_send_extra_clocks(mmc);

    return status;
}

static uint8_t
mmc_acmd(mmc_disk_t *mmc, uint8_t cmd, uint32_t payload, uint8_t *response,
         size_t response_size)
{
    uint8_t status;

    status = mmc_cmd_r1(mmc, CMD55, 0);
    if (CMD_OK(status)) {
        mmc->app_cmd = 1;
        status = mmc_cmd_rx(mmc, cmd, payload, response, response_size);
    }

    return status;
}

static uint8_t
mmc_acmd_with_data(mmc_disk_t *mmc, uint8_t cmd, uint32_t payload,
                   uint8_t *buffer, size_t count)
{
    uint8_t status;

    status = mmc_cmd_r1(mmc, CMD55, 0);

    if (CMD_OK(status)) {
        mmc->app_cmd = 1;
        status = mmc_cmd_receive_data(mmc, cmd, payload, buffer, count);
    }

    return status;
}

#define SCR_SCR_STRUCTURE_POS       60
#define SCR_SCR_STRUCTURE_LEN       4
#define SCR_SD_SPEC_POS             56
#define SCR_SD_SPEC_LEN             4
#define SD_BUS_WIDTHS_POS           48
#define SD_BUS_WIDTHS_LEN           4
#define SCR_SD_SPEC3_POS            47
#define SCR_SD_SPEC3_LEN            1
#define SCR_CMD_SUPPORT_POS         32
#define SCR_CMD_SUPPORT_LEN         14

#define CSD_V1_CSD_STRUCTURE_POS    126
#define CSD_V1_CSD_STRUCTURE_LEN    2
#define CSD_V1_CSD_STRUCTURE_VER_1  0
#define CSD_V1_READ_BL_LEN_POS      80
#define CSD_V1_READ_BL_LEN_LEN      4
#define CSD_V1_C_SIZE_POS           62
#define CSD_V1_C_SIZE_LEN           12
#define CSD_V1_C_SIZE_MULT_POS      47
#define CSD_V1_C_SIZE_MULT_LEN      3

#define CSD_V2_CSD_STRUCTURE_POS    126
#define CSD_V2_CSD_STRUCTURE_LEN    2
#define CSD_V2_CSD_STRUCTURE_VER_2  1
#define CSD_V2_C_SIZE_POS           48
#define CSD_V2_C_SIZE_LEN           22

static uint32_t
csd_sector_count(const uint8_t csd[16])
{
    uint32_t ver = bit_set_get_uint32(csd, CSD_V1_CSD_STRUCTURE_POS,
                                      CSD_V1_CSD_STRUCTURE_LEN);
    uint32_t c_size;
    uint32_t c_size_mult;

    if (ver == CSD_V1_CSD_STRUCTURE_VER_1) {
        c_size = bit_set_get_uint32(csd, CSD_V1_C_SIZE_POS, CSD_V1_C_SIZE_LEN);
        c_size_mult = bit_set_get_uint32(csd, CSD_V1_C_SIZE_MULT_POS,
                                         CSD_V1_C_SIZE_MULT_LEN);
        return (c_size + 1) << (c_size_mult + 2);
    } else {
        c_size = bit_set_get_uint32(csd, CSD_V2_C_SIZE_POS, CSD_V2_C_SIZE_LEN);
        return (c_size + 1) << 10;
    }
}

int
mmc_read_csd(mmc_disk_t *mmc, uint8_t csd[16])
{
    uint8_t status;
    uint32_t ver;
    uint32_t c_size;

    status = mmc_cmd_receive_data(mmc, CMD9, 0, csd, 16);

    if (CMD_OK(status)) {
        bit_set_reverse(csd, 128);

        ver = bit_set_get_uint32(csd, CSD_V1_CSD_STRUCTURE_POS,
                                 CSD_V1_CSD_STRUCTURE_LEN);
        MMC_LOG_INFO("CSD ver %d", ver == CSD_V1_CSD_STRUCTURE_VER_1 ? 1 : 2);

        if (ver == CSD_V1_CSD_STRUCTURE_VER_1) {
            c_size = bit_set_get_uint32(csd, CSD_V1_C_SIZE_POS, CSD_V1_C_SIZE_LEN);
            MMC_LOG_DEBUG("CSD c_size %d c_size_mult %d read_bl_len %d)", (int)c_size,
                          (int)bit_set_get_uint32(csd, CSD_V1_C_SIZE_MULT_POS,
                                                  CSD_V1_C_SIZE_MULT_LEN),
                          (int)bit_set_get_uint32(csd, CSD_V1_READ_BL_LEN_POS,
                                                  CSD_V1_READ_BL_LEN_LEN));
        } else {
            c_size = bit_set_get_uint32(csd, CSD_V2_C_SIZE_POS, CSD_V2_C_SIZE_LEN);
            MMC_LOG_DEBUG("CSD c_size %d (%ul sectors %u MB)", (int)c_size,
                          csd_sector_count(csd), csd_sector_count(csd) >> 11);
        }
    }

    return status;
}

uint8_t
mmc_read_cid(mmc_disk_t *mmc, uint8_t cid[16])
{
    uint8_t status;

    status = mmc_cmd_receive_data(mmc, CMD10, 0, (uint8_t *)cid, 16);

    if (CMD_OK(status)) {
        MMC_LOG_INFO("CID MID 0x%x", cid[0]);
        MMC_LOG_INFO("CID OID %c%c", cid[1], cid[2]);
        MMC_LOG_INFO("CID PNM %c%c%c%c%c", cid[3], cid[4], cid[5], cid[6], cid[7]);
        MMC_LOG_INFO("CID PRV %c.%c", (cid[8] >> 4) + '0', (cid[8] & 15) + '0');
        MMC_LOG_INFO("CID MDT %d.%02d",
                     2000 + ((cid[13] & 15) << 4) + (cid[14] >> 4), (cid[14] & 15));
    }

    return status;
}

uint8_t
mmc_read_scr(mmc_disk_t *mmc, uint8_t scr[8])
{
    uint8_t status;

    status = mmc_acmd_with_data(mmc, ACMD51, 0, scr, 8);

    if (CMD_OK(status)) {
        bit_set_reverse(scr, 64);
        MMC_LOG_INFO("SCR structure %d",
                     (int)bit_set_get_uint32(scr, SCR_SCR_STRUCTURE_POS,
                                             SCR_SCR_STRUCTURE_LEN));
        MMC_LOG_INFO("SCR sd_spec %d",
                     (int)bit_set_get_uint32(scr, SCR_SD_SPEC_POS, SCR_SD_SPEC_LEN));
        MMC_LOG_INFO("SCR sd_bus_width 0x%x",
                     (int)bit_set_get_uint32(scr, SD_BUS_WIDTHS_POS, SD_BUS_WIDTHS_LEN));
    }

    return status;
}

static void
mmc_set_init_settings(mmc_disk_t *mmc)
{
#if MYNEWT_VAL_BUS_DRIVER_PRESENT
    mmc->spi.node.freq = mmc->spi.mmc_spi_cfg.initial_freq_khz;
#else
    int rc;

    struct hal_spi_settings spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .data_mode = mmc->spi.mmc_spi_cfg.clock_mode,
        .baudrate = mmc->spi.mmc_spi_cfg.initial_freq_khz,
        .word_size = HAL_SPI_WORD_SIZE_8BIT,
    };

    hal_spi_disable(mmc->spi.spi_num);
    rc = hal_spi_config(mmc->spi.spi_num, &spi_settings);
    if (rc == 0) {
        hal_spi_enable(mmc->spi.spi_num);
    }
#endif
}

static void
mmc_set_working_settings(mmc_disk_t *mmc)
{
#if MYNEWT_VAL_BUS_DRIVER_PRESENT
    mmc->spi.node.freq = mmc->spi.mmc_spi_cfg.freq_khz;
#else
    int rc;

    struct hal_spi_settings spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .data_mode = mmc->spi.mmc_spi_cfg.clock_mode,
        .baudrate = mmc->spi.mmc_spi_cfg.freq_khz,
        .word_size = HAL_SPI_WORD_SIZE_8BIT,
    };

    hal_spi_disable(mmc->spi.spi_num);
    rc = hal_spi_config(mmc->spi.spi_num, &spi_settings);
    if (rc == 0) {
        hal_spi_enable(mmc->spi.spi_num);
    }
#endif
    MMC_LOG_INFO("Switching to %d kHz", mmc->spi.mmc_spi_cfg.freq_khz);
}

static int
mmc_init(mmc_disk_t *mmc)
{
    int rc = 0;
    uint8_t status;
    uint8_t cmd_resp[4];
    uint8_t buf[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    os_time_t timeout;

    /**
     * NOTE: The state machine below follows:
     *       Section 6.4.1: Power Up Sequence for SD Bus Interface.
     *       Section 7.2.1: Mode Selection and Initialization.
     */

    /* give 10ms for VDD rampup */
    os_time_delay(OS_TICKS_PER_SEC / 100);

    mmc_set_init_settings(mmc);

    mmc_spi_set_cs(mmc, 1);

    MMC_LOG_DEBUG("Send clocks to switch to SPI mode");
    /* send the required >= 74 clock cycles (10 bytes, 80 clock cycles). */
    mmc_spi_tx(mmc, buf, 10);

    /* put card in idle state */
    status = mmc_cmd_r1(mmc, CMD0, 0);

    /* No card inserted or bad card? */
    if (status != R_IDLE) {
        MMC_LOG_ERROR("Card not detected");
        rc = MMC_CARD_ERROR;
        goto out;
    }

    mmc_set_working_settings(mmc);

    /**
     * FIXME: while doing a hot-swap of the card or powering off the board
     * it is required to send a CMD1 here otherwise CMD8's response will
     * be always invalid.
     *
     * Why is this happening? CMD1 should never be required for SDxC cards...
     */

    /**
     * 4.3.13: Ask for 2.7-3.3V range, send 0xAA pattern
     *
     * NOTE: cards that are not compliant with "Physical Spec Version 2.00"
     *       will answer this with R_ILLEGAL_COMMAND.
     */
    status = mmc_cmd_rx(mmc, CMD8, 0x1AA, cmd_resp, 4);
    if (status != R_IDLE) {
        status = mmc_cmd_rx(mmc, CMD8, 0x1AA, cmd_resp, 4);
    }
    if (status != 0xff && !(status & R_ILLEGAL_COMMAND)) {

        /**
         * Ver2.00 or later SD Memory Card
         */

        /* Did the card return the same pattern? */
        if (cmd_resp[3] != 0xAA) {
            MMC_LOG_ERROR("CMD8 response error %02x%02x", cmd_resp[2], cmd_resp[3]);
            rc = MMC_RESPONSE_ERROR;
            goto out;
        }

        /**
         * 4.3.13 Send Interface Condition Command (CMD8)
         *   Check VHS for 2.7-3.6V support
         */
        if (cmd_resp[2] != 0x01) {
            MMC_LOG_ERROR("CMD8 voltage error %02x%02x", cmd_resp[2], cmd_resp[3]);
            rc = MMC_VOLTAGE_ERROR;
            goto out;
        }

        /**
         * Wait for card to leave IDLE state or time out
         */

        timeout = os_time_get() + OS_TICKS_PER_SEC;
        for (;;) {
            status = mmc_acmd(mmc, ACMD41, HCS, NULL, 0);
            if ((status & R_IDLE) == 0 || os_time_get() > timeout) {
                break;
            }
            os_time_delay(OS_TICKS_PER_SEC / 10);
        }

        if (status) {
            goto out;
        }

        /**
         * Check if this is an high density card
         */

        status = mmc_cmd_rx(mmc, CMD58, 0, mmc->ocr, 4);
        if (CMD_FAILED(status)) {
            goto out;
        }

        memset(&mmc->csd, 0, sizeof(mmc->csd));
        memset(&mmc->cid, 0, sizeof(mmc->cid));
        memset(&mmc->scr, 0, sizeof(mmc->scr));

        /* CMD9 read csd */
        status = mmc_read_csd(mmc, mmc->csd);
        if (CMD_FAILED(status)) {
            goto out;
        }
        /* ACMD51 read scr */
        status = mmc_read_scr(mmc, mmc->scr);
        if (CMD_FAILED(status)) {
            goto out;
        }
        /* CMD10 read cid, some older cards don't have this */
        (void)mmc_read_cid(mmc, mmc->cid);
    } else if (status != 0xff && status & R_ILLEGAL_COMMAND) {

        mmc_cmd_r1(mmc, CMD0, 0);
        /**
         * Ver1.x SD Memory Card or Not SD Memory Card
         */

        status = mmc_cmd_rx(mmc, CMD58, 0, cmd_resp, 4);
    }

out:
    if (rc == 0 && status != 0) {
        rc = error_by_response(status);
    }
    return rc;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

static void
mmc_open_cb(struct bus_node *node)
{
    mmc_disk_t *mmc = CONTAINER_OF(node, mmc_disk_t, spi);
    mmc_init(mmc);
}

int
mmc_create_dev(mmc_disk_t *mmc, const char *name, struct mmc_spi_cfg *mmc_cfg)
{
    int rc;
    struct bus_node_callbacks cbs = {
        .open = mmc_open_cb,
    };
    struct bus_spi_node_cfg spi_cfg = {
        .freq = mmc_cfg->initial_freq_khz,
        .pin_cs = -1,
        .data_order = BUS_SPI_DATA_ORDER_MSB,
        .mode = mmc_cfg->clock_mode,
        .node_cfg.bus_name = mmc_cfg->bus_name,
    };

    if (mmc_cfg->pin_cs >= 0) {
        hal_gpio_init_out(mmc_cfg->pin_cs, 1);
    }

    mmc->spi.mmc_spi_cfg = *mmc_cfg;
    os_sem_init(&mmc->sem, 0);
    bus_node_set_callbacks((struct os_dev *)mmc, &cbs);

    rc = bus_spi_node_create(name, &mmc->spi.node, &spi_cfg, NULL);

    return rc;
}

#else

int
mmc_create(mmc_disk_t *mmc, int spi_num, struct mmc_spi_cfg *mmc_cfg)
{
    int rc;

    mmc->spi.spi_num = spi_num;
    mmc->spi.mmc_spi_cfg = *mmc_cfg;

    struct hal_spi_settings spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .data_mode = mmc->spi.mmc_spi_cfg.clock_mode,
        .baudrate = mmc->spi.mmc_spi_cfg.initial_freq_khz,
        .word_size = HAL_SPI_WORD_SIZE_8BIT,
    };

    hal_gpio_init_out(mmc->spi.mmc_spi_cfg.pin_cs, 1);

    rc = hal_spi_config(mmc->spi.spi_num, &spi_settings);
    if (rc) {
        return (rc);
    }

    hal_spi_set_txrx_cb(mmc->spi.spi_num, NULL, NULL);
    rc = hal_spi_enable(mmc->spi.spi_num);
    if (rc) {
        return (rc);
    }
    return 0;
}

#endif

/**
 * Commands that return response in R1b format and write
 * commands enter busy state and keep return 0 while the
 * operations are in progress.
 */
static uint8_t
wait_busy(mmc_disk_t *mmc)
{
    os_time_t timeout;
    uint8_t res;

    timeout = os_time_get() + OS_TICKS_PER_SEC / 2;
    do {
        mmc_spi_rx(mmc, &res, 1);
        if (res) {
            break;
        }
        os_time_delay(OS_TICKS_PER_SEC / 1000);
    } while (os_time_get() < timeout);

    return res;
}

static int
mmc_get_info(const struct disk *disk, struct disk_info *info)
{
    mmc_disk_t *mmc = CONTAINER_OF(disk, mmc_disk_t, disk);

    *info = mmc->disk_info;

    return 0;
}

static int
mmc_eject(struct disk *disk)
{
    mmc_disk_t *mmc = CONTAINER_OF(disk, mmc_disk_t, disk);
    /* TODO: Check what needs to be done here */

    mmc->disk_info.present = false;
    return 0;
}

/**
 * @return 0 on success, non-zero on failure
 */
static int
mmc_read(struct disk *disk, uint32_t lba, void *buf, uint32_t block_count)
{
    uint8_t cmd;
    uint8_t res;
    int rc;
    mmc_disk_t *mmc;
    uint8_t crc[2];

    mmc = CONTAINER_OF(disk, mmc_disk_t, disk);

    rc = MMC_OK;

    mmc_spi_set_cs(mmc, 0);

    cmd = (block_count == 1) ? CMD17 : CMD18;
    res = send_mmc_cmd(mmc, cmd, lba);
    if (res) {
        rc = error_by_response(res);
        goto out;
    }

    while (block_count--) {
        /**
         * 7.3.3 Control tokens
         *   Wait up to 200ms for control token.
         */
        res = mmc_wait_for_data(mmc, 200);

        /**
         * 7.3.3.2 Start Block Tokens and Stop Tran Token
         */
        if (res != START_BLOCK) {
            rc = MMC_TIMEOUT;
            goto out;
        }

        mmc_spi_rx(mmc, buf, BLOCK_LEN);
        mmc_spi_rx(mmc, crc, 2);

        buf = (uint8_t *)buf + BLOCK_LEN;
    }

    if (cmd == CMD18) {
        send_mmc_cmd(mmc, CMD12, 0);
    }
    wait_busy(mmc);

out:
    mmc_spi_set_cs(mmc, 1);
    return (rc);
}

/**
 * @return 0 on success, non-zero on failure
 */
int
mmc_write(struct disk *disk, uint32_t lba, const void *buf, uint32_t block_count)
{
    uint8_t cmd;
    uint8_t res;
    uint8_t crc[2] = { 0xFF, 0xFF };
    int rc;
    mmc_disk_t *mmc;
    mmc = CONTAINER_OF(disk, mmc_disk_t, disk);

    mmc_spi_set_cs(mmc, 0);

    /* now start write */
    cmd = (block_count == 1) ? CMD24 : CMD25;
    res = send_mmc_cmd(mmc, cmd, lba);
    if (res) {
        rc = error_by_response(res);
        goto out;
    }

    while (block_count--) {
        /**
         * 7.3.3.2 Start Block Tokens and Stop Tran Token
         */
        if (cmd == CMD24) {
            mmc_spi_tx_byte(mmc, START_BLOCK);
        } else {
            mmc_spi_tx_byte(mmc, START_BLOCK_TOKEN);
        }

        mmc_spi_tx(mmc, buf, BLOCK_LEN);
        mmc_spi_tx(mmc, crc, 2);

        /**
         * 7.3.3.1 Data Response Token
         */
        mmc_spi_rx(mmc, &res, 1);
        if ((res & 0x1f) != 0x05) {
            break;
        }

        if (cmd == CMD25) {
            wait_busy(mmc);
        }

        buf = (uint8_t *)buf + BLOCK_LEN;
    }

    if (cmd == CMD25) {
        mmc_spi_tx_byte(mmc, STOP_TRAN_TOKEN);
        wait_busy(mmc);
    }

    res &= 0x1f;
    switch (res) {
        case 0x05:
            rc = MMC_OK;
            break;
        case 0x0b:
            rc = MMC_CRC_ERROR;
            break;
        case 0x0d:  /* passthrough */
        default:
            rc = MMC_WRITE_ERROR;
    }

    wait_busy(mmc);

out:
    mmc_spi_set_cs(mmc, 1);
    return (rc);
}

/*
 *
 */
disk_ops_t mmc_disk_ops = {
    .get_info = mmc_get_info,
    .eject = mmc_eject,
    .read = mmc_read,
    .write = mmc_write,
};

static mmc_disk_t mmc0 = {
    .disk.ops = &mmc_disk_ops,
    .disk_info = {
        .name = "SD0",
    }
};

void
mmc_pkg_init(void)
{
    int rc;

    if (MYNEWT_VAL_MMC_LED_PIN >= 0) {
        hal_gpio_init_out(MYNEWT_VAL_MMC_LED_PIN, !MYNEWT_VAL_MMC_LED_ON_VALUE);
    }

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = mmc_create_dev(&mmc0, MYNEWT_VAL(MMC_DEV_NAME), &mmc0_config);
#else
    struct mmc_spi_cfg spi_cfg = {
        .clock_mode = MYNEWT_VAL_MMC_SPI_CLOCK_MODE,
        .initial_freq_khz = 400,
        .freq_khz = MYNEWT_VAL_MMC_SPI_FREQ,
        .pin_cs = MYNEWT_VAL_MMC_SPI_CS_PIN,
    };
    rc = mmc_create(&mmc0, MYNEWT_VAL(MMC_SPI_NUM), &spi_cfg);
#endif
    if (rc == 0 && MYNEWT_VAL_MMC_AUTO_INIT) {
        if (mmc_init(&mmc0) == 0) {
            mmc0.disk_info.block_size = BLOCK_LEN;
            mmc0.disk_info.block_count = csd_sector_count(mmc0.csd);
            mmc0.disk_info.present = 1;
            mn_disk_inserted(&mmc0.disk);
        }
    }
}
