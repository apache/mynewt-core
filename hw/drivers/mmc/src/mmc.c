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

#define MIN(n, m) (((n) < (m)) ? (n) : (m))

/* MMC commands used by the driver */
#define CMD0                (0)            /* GO_IDLE_STATE */
#define CMD1                (1)            /* SEND_OP_COND (MMC) */
#define CMD8                (8)            /* SEND_IF_COND */
#define CMD12               (12)           /* STOP_TRANSMISSION */
#define CMD16               (16)           /* SET_BLOCKLEN */
#define CMD17               (17)           /* READ_SINGLE_BLOCK */
#define CMD18               (18)           /* READ_MULTIPLE_BLOCK */
#define CMD24               (24)           /* WRITE_BLOCK */
#define CMD25               (25)           /* WRITE_MULTIPLE_BLOCK */
#define CMD55               (55)           /* APP_CMD */
#define CMD58               (58)           /* READ_OCR */
#define ACMD41              (0x80 + 41)    /* SEND_OP_COND (SDC) */

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

#define START_BLOCK         (0xFE)
#define START_BLOCK_TOKEN   (0xFC)
#define STOP_TRAN_TOKEN     (0xFD)

#define BLOCK_LEN           (512)

static uint8_t g_block_buf[BLOCK_LEN + 2];

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)

static struct mmc {
    int spi_num;
    int ss_pin;
    struct mmc_spi_cfg mmc_spi_cfg;
} g_mmc;

/* FIXME: currently limited to single MMC spi device */
static struct mmc *mmcs[1] = { &g_mmc };

#else
static struct mmc *mmcs[1];
static uint8_t mmcs_count;
#endif

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

static struct mmc *
mmc_cfg_dev(uint8_t id)
{
    if (id != 0) {
        return NULL;
    }

    return mmcs[0];
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

static void
mmc_spi_set_cs(struct mmc *mmc, int cs)
{
    /* Let bus driver actiave CS, deactivation is done here */
    if (cs) {
        hal_gpio_write(mmc->node.pin_cs, cs);
    }
}

static int
mmc_spi_tx(struct mmc *mmc, const uint8_t *buf, uint16_t count)
{
    return bus_node_write(&mmc->node.bnode.odev, buf, count, 1000, BUS_F_NOSTOP);
}

static int
mmc_spi_rx(struct mmc *mmc, uint8_t *buf, uint16_t count)
{
    memset(buf, 0xFF, count);
    return bus_node_duplex_write_read(&mmc->node.bnode.odev, buf, buf, count, 1000, BUS_F_NOSTOP);
}

#else

static void
mmc_spi_set_cs(struct mmc *mmc, int cs)
{
    hal_gpio_write(mmc->ss_pin, cs);
}

static int
mmc_spi_tx(struct mmc *mmc, const uint8_t *buf, uint16_t count)
{
    for (int i = 0; i < count; i++) {
        hal_spi_tx_val(mmc->spi_num, buf[i]);
    }
    return 0;
}

static int
mmc_spi_rx(struct mmc *mmc, uint8_t *buf, uint16_t count)
{
    for (int i = 0; i < count; i++) {
        buf[i] = hal_spi_tx_val(mmc->spi_num, 0xFF);
    }
    return 0;
}

#endif

static int
mmc_spi_tx_byte(struct mmc *mmc, uint8_t b)
{
    return mmc_spi_tx(mmc, &b, 1);
}

static uint8_t
send_mmc_cmd(struct mmc *mmc, uint8_t cmd, uint32_t payload)
{
    int n;
    uint8_t status;
    uint8_t crc;
    uint8_t buf[6];

    if (cmd & 0x80) {
        /* TODO: refactor recursion? */
        /* TODO: error checking */
        send_mmc_cmd(mmc, CMD55, 0);
    }

    /* 4.7.2: Command Format */
    buf[0] = 0x40 | (cmd & ~0x80);
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

    for (n = 255; n > 0; n--) {
        mmc_spi_rx(mmc, buf, 1);
        status = buf[0];
        if ((status & 0x80) == 0) {
            break;
        }
    }

    return status;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

static void
mmc_open_cb(struct bus_node *node)
{
    int rc;
    int i;
    uint8_t status;
    uint8_t cmd_resp[4];
    uint32_t ocr;
    os_time_t timeout;
    struct mmc *mmc = (struct mmc *)node;

    /**
     * NOTE: The state machine below follows:
     *       Section 6.4.1: Power Up Sequence for SD Bus Interface.
     *       Section 7.2.1: Mode Selection and Initialization.
     */

    /* give 10ms for VDD rampup */
    os_time_delay(OS_TICKS_PER_SEC / 100);

    /* send the required >= 74 clock cycles (10 bytes, 80 clock cycles). */
    for (i = 0; i < 10; i++) {
        mmc_spi_tx_byte(mmc, 0xff);
    }

    /* put card in idle state */
    status = send_mmc_cmd(mmc, CMD0, 0);

    mmc_spi_set_cs(mmc, 1);
    /* No card inserted or bad card? */
    if (status != R_IDLE) {
        rc = error_by_response(status);
        goto out;
    }

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
    status = send_mmc_cmd(mmc, CMD8, 0x1AA);
    if (status != 0xff && !(status & R_ILLEGAL_COMMAND)) {

        /**
         * Ver2.00 or later SD Memory Card
         */

        /* Read the contents of R7 */
        mmc_spi_rx(mmc, cmd_resp, 4);

        /* Did the card return the same pattern? */
        if (cmd_resp[3] != 0xAA) {
            rc = MMC_RESPONSE_ERROR;
            goto out;
        }

        /**
         * 4.3.13 Send Interface Condition Command (CMD8)
         *   Check VHS for 2.7-3.6V support
         */
        if (cmd_resp[2] != 0x01) {
            rc = MMC_VOLTAGE_ERROR;
            goto out;
        }

        /**
         * Wait for card to leave IDLE state or time out
         */

        timeout = os_time_get() + OS_TICKS_PER_SEC;
        for (;;) {
            status = send_mmc_cmd(mmc, ACMD41, HCS);
            if ((status & R_IDLE) == 0 || os_time_get() > timeout) {
                break;
            }
            os_time_delay(OS_TICKS_PER_SEC / 10);
        }

        if (status) {
            rc = error_by_response(status);
            goto out;
        }

        /**
         * Check if this is an high density card
         */

        status = send_mmc_cmd(mmc, CMD58, 0);
        mmc_spi_rx(mmc, cmd_resp, 4);
        if (status == 0 && (cmd_resp[0] & (1 << 6))) {  /* FIXME: CCS */
            /**
             * TODO: if CCS is set this is an SDHC or SDXC card!
             *       SDSC uses byte addressing, SDHC/SDXC block addressing
             *
             *       can also check the whole voltage range supported here
             */
        }

    } else if (status != 0xff && status & R_ILLEGAL_COMMAND) {

        /**
         * Ver1.x SD Memory Card or Not SD Memory Card
         */

        ocr = send_mmc_cmd(mmc, CMD58, 0);

        /* TODO: check if voltage range is ok! */

        if (ocr & R_ILLEGAL_COMMAND) {

        }

        /* TODO: set blocklen */

    } else {
        rc = error_by_response(status);
    }

out:
    mmc_spi_set_cs(mmc, 1);
    (void)rc;
}

int
mmc_create_dev(struct mmc *mmc, const char *name, struct mmc_config *mmc_cfg)
{
    int rc;
    struct bus_node_callbacks cbs = {
        .open = mmc_open_cb,
    };

    bus_node_set_callbacks((struct os_dev *)mmc, &cbs);

    rc = bus_spi_node_create(name, &mmc->node, &mmc_cfg->spi_cfg, NULL);
    if (rc == 0 && mmcs_count < ARRAY_SIZE(mmcs)) {
        mmcs[mmcs_count++] = mmc;
    }

    return rc;
}

#else
/**
 * Initialize the MMC driver
 *
 * @param spi_num Number of the SPI channel to be used by MMC
 * @param spi_cfg Low-level device specific SPI configuration
 * @param ss_pin Number of SS pin if SW controlled, -1 otherwise
 *
 * @return 0 on success, non-zero on failure
 */
int
mmc_init(int spi_num, struct mmc_spi_cfg *spi_cfg, int ss_pin)
{
    int rc;
    int i;
    uint8_t status;
    uint8_t cmd_resp[4];
    uint32_t ocr;
    os_time_t timeout;
    /* TODO: create new struct for every new spi mmc, add to SLIST */
    struct mmc *mmc = mmcs[0];
    mmc->ss_pin = ss_pin;
    mmc->mmc_spi_cfg = *spi_cfg;

    mmc->spi_num = spi_num;
    struct hal_spi_settings spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .data_mode = mmc->mmc_spi_cfg.clock_mode,
        .baudrate = mmc->mmc_spi_cfg.initial_freq_khz,
        .word_size = HAL_SPI_WORD_SIZE_8BIT,
    };

    hal_gpio_init_out(mmc->ss_pin, 1);

    rc = hal_spi_config(mmc->spi_num, &spi_settings);
    if (rc) {
        return (rc);
    }

    hal_spi_set_txrx_cb(mmc->spi_num, NULL, NULL);
    rc = hal_spi_enable(mmc->spi_num);
    if (rc) {
        return (rc);
    }

    /**
     * NOTE: The state machine below follows:
     *       Section 6.4.1: Power Up Sequence for SD Bus Interface.
     *       Section 7.2.1: Mode Selection and Initialization.
     */

    /* give 10ms for VDD rampup */
    os_time_delay(OS_TICKS_PER_SEC / 100);

    mmc_spi_set_cs(mmc, 0);

    /* send the required >= 74 clock cycles (10 bytes, 80 clock cycles). */
    for (i = 0; i < 10; i++) {
        mmc_spi_tx_byte(mmc, 0xff);
    }

    /* put card in idle state */
    status = send_mmc_cmd(mmc, CMD0, 0);

    mmc_spi_set_cs(mmc, 1);
    /* No card inserted or bad card? */
    if (status != R_IDLE) {
        rc = error_by_response(status);
        goto out;
    }

    hal_spi_disable(mmc->spi_num);
    spi_settings.baudrate = mmc->mmc_spi_cfg.freq_khz;
    rc = hal_spi_config(mmc->spi_num, &spi_settings);
    hal_spi_enable(mmc->spi_num);
    mmc_spi_set_cs(mmc, 0);

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
    status = send_mmc_cmd(mmc, CMD8, 0x1AA);
    if (status != 0xff && !(status & R_ILLEGAL_COMMAND)) {

        /**
         * Ver2.00 or later SD Memory Card
         */

        /* Read the contents of R7 */
        mmc_spi_rx(mmc, cmd_resp, 4);

        /* Did the card return the same pattern? */
        if (cmd_resp[3] != 0xAA) {
            rc = MMC_RESPONSE_ERROR;
            goto out;
        }

        /**
         * 4.3.13 Send Interface Condition Command (CMD8)
         *   Check VHS for 2.7-3.6V support
         */
        if (cmd_resp[2] != 0x01) {
            rc = MMC_VOLTAGE_ERROR;
            goto out;
        }

        /**
         * Wait for card to leave IDLE state or time out
         */

        timeout = os_time_get() + OS_TICKS_PER_SEC;
        for (;;) {
            status = send_mmc_cmd(mmc, ACMD41, HCS);
            if ((status & R_IDLE) == 0 || os_time_get() > timeout) {
                break;
            }
            os_time_delay(OS_TICKS_PER_SEC / 10);
        }

        if (status) {
            rc = error_by_response(status);
            goto out;
        }

        /**
         * Check if this is an high density card
         */

        status = send_mmc_cmd(mmc, CMD58, 0);
        mmc_spi_rx(mmc, cmd_resp, 4);
        if (status == 0 && (cmd_resp[0] & (1 << 6))) {  /* FIXME: CCS */
            /**
             * TODO: if CCS is set this is an SDHC or SDXC card!
             *       SDSC uses byte addressing, SDHC/SDXC block addressing
             *
             *       can also check the whole voltage range supported here
             */
        }

    } else if (status != 0xff && status & R_ILLEGAL_COMMAND) {

        /**
         * Ver1.x SD Memory Card or Not SD Memory Card
         */

        ocr = send_mmc_cmd(mmc, CMD58, 0);

        /* TODO: check if voltage range is ok! */

        if (ocr & R_ILLEGAL_COMMAND) {

        }

        /* TODO: set blocklen */

    } else {
        rc = error_by_response(status);
    }

out:
    mmc_spi_set_cs(mmc, 1);
    return rc;
}

#endif

/**
 * Commands that return response in R1b format and write
 * commands enter busy state and keep return 0 while the
 * operations are in progress.
 */
static uint8_t
wait_busy(struct mmc *mmc)
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

/**
 * @return 0 on success, non-zero on failure
 */
int
mmc_read(uint8_t mmc_id, uint32_t addr, void *buf, uint32_t len)
{
    uint8_t cmd;
    uint8_t res;
    int rc;
    size_t block_len;
    size_t block_count;
    os_time_t timeout;
    uint32_t block_addr;
    size_t offset;
    size_t index;
    size_t amount;
    struct mmc *mmc;

    mmc = mmc_cfg_dev(mmc_id);
    if (mmc == NULL) {
        return (MMC_DEVICE_ERROR);
    }

    rc = MMC_OK;

    block_len = (len + BLOCK_LEN - 1) & ~(BLOCK_LEN - 1);
    block_count = block_len / BLOCK_LEN;
    block_addr = addr / BLOCK_LEN;
    offset = addr - (block_addr * BLOCK_LEN);

    mmc_spi_set_cs(mmc, 0);

    cmd = (block_count == 1) ? CMD17 : CMD18;
    res = send_mmc_cmd(mmc, cmd, block_addr);
    if (res) {
        rc = error_by_response(res);
        goto out;
    }

    index = 0;
    while (block_count--) {
        /**
         * 7.3.3 Control tokens
         *   Wait up to 200ms for control token.
         */
        timeout = os_time_get() + OS_TICKS_PER_SEC / 5;
        do {
            mmc_spi_rx(mmc, &res, 1);
            if (res != 0xFF) {
                break;
            }
            os_time_delay(OS_TICKS_PER_SEC / 20);
        } while (os_time_get() < timeout);

        /**
         * 7.3.3.2 Start Block Tokens and Stop Tran Token
         */
        if (res != START_BLOCK) {
            rc = MMC_TIMEOUT;
            goto out;
        }

        mmc_spi_rx(mmc, g_block_buf, BLOCK_LEN + 2 /* CRC */);

        /* TODO: CRC-16 not used here but would be cool to have */

        amount = MIN(BLOCK_LEN - offset, len);

        memcpy(((uint8_t *)buf + index), &g_block_buf[offset], amount);

        offset = 0;
        len -= amount;
        index += amount;
    }

    if (cmd == CMD18) {
        send_mmc_cmd(mmc, CMD12, 0);
        wait_busy(mmc);
    }

out:
    mmc_spi_set_cs(mmc, 1);
    return (rc);
}

/**
 * @return 0 on success, non-zero on failure
 */
int
mmc_write(uint8_t mmc_id, uint32_t addr, const void *buf, uint32_t len)
{
    uint8_t cmd;
    uint8_t res;
    size_t block_len;
    size_t block_count;
    os_time_t timeout;
    uint32_t block_addr;
    size_t offset;
    size_t index;
    size_t amount;
    int rc;
    struct mmc *mmc;

    mmc = mmc_cfg_dev(mmc_id);
    if (mmc == NULL) {
        return (MMC_DEVICE_ERROR);
    }

    block_len = (len + BLOCK_LEN - 1) & ~(BLOCK_LEN - 1);
    block_count = block_len / BLOCK_LEN;
    block_addr = addr / BLOCK_LEN;
    offset = addr - (block_addr * BLOCK_LEN);

    mmc_spi_set_cs(mmc, 0);

    /**
     * This code ensures that if the requested address doesn't align with the
     * beginning address of a sector, the initial bytes are first read to the
     * buffer to be then written later.
     *
     * NOTE: this code will never run when using a FS that is sector addressed
     * like FAT (offset is always 0).
     */
    if (offset) {
        res = send_mmc_cmd(mmc, CMD17, block_addr);
        if (res != 0) {
            rc = error_by_response(res);
            goto out;
        }

        timeout = os_time_get() + OS_TICKS_PER_SEC / 5;
        do {
            mmc_spi_rx(mmc, &res, 1);
            if (res != 0xff) {
                break;
            }
            os_time_delay(OS_TICKS_PER_SEC / 20);
        } while (os_time_get() < timeout);

        if (res != START_BLOCK) {
            rc = MMC_CARD_ERROR;
            goto out;
        }

        mmc_spi_rx(mmc, g_block_buf, BLOCK_LEN + 2);
    }

    /* now start write */

    cmd = (block_count == 1) ? CMD24 : CMD25;
    res = send_mmc_cmd(mmc, cmd, block_addr);
    if (res) {
        rc = error_by_response(res);
        goto out;
    }

    index = 0;
    while (block_count--) {
        /**
         * 7.3.3.2 Start Block Tokens and Stop Tran Token
         */
        if (cmd == CMD24) {
            mmc_spi_tx_byte(mmc, START_BLOCK);
        } else {
            mmc_spi_tx_byte(mmc, START_BLOCK_TOKEN);
        }

        amount = MIN(BLOCK_LEN - offset, len);
        memcpy(&g_block_buf[offset], ((uint8_t *)buf + index), amount);

        /* CRC */
        g_block_buf[BLOCK_LEN] = 0xFF;
        g_block_buf[BLOCK_LEN + 1] = 0xFF;
        mmc_spi_tx(mmc, g_block_buf, BLOCK_LEN + 2);

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

        offset = 0;
        len -= amount;
        index += amount;
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
int
mmc_ioctl(uint8_t mmc_id, uint32_t cmd, void *arg)
{
    return 0;
}

/*
 *
 */
struct disk_ops mmc_ops = {
    .read  = &mmc_read,
    .write = &mmc_write,
    .ioctl = &mmc_ioctl,
};

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static struct mmc mmc;
static struct mmc_config mmc_config = {
    .spi_cfg = {
        .pin_cs = MYNEWT_VAL(MMC_SPI_CS_PIN),
        .freq = MYNEWT_VAL(MMC_SPI_FREQ),
        .data_order = BUS_SPI_DATA_ORDER_MSB,
        .mode = BUS_SPI_MODE_0,
        .node_cfg.bus_name = MYNEWT_VAL(MMC_SPI_BUS_NAME),
        .node_cfg.lock_timeout_ms = 1000,
    }
};
#endif

void
mmc_pkg_init(void)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    if (0 == mmc_create_dev(&mmc, MYNEWT_VAL(MMC_DEV_NAME), &mmc_config)) {
        if (MYNEWT_VAL(MMC_AUTO_MOUNT)) {
            os_dev_open(MYNEWT_VAL(MMC_DEV_NAME), 1000, NULL);
        }
    }
#else
    struct mmc_spi_cfg spi_cfg = {
        .clock_mode = HAL_SPI_MODE0,
        .initial_freq_khz = 400,
        .freq_khz = MYNEWT_VAL(MMC_SPI_FREQ),
    };
    mmc_init(MYNEWT_VAL(MMC_SPI_NUM), &spi_cfg, MYNEWT_VAL(MMC_SPI_CS_PIN));
#endif
    if (MYNEWT_VAL(MMC_AUTO_MOUNT)) {
        uint8_t *sector = malloc(512);

        if (sector == NULL) {
        } else {
            /* For now only FAT partition are mounted */
            if (mmc_read(0, 0, sector, 512) == 0) {
                /* Check if first partition is FAT */
                if (sector[450] == 0x0C) {
                    disk_register(MYNEWT_VAL(MMC_DEV_NAME), "fatfs", &mmc_ops);
                }
            }
        }
        free(sector);
    }
}
