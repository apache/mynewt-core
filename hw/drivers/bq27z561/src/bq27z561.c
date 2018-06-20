/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
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
#include <math.h>
#include <string.h>

#include "os/mynewt.h"
#include "bq27z561/bq27z561.h"
#include "hal/hal_gpio.h"
#include "hal/hal_i2c.h"

#if MYNEWT_VAL(BQ27Z561_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(BQ27Z561_LOG)
static struct log bq27z561_log;
#define LOG_MODULE_BQ27Z561 (253)
#define BQ27Z561_ERROR(...) LOG_ERROR(&bq27z561_log, LOG_MODULE_BQ27Z561, __VA_ARGS__)
#define BQ27Z561_INFO(...)  LOG_INFO(&bq27z561_log, LOG_MODULE_BQ27Z561, __VA_ARGS__)
#else
#define BQ27Z561_ERROR(...)
#define BQ27Z561_INFO(...)
#endif

static uint8_t
bq27z561_calc_chksum(uint8_t *tmpbuf, uint8_t len)
{
    uint8_t i;
    uint8_t chksum;

    chksum = 0;
    if (len != 0) {
        for (i = 0; i < len; ++i) {
            chksum += tmpbuf[i];
        }
        chksum = 0xFF - chksum;
    }
    return chksum;
}


static float
bq27z561_temp_to_celsius(uint16_t temp)
{
    float temp_c;

    temp_c = ((float)temp * 0.1) - 273;
    return temp_c;
}

static int
bq27z561_open(struct os_dev *dev, uint32_t timeout, void *arg)
{
    return 0;
}

static int
bq27z561_close(struct os_dev *dev)
{
    return 0;
}

int
bq27z561_rd_std_reg_word(struct bq27z561 *dev, uint8_t reg, uint16_t *val)
{
    int rc;
    struct hal_i2c_master_data i2c;

    i2c.address = dev->bq27_itf.itf_addr;
    i2c.len = 1;
    i2c.buffer = &reg;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 0);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (wr) failed 0x%02X\n", reg);
        return rc;
    }

    i2c.len = 2;
    i2c.buffer = (uint8_t *)val;
    rc = hal_i2c_master_read(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (rd) failed 0x%02X\n", reg);
        return rc;
    }

    /* XXX: add big-endian support */

    return 0;
}

static int
bq27z561_wr_std_reg_word(struct bq27z561 *dev, uint8_t reg, uint16_t val)
{
    int rc;
    uint8_t buf[3];
    struct hal_i2c_master_data i2c;

    buf[0] = reg;
    buf[1] = (uint8_t)val;
    buf[2] = (uint8_t)(val >> 8);

    i2c.address = dev->bq27_itf.itf_num;
    i2c.len     = 3;
    i2c.buffer  = buf;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg write 0x%02X failed\n", reg);
        return rc;
    }

    return 0;
}

bq27z561_err_t
bq27x561_wr_alt_mfg_cmd(struct bq27z561 *dev, uint16_t cmd, uint8_t *buf,
                        int len)
{
    /* NOTE: add three here for register and two-byte command */
    int rc;
    uint8_t tmpbuf[BQ27Z561_MAX_ALT_MFG_CMD_LEN + 3];
    struct hal_i2c_master_data i2c;

    if ((len > 0) && (buf == NULL)) {
        return BQ27Z561_ERR_INV_PARAMS;
    }

    /* Make sure length is not too long */
    if (len > BQ27Z561_MAX_ALT_MFG_CMD_LEN) {
        return BQ27Z561_ERR_CMD_LEN;
    }

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;
    tmpbuf[1] = (uint8_t)cmd;
    tmpbuf[2] = (uint8_t)(cmd >> 8);

    if (len > 0) {
        memcpy(&tmpbuf[3], buf, len);
    }

    i2c.len = len + 3;
    i2c.address = dev->bq27_itf.itf_addr;
    i2c.buffer = tmpbuf;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (wr) failed 0x%02X\n", reg);
        return BQ27Z561_ERR_I2C_ERR;
    }

    return BQ27Z561_OK;
}

bq27z561_err_t
bq27x561_rd_alt_mfg_cmd(struct bq27z561 *dev, uint16_t cmd, uint8_t *val,
                        int val_len)
{
    bq27z561_err_t rc;
    uint8_t tmpbuf[36];
    uint8_t len;
    uint16_t cmd_read;
    uint8_t chksum;
    struct hal_i2c_master_data i2c;

    if ((val_len == 0) || (val == NULL)) {
        return BQ27Z561_ERR_INV_PARAMS;
    }

    tmpbuf[0] = BQ27Z561_REG_CNTL;
    tmpbuf[1] = (uint8_t)cmd;
    tmpbuf[2] = (uint8_t)(cmd >> 8);

    i2c.len = 3;
    i2c.address = dev->bq27_itf.itf_addr;
    i2c.buffer = tmpbuf;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (wr) failed 0x%02X\n", reg);
        rc = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;
    i2c.len = 1;
    i2c.buffer = tmpbuf;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 0);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (wr) failed 0x%02X\n", reg);
        rc = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    i2c.len = 36;
    i2c.buffer = tmpbuf;
    rc = hal_i2c_master_read(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (rd) failed 0x%02X\n", reg);
        rc = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    /* Verify that first two bytes are the command */
    cmd_read = tmpbuf[0];
    cmd_read |= ((uint16_t)tmpbuf[1]) << 8;
    if (cmd_read != cmd) {
        BQ27Z561_ERROR("cmd mismatch (cmd=%x cmd_ret=%x\n", cmd, cmd_read);
        rc = BQ27Z561_ERR_CMD_MISMATCH;
        goto err;
    }

    /*
     * Verify length. The length contains two bytes for the command and
     * another two for the checksum and length bytes. Thus, there better
     * be at least 5 bytes here
     */
    len = tmpbuf[35];
    if (len < 5) {
        rc = BQ27Z561_ERR_ALT_MFG_LEN;
        goto err;
    }

    /* Subtract out checksum and length bytes from length */
    len -= 2;
    chksum = bq27z561_calc_chksum(tmpbuf, len);
    if (chksum != tmpbuf[34]) {
        BQ27Z561_ERROR("chksum failure for cmd %u (calc=%u read=%u)", cmd,
                       chksum, tmpbuf[34]);
        rc = BQ27Z561_ERR_CHKSUM_FAIL;
    }

    /* Now copy returned data. We subtract command from length */
    len -= 2;
    if (val_len < len) {
        val_len = len;
    }
    memcpy(val, &tmpbuf[2], val_len);

    rc = BQ27Z561_OK;

err:
    return rc;
}

bq27z561_err_t
bq27x561_rd_flash(struct bq27z561 *dev, uint16_t addr, uint8_t *buf, int buflen)
{
    uint8_t tmpbuf[BQ27Z561_MAX_FLASH_RW_LEN + 2];
    uint16_t addr_read;
    bq27z561_err_t rc;
    struct hal_i2c_master_data i2c;

    if ((buflen == 0) || !buf || (buflen > BQ27Z561_MAX_FLASH_RW_LEN)) {
        return BQ27Z561_ERR_INV_PARAMS;
    }

    if ((addr < BQ27Z561_FLASH_BEG_ADDR) || (addr > BQ27Z561_FLASH_END_ADDR)) {
        return BQ27Z561_ERR_INV_FLASH_ADDR;
    }

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;
    tmpbuf[1] = (uint8_t)addr;
    tmpbuf[2] = (uint8_t)(addr >> 8);

    i2c.len = 3;
    i2c.address = dev->bq27_itf.itf_addr;
    i2c.buffer = tmpbuf;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (wr) failed 0x%02X\n", reg);
        rc = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;
    i2c.len = 1;
    i2c.buffer = tmpbuf;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 0);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (wr) failed 0x%02X\n", reg);
        rc = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    i2c.len = buflen + 2;
    i2c.buffer = tmpbuf;
    rc = hal_i2c_master_read(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (rd) failed 0x%02X\n", reg);
        rc = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    /* Verify that first two bytes are the address*/
    addr_read = tmpbuf[0];
    addr_read |= ((uint16_t)tmpbuf[1]) << 8;
    if (addr_read != addr) {
        BQ27Z561_ERROR("addr mismatch (addr_read=%x addr_ret=%x\n", cmd,
                        cmd_read);
        rc = BQ27Z561_ERR_FLASH_ADDR_MISMATCH;
        goto err;
    }

    /* Now copy returned data. */
    memcpy(buf, &tmpbuf[2], buflen);

    rc = BQ27Z561_OK;

err:
    return rc;
}

bq27z561_err_t
bq27x561_wr_flash(struct bq27z561 *dev, uint16_t addr, uint8_t *buf, int buflen)
{
    uint8_t tmpbuf[BQ27Z561_MAX_FLASH_RW_LEN + 2];
    uint8_t chksum;
    bq27z561_err_t rc;
    struct hal_i2c_master_data i2c;

    if ((buflen == 0) || (!buf) || (buflen > BQ27Z561_MAX_FLASH_RW_LEN)) {
        return BQ27Z561_ERR_INV_PARAMS;
    }

    if ((addr < BQ27Z561_FLASH_BEG_ADDR) || (addr > BQ27Z561_FLASH_END_ADDR)) {
        return BQ27Z561_ERR_INV_FLASH_ADDR;
    }

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;
    tmpbuf[1] = (uint8_t)addr;
    tmpbuf[2] = (uint8_t)(addr >> 8);
    memcpy(&tmpbuf[3], buf, buflen);

    i2c.len = buflen + 3;
    i2c.address = dev->bq27_itf.itf_addr;
    i2c.buffer = tmpbuf;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (wr) failed 0x%02X\n", reg);
        rc = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    /* Calculate checksum */
    chksum = bq27z561_calc_chksum(&tmpbuf[1], buflen + 2);

    /* Write checksum and length */
    tmpbuf[0] = BQ27Z561_REG_CHKSUM;
    tmpbuf[1] = chksum;
    tmpbuf[2] = buflen + 4;
    i2c.len = 3;
    i2c.buffer = tmpbuf;

    rc = hal_i2c_master_write(dev->bq27_itf.itf_num, &i2c, OS_TICKS_PER_SEC, 1);
    if (rc != 0) {
        BQ27Z561_ERROR("I2C reg read (wr) failed 0x%02X\n", reg);
        rc = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    rc = BQ27Z561_OK;

err:
    return rc;
}

#if 0
static int
bq27z561_get_chip_id(struct bq27z561 *dev, uint8_t *chip_id)
{
    return 0;
}
#endif

/* XXX: no support for control register yet */

int
bq27z561_set_at_rate(struct bq27z561 *dev, int16_t current)
{
    int rc;

    rc = bq27z561_wr_std_reg_word(dev, BQ27Z561_REG_AR, (uint16_t)current);
    return rc;
}

int
bq27z561_get_at_rate(struct bq27z561 *dev, int16_t *current)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_AR, (uint16_t *)current);
    return rc;
}

int
bq27z561_get_time_to_empty(struct bq27z561 *dev, uint16_t *tte)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_ARTTE, tte);
    return rc;
}

int
bq27z561_get_temp(struct bq27z561 *dev, float *temp_c)
{
    int rc;
    uint16_t val;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_TEMP, &val);
    if (!rc) {
        /* Kelvin to Celsius */
        *temp_c = bq27z561_temp_to_celsius(val);
    }
    return rc;
}

int
bq27z561_get_voltage(struct bq27z561 *dev, uint16_t *voltage)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_VOLT, voltage);
    return rc;
}

int
bq27z561_get_batt_status(struct bq27z561 *dev, uint16_t *status)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_FLAGS, status);
    return rc;
}

int
bq27z561_get_current(struct bq27z561 *dev, int16_t *current)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_INSTCURR,
                                  (uint16_t *)current);
    return rc;
}

/* XXX: no support for register IMAX */

int
bq27z561_get_rem_capacity(struct bq27z561 *dev, uint16_t *capacity)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_RM, capacity);
    return rc;
}

int
bq27z561_get_full_chg_capacity(struct bq27z561 *dev, uint16_t *capacity)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_FCC, capacity);
    return rc;
}

int
bq27z561_get_avg_current(struct bq27z561 *dev, int16_t *current)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_AI, (uint16_t *)current);
    return rc;
}

int
bq27z561_get_avg_time_to_empty(struct bq27z561 *dev, uint16_t *tte)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_TTE, tte);
    return rc;
}

int
bq27z561_get_avg_time_to_full(struct bq27z561 *dev, uint16_t *ttf)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_TTF, ttf);
    return rc;
}

int
bq27z561_get_avg_power(struct bq27z561 *dev, int16_t *pwr)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_AP, (uint16_t *)pwr);
    return rc;
}

/* XXX: no support for max load current */
/* XXX: no support for max load time to empty */

int
bq27z561_get_internal_temp(struct bq27z561 *dev, float *temp_c)
{
    int rc;
    uint16_t val;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_INT_TEMP, &val);
    if (!rc) {
        *temp_c = bq27z561_temp_to_celsius(val);
    }
    return rc;
}

int
bq27z561_get_discharge_cycles(struct bq27z561 *dev, uint16_t *cycles)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_CC, cycles);
    return rc;
}

int
bq27z561_get_relative_state_of_charge(struct bq27z561 *dev, uint8_t *pcnt)
{
    int rc;
    uint16_t val;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_RSOC, &val);
    if (!rc) {
        *pcnt = (uint8_t)val;
    }
    return rc;
}

int
bq27z561_get_state_of_health(struct bq27z561 *dev, uint8_t *pcnt)
{
    int rc;
    uint16_t val;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_SOH, &val);
    if (!rc) {
        *pcnt = (uint8_t)val;
    }
    return rc;
}

int
bq27z561_get_charging_voltage(struct bq27z561 *dev, uint16_t *voltage)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_CV, voltage);
    return rc;
}

int
bq27z561_get_charging_current(struct bq27z561 *dev, uint16_t *current)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_CHGC, current);
    return rc;
}

int
bq27z561_config(struct bq27z561 *dev, struct bq27z561_cfg *cfg)
{
    return 0;
}

int
bq27z561_init(struct os_dev *dev, void *arg)
{
    struct bq27z561 *bq27;

    if (!dev || !arg) {
        return SYS_ENODEV;
    }

    OS_DEV_SETHANDLERS(dev, bq27z561_open, bq27z561_close);

    /* Copy the interface struct */
    bq27 = (struct bq27z561 *)dev;
    memcpy(&bq27->bq27_itf, arg, sizeof(struct bq27z561_itf));

    return 0;
}
