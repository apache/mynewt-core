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

#include <math.h>
#include <string.h>

#include "os/mynewt.h"
#include "bq27z561/bq27z561.h"
#include "hal/hal_gpio.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#else
#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"
#endif

#include "battery/battery_prop.h"

#if MYNEWT_VAL(BQ27Z561_LOG)
#include "modlog/modlog.h"
#endif

#if MYNEWT_VAL(BQ27Z561_LOG)

#define BQ27Z561_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(BQ27Z561_LOG_MODULE), __VA_ARGS__)
#else
#define BQ27Z561_LOG(lvl_, ...)
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

    temp_c = temp * 0.1f - 273.0f;
    return temp_c;
}

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Lock access to the bq27z561_itf specified by si. Blocks until lock acquired.
 *
 * @param The bq27z561_itf to lock
 * @param The timeout
 *
 * @return 0 on success, non-zero on failure.
 */
static int
bq27z561_itf_lock(struct bq27z561_itf *bi, uint32_t timeout)
{
    int rc;
    os_time_t ticks;

    if (!bi->itf_lock) {
        return 0;
    }

    rc = os_time_ms_to_ticks(timeout, &ticks);
    if (rc) {
        return rc;
    }

    rc = os_mutex_pend(bi->itf_lock, ticks);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }

    return (rc);
}

/**
 * Unlock access to the bq27z561_itf specified by bi.
 *
 * @param The bq27z561_itf to unlock access to
 *
 * @return 0 on success, non-zero on failure.
 */
static void
bq27z561_itf_unlock(struct bq27z561_itf *bi)
{
    if (!bi->itf_lock) {
        return;
    }

    os_mutex_release(bi->itf_lock);
}
#endif

static int
bq27z561_rd_std_reg_byte(struct bq27z561 *dev, uint8_t reg, uint8_t *val)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write_read_transact(&dev->dev.dev, &reg, 1, val, 1);
#else
    struct hal_i2c_master_data i2c;

    i2c.address = dev->bq27_itf.itf_addr;
    i2c.len = 1;
    i2c.buffer = &reg;

    rc = bq27z561_itf_lock(&dev->bq27_itf, MYNEWT_VAL(BQ27Z561_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }


    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 0,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", reg);
        goto err;
    }

    i2c.len = 1;
    i2c.buffer = (uint8_t *)val;
    rc = i2cn_master_read(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                          MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (rd) failed 0x%02X\n", reg);
        goto err;
    }

err:
    bq27z561_itf_unlock(&dev->bq27_itf);

#endif
    return rc;
}

int
bq27z561_rd_std_reg_word(struct bq27z561 *dev, uint8_t reg, uint16_t *val)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write_read_transact(&dev->dev.dev, &reg, 1, val, 2);
#else
    struct hal_i2c_master_data i2c;

    i2c.address = dev->bq27_itf.itf_addr;
    i2c.len = 1;
    i2c.buffer = &reg;

    rc = bq27z561_itf_lock(&dev->bq27_itf, MYNEWT_VAL(BQ27Z561_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 0,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", reg);
        goto err;
    }

    i2c.len = 2;
    i2c.buffer = (uint8_t *)val;
    rc = i2cn_master_read(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                          MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (rd) failed 0x%02X\n", reg);
        goto err;
    }

err:
    bq27z561_itf_unlock(&dev->bq27_itf);
#endif

    /* XXX: add big-endian support */

    return rc;
}

static int
bq27z561_wr_std_reg_byte(struct bq27z561 *dev, uint8_t reg, uint8_t val)
{
    int rc;
    uint8_t buf[2];

    buf[0] = reg;
    buf[1] = val;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write(&dev->dev.dev, buf, 2);
#else
    struct hal_i2c_master_data i2c;

    i2c.address = dev->bq27_itf.itf_num;
    i2c.len     = 2;
    i2c.buffer  = buf;

    rc = bq27z561_itf_lock(&dev->bq27_itf, MYNEWT_VAL(BQ27Z561_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg write 0x%02X failed\n", reg);
    }

    bq27z561_itf_unlock(&dev->bq27_itf);
#endif

    return rc;
}

static int
bq27z561_wr_std_reg_word(struct bq27z561 *dev, uint8_t reg, uint16_t val)
{
    int rc;
    uint8_t buf[3];

    buf[0] = reg;
    buf[1] = (uint8_t)val;
    buf[2] = (uint8_t)(val >> 8);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write(&dev->dev.dev, buf, 3);
#else
    struct hal_i2c_master_data i2c;

    i2c.address = dev->bq27_itf.itf_num;
    i2c.len     = 3;
    i2c.buffer  = buf;

    rc = bq27z561_itf_lock(&dev->bq27_itf, MYNEWT_VAL(BQ27Z561_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg write 0x%02X failed\n", reg);
        goto err;
    }

err:
    bq27z561_itf_unlock(&dev->bq27_itf);
#endif

    return rc;
}

bq27z561_err_t
bq27x561_wr_alt_mfg_cmd(struct bq27z561 *dev, uint16_t cmd, uint8_t *buf,
                        int len)
{
    /* NOTE: add three here for register and two-byte command */
    int rc;
    uint8_t tmpbuf[BQ27Z561_MAX_ALT_MFG_CMD_LEN + 3];

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

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write(&dev->dev.dev, tmpbuf, len + 3);
#else
    struct hal_i2c_master_data i2c;

    i2c.address = dev->bq27_itf.itf_addr;
    i2c.len = len + 3;
    i2c.buffer = tmpbuf;

    rc = bq27z561_itf_lock(&dev->bq27_itf, MYNEWT_VAL(BQ27Z561_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", tmpbuf[0]);
        rc = BQ27Z561_ERR_I2C_ERR;
    }

    bq27z561_itf_unlock(&dev->bq27_itf);
#endif

    return rc;
}

bq27z561_err_t
bq27x561_rd_alt_mfg_cmd(struct bq27z561 *dev, uint16_t cmd, uint8_t *val,
                        int val_len)
{
    bq27z561_err_t ret;
    int rc;
    uint8_t tmpbuf[36];
    uint8_t len;
    uint16_t cmd_read;
    uint8_t chksum;
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct os_dev *odev = &dev->dev.dev;
#else
    struct hal_i2c_master_data i2c;
#endif

    if ((val_len == 0) || (val == NULL)) {
        return BQ27Z561_ERR_INV_PARAMS;
    }

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_lock(odev, BUS_NODE_LOCK_DEFAULT_TIMEOUT);
    if (rc) {
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }
#else
    i2c.address = dev->bq27_itf.itf_addr;

    rc = bq27z561_itf_lock(&dev->bq27_itf, MYNEWT_VAL(BQ27Z561_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }
#endif

    tmpbuf[0] = BQ27Z561_REG_CNTL;
    tmpbuf[1] = (uint8_t)cmd;
    tmpbuf[2] = (uint8_t)(cmd >> 8);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write(odev, tmpbuf, 3);
    if (rc) {
        (void)bus_node_unlock(odev);
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }
#else
    i2c.len = 3;
    i2c.buffer = tmpbuf;
    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", tmpbuf[0]);
        ret = BQ27Z561_ERR_I2C_ERR;
        bq27z561_itf_unlock(&dev->bq27_itf);
        goto err;
    }
#endif

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write_read_transact(odev, tmpbuf, 1, tmpbuf, 36);
    if (rc) {
        (void)bus_node_unlock(odev);
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    (void)bus_node_unlock(odev);
#else
    i2c.len = 1;
    i2c.buffer = tmpbuf;
    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 0,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", tmpbuf[0]);
        ret = BQ27Z561_ERR_I2C_ERR;
        bq27z561_itf_unlock(&dev->bq27_itf);
        goto err;
    }

    i2c.len = 36;
    i2c.buffer = tmpbuf;
    rc = i2cn_master_read(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                          MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (rd) failed 0x%02X\n", tmpbuf[0]);
        ret = BQ27Z561_ERR_I2C_ERR;
        bq27z561_itf_unlock(&dev->bq27_itf);
        goto err;
    }

    bq27z561_itf_unlock(&dev->bq27_itf);
#endif

    /* Verify that first two bytes are the command */
    cmd_read = tmpbuf[0];
    cmd_read |= ((uint16_t)tmpbuf[1]) << 8;
    if (cmd_read != cmd) {
        BQ27Z561_LOG(ERROR, "cmd mismatch (cmd=%x cmd_ret=%x\n", cmd, cmd_read);
        ret = BQ27Z561_ERR_CMD_MISMATCH;
        goto err;
    }

    /*
     * Verify length. The length contains two bytes for the command and
     * another two for the checksum and length bytes. Thus, there better
     * be at least 5 bytes here
     */
    len = tmpbuf[35];
    if (len < 5) {
        ret = BQ27Z561_ERR_ALT_MFG_LEN;
        goto err;
    }

    /* Subtract out checksum and length bytes from length */
    len -= 2;
    chksum = bq27z561_calc_chksum(tmpbuf, len);
    if (chksum != tmpbuf[34]) {
        BQ27Z561_LOG(ERROR, "chksum failure for cmd %u (calc=%u read=%u)", cmd,
                       chksum, tmpbuf[34]);
        ret = BQ27Z561_ERR_CHKSUM_FAIL;
    }

    /* Now copy returned data. We subtract command from length */
    len -= 2;
    if (val_len < len) {
        val_len = len;
    }
    memcpy(val, &tmpbuf[2], val_len);

    ret = BQ27Z561_OK;

err:
    return ret;
}

bq27z561_err_t
bq27x561_rd_flash(struct bq27z561 *dev, uint16_t addr, uint8_t *buf, int buflen)
{
    uint8_t tmpbuf[BQ27Z561_MAX_FLASH_RW_LEN + 2];
    uint16_t addr_read;
    bq27z561_err_t ret;
    int rc;
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct os_dev *odev = &dev->dev.dev;
#else
    struct hal_i2c_master_data i2c;
#endif

    if ((buflen == 0) || !buf || (buflen > BQ27Z561_MAX_FLASH_RW_LEN)) {
        return BQ27Z561_ERR_INV_PARAMS;
    }

    if ((addr < BQ27Z561_FLASH_BEG_ADDR) || (addr > BQ27Z561_FLASH_END_ADDR)) {
        return BQ27Z561_ERR_INV_FLASH_ADDR;
    }

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_lock(odev, BUS_NODE_LOCK_DEFAULT_TIMEOUT);
    if (rc) {
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }
#else
    i2c.address = dev->bq27_itf.itf_addr;

    rc = bq27z561_itf_lock(&dev->bq27_itf, MYNEWT_VAL(BQ27Z561_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }
#endif

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;
    tmpbuf[1] = (uint8_t)addr;
    tmpbuf[2] = (uint8_t)(addr >> 8);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write(odev, tmpbuf, 3);
    if (rc) {
        (void)bus_node_unlock(odev);
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }
#else
    i2c.len = 3;
    i2c.buffer = tmpbuf;
    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", tmpbuf[0]);
        ret = BQ27Z561_ERR_I2C_ERR;
        bq27z561_itf_unlock(&dev->bq27_itf);
        goto err;
    }
#endif

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write_read_transact(odev, tmpbuf, 1, tmpbuf, buflen + 2);
    if (rc) {
        (void)bus_node_unlock(odev);
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    (void)bus_node_unlock(odev);
#else
    i2c.len = 1;
    i2c.buffer = tmpbuf;

    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 0,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", tmpbuf[0]);
        ret = BQ27Z561_ERR_I2C_ERR;
        bq27z561_itf_unlock(&dev->bq27_itf);
        goto err;
    }

    i2c.len = buflen + 2;
    i2c.buffer = tmpbuf;
    rc = i2cn_master_read(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                          MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (rd) failed 0x%02X\n", tmpbuf[0]);
        ret = BQ27Z561_ERR_I2C_ERR;
        bq27z561_itf_unlock(&dev->bq27_itf);
        goto err;
    }

    bq27z561_itf_unlock(&dev->bq27_itf);
#endif

    /* Verify that first two bytes are the address*/
    addr_read = tmpbuf[0];
    addr_read |= ((uint16_t)tmpbuf[1]) << 8;
    if (addr_read != addr) {
        BQ27Z561_LOG(ERROR, "addr mismatch (addr_read=%x addr_ret=%x\n", addr_read,
                        addr);
        ret = BQ27Z561_ERR_FLASH_ADDR_MISMATCH;
        goto err;
    }

    /* Now copy returned data. */
    memcpy(buf, &tmpbuf[2], buflen);

    ret = BQ27Z561_OK;

err:
    return ret;
}

bq27z561_err_t
bq27z561_wr_flash(struct bq27z561 *dev, uint16_t addr, uint8_t *buf, int buflen)
{
    uint8_t tmpbuf[BQ27Z561_MAX_FLASH_RW_LEN + 2];
    uint8_t chksum;
    bq27z561_err_t ret;
    int rc;
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct os_dev *odev = &dev->dev.dev;
#else
    struct hal_i2c_master_data i2c;
#endif

    if ((buflen == 0) || (!buf) || (buflen > BQ27Z561_MAX_FLASH_RW_LEN)) {
        return BQ27Z561_ERR_INV_PARAMS;
    }

    if ((addr < BQ27Z561_FLASH_BEG_ADDR) || (addr + buflen > BQ27Z561_FLASH_END_ADDR)) {
        return BQ27Z561_ERR_INV_FLASH_ADDR;
    }

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_lock(odev, BUS_NODE_LOCK_DEFAULT_TIMEOUT);
    if (rc) {
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }
#else
    i2c.address = dev->bq27_itf.itf_addr;

    rc = bq27z561_itf_lock(&dev->bq27_itf, MYNEWT_VAL(BQ27Z561_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }
#endif

    tmpbuf[0] = BQ27Z561_REG_MFRG_ACC;
    tmpbuf[1] = (uint8_t)addr;
    tmpbuf[2] = (uint8_t)(addr >> 8);
    memcpy(&tmpbuf[3], buf, buflen);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write(odev, tmpbuf, buflen + 3);
    if (rc) {
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }
#else
    i2c.len = buflen + 3;
    i2c.buffer = tmpbuf;
    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", tmpbuf[0]);
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }
#endif

    /* Calculate checksum */
    chksum = bq27z561_calc_chksum(&tmpbuf[1], buflen + 2);

    /* Write checksum and length */
    tmpbuf[0] = BQ27Z561_REG_CHKSUM;
    tmpbuf[1] = chksum;
    tmpbuf[2] = buflen + 4;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write(odev, tmpbuf, 3);
    if (rc) {
        ret = BQ27Z561_ERR_I2C_ERR;
        goto err;
    }

    (void)bus_node_unlock(odev);

    return BQ27Z561_OK;

err:
    (void)bus_node_unlock(odev);
#else
    i2c.len = 3;
    i2c.buffer = tmpbuf;
    rc = i2cn_master_write(dev->bq27_itf.itf_num, &i2c, MYNEWT_VAL(BQ27Z561_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(BQ27Z561_I2C_RETRIES));
    if (rc != 0) {
        BQ27Z561_LOG(ERROR, "I2C reg read (wr) failed 0x%02X\n", tmpbuf[0]);
        ret = BQ27Z561_ERR_I2C_ERR;
    }

err:
    bq27z561_itf_unlock(&dev->bq27_itf);
#endif

    return ret;
}

#if 0
static int
bq27z561_get_chip_id(struct bq27z561 *dev, uint8_t *chip_id)
{
    return 0;
}
#endif

/* Check if bq27z561 is initialized and sets bq27z561 initialized flag */
int
bq27z561_get_init_status(struct bq27z561 *dev, uint8_t *init_flag)
{
    int rc;
    uint16_t init;
    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_FLAGS, &init);
    if (init & BQ27Z561_BATTERY_STATUS_INIT)
    {
    	    *init_flag = 1;
    }
    else
    {
	    *init_flag = 0;
    }
    return rc;
}

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
bq27z561_get_temp_lo_set_threshold(struct bq27z561 *dev, int8_t *temp_c)
{
    int rc;

    rc = bq27z561_rd_std_reg_byte(dev, BQ27Z561_REG_TEMP_LO_SET_TH,
            (uint8_t *)temp_c);

    return rc;
}

int
bq27z561_set_temp_lo_set_threshold(struct bq27z561 *dev, int8_t temp_c)
{
    int rc;
    uint8_t temp = (uint8_t)(temp_c);

    rc = bq27z561_wr_std_reg_byte(dev, BQ27Z561_REG_TEMP_LO_SET_TH, temp);

    return rc;
}

int
bq27z561_get_temp_lo_clr_threshold(struct bq27z561 *dev, int8_t *temp_c)
{
    int rc;

    rc = bq27z561_rd_std_reg_byte(dev, BQ27Z561_REG_TEMP_LO_CLR_TH,
            (uint8_t *)temp_c);

    return rc;
}

int
bq27z561_set_temp_lo_clr_threshold(struct bq27z561 *dev, int8_t temp_c)
{
    int rc;

    rc = bq27z561_wr_std_reg_byte(dev, BQ27Z561_REG_TEMP_LO_CLR_TH,
            (uint8_t)temp_c);

    return rc;
}

int
bq27z561_get_temp_hi_set_threshold(struct bq27z561 *dev, int8_t *temp_c)
{
    int rc;

    rc = bq27z561_rd_std_reg_byte(dev, BQ27Z561_REG_TEMP_HI_SET_TH,
            (uint8_t *)temp_c);

    return rc;
}

int
bq27z561_set_temp_hi_set_threshold(struct bq27z561 *dev, int8_t temp_c)
{
    int rc;

    rc = bq27z561_wr_std_reg_byte(dev, BQ27Z561_REG_TEMP_HI_SET_TH,
            (uint8_t)temp_c);

    return rc;
}

int
bq27z561_get_temp_hi_clr_threshold(struct bq27z561 *dev, int8_t *temp_c)
{
    int rc;

    rc = bq27z561_rd_std_reg_byte(dev, BQ27Z561_REG_TEMP_HI_CLR_TH,
            (uint8_t *)temp_c);

    return rc;
}

int
bq27z561_set_temp_hi_clr_threshold(struct bq27z561 *dev, int8_t temp_c)
{
    int rc;

    rc = bq27z561_wr_std_reg_byte(dev, BQ27Z561_REG_TEMP_HI_CLR_TH,
            (uint8_t)temp_c);

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
bq27z561_get_voltage_lo_set_threshold(struct bq27z561 *dev, uint16_t *voltage)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_VOLT_LO_SET_TH, voltage);

    return rc;
}

int
bq27z561_set_voltage_lo_set_threshold(struct bq27z561 *dev, uint16_t voltage)
{
    int rc;

    rc = bq27z561_wr_std_reg_word(dev, BQ27Z561_REG_VOLT_LO_SET_TH, voltage);

    return rc;
}

int
bq27z561_get_voltage_lo_clr_threshold(struct bq27z561 *dev, uint16_t *voltage)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_VOLT_LO_CLR_TH, voltage);

    return rc;
}

int
bq27z561_set_voltage_lo_clr_threshold(struct bq27z561 *dev, uint16_t voltage)
{
    int rc;

    rc = bq27z561_wr_std_reg_word(dev, BQ27Z561_REG_VOLT_LO_CLR_TH, voltage);

    return rc;
}

int
bq27z561_get_voltage_hi_set_threshold(struct bq27z561 *dev, uint16_t *voltage)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_VOLT_HI_SET_TH, voltage);

    return rc;
}

int
bq27z561_set_voltage_hi_set_threshold(struct bq27z561 *dev, uint16_t voltage)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_VOLT_HI_SET_TH, &voltage);

    return rc;
}

int
bq27z561_get_voltage_hi_clr_threshold(struct bq27z561 *dev, uint16_t *voltage)
{
    int rc;

    rc = bq27z561_rd_std_reg_word(dev, BQ27Z561_REG_VOLT_HI_CLR_TH, voltage);

    return rc;
}

int
bq27z561_set_voltage_hi_clr_threshold(struct bq27z561 *dev, uint16_t voltage)
{
    int rc;

    rc = bq27z561_wr_std_reg_word(dev, BQ27Z561_REG_VOLT_HI_CLR_TH, voltage);

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

/* Battery manager interface functions */

static int
bq27z561_battery_property_get(struct battery_driver *driver,
                          struct battery_property *property, uint32_t timeout)
{
    int rc = 0;
    struct bq27z561 * bq_dev;
    bq_dev = (struct bq27z561 *)&driver->dev;

    if (!bq_dev->bq27_initialized)
    {
        rc = bq27z561_get_init_status((struct bq27z561 *) driver->bd_driver_data,
                                          &bq_dev->bq27_initialized);
        if (!bq_dev->bq27_initialized)
        {
            rc = -2;
            property->bp_valid = 0;
            return rc;
        }
    }

    battery_property_value_t val;
    if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
        property->bp_flags == 0) {
        rc = bq27z561_get_voltage((struct bq27z561 *) driver->bd_driver_data,
                                  &val.bpv_u16);
        property->bp_value.bpv_voltage = val.bpv_u16;
    } else if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_LOW_ALARM_SET_THRESHOLD) {
            rc = bq27z561_get_voltage_lo_set_threshold(
                    (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u16);
            property->bp_value.bpv_voltage = val.bpv_u16;
    } else if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_LOW_ALARM_CLEAR_THRESHOLD) {
            rc = bq27z561_get_voltage_lo_clr_threshold(
                    (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u16);
            property->bp_value.bpv_voltage = val.bpv_u16;
    } else if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_HIGH_ALARM_SET_THRESHOLD) {
            rc = bq27z561_get_voltage_hi_set_threshold(
                    (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u16);
            property->bp_value.bpv_voltage = val.bpv_u16;
    } else if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_HIGH_ALARM_CLEAR_THRESHOLD) {
            rc = bq27z561_get_voltage_hi_clr_threshold(
                    (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u16);
            property->bp_value.bpv_voltage = val.bpv_u16;
    } else if (property->bp_type == BATTERY_PROP_STATUS &&
               property->bp_flags == 0) {
        rc = bq27z561_get_batt_status((struct bq27z561 *) driver->bd_driver_data,
                                      &val.bpv_u16);
        if (val.bpv_u16 & BQ27Z561_BATTERY_STATUS_DSG) {
            property->bp_value.bpv_status = BATTERY_STATUS_DISCHARGING;
        } else if (val.bpv_u16 & BQ27Z561_BATTERY_STATUS_FC) {
            property->bp_value.bpv_status = BATTERY_STATUS_FULL;
        } else {
            property->bp_value.bpv_status = BATTERY_STATUS_CHARGING;
        }
    } else if (property->bp_type == BATTERY_PROP_CURRENT_NOW &&
               property->bp_flags == 0) {
        rc = bq27z561_get_current((struct bq27z561 *) driver->bd_driver_data,
                                  &val.bpv_i16);
        property->bp_value.bpv_current = val.bpv_i16;
    } else if (property->bp_type == BATTERY_PROP_CAPACITY &&
               property->bp_flags == 0) {
        rc = bq27z561_get_rem_capacity((struct bq27z561 *) driver->bd_driver_data,
                                  &val.bpv_u16);
        property->bp_value.bpv_capacity = val.bpv_u16;
    } else if (property->bp_type == BATTERY_PROP_CAPACITY_FULL &&
               property->bp_flags == 0) {
        rc = bq27z561_get_full_chg_capacity((struct bq27z561 *) driver->bd_driver_data,
                                            &val.bpv_u16);
        property->bp_value.bpv_capacity = val.bpv_u16;
    } else if (property->bp_type == BATTERY_PROP_SOC &&
               property->bp_flags == 0) {
        rc = bq27z561_get_relative_state_of_charge(
                (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u8);
        property->bp_value.bpv_soc = val.bpv_u8;
    } else if (property->bp_type == BATTERY_PROP_SOH &&
               property->bp_flags == 0) {
        rc = bq27z561_get_state_of_health(
                (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u8);
        property->bp_value.bpv_soh = val.bpv_u8;
    } else if (property->bp_type == BATTERY_PROP_CYCLE_COUNT &&
               property->bp_flags == 0) {
        rc = bq27z561_get_discharge_cycles(
                (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u16);
        property->bp_value.bpv_cycle_count = val.bpv_u16;
    } else if (property->bp_type == BATTERY_PROP_TIME_TO_EMPTY_NOW &&
               property->bp_flags == 0) {
        rc = bq27z561_get_time_to_empty(
                (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u16);
        property->bp_value.bpv_time_in_s = (uint8_t)val.bpv_u16 * 60;
    } else if (property->bp_type == BATTERY_PROP_TIME_TO_FULL_NOW &&
               property->bp_flags == 0) {
        rc = bq27z561_get_avg_time_to_full(
                (struct bq27z561 *) driver->bd_driver_data, &val.bpv_u16);
        property->bp_value.bpv_time_in_s = val.bpv_u16 * 60;
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
               property->bp_flags == 0) {
        rc = bq27z561_get_temp(
                (struct bq27z561 *) driver->bd_driver_data, &val.bpv_flt);
        property->bp_value.bpv_temperature = val.bpv_flt;
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_LOW_ALARM_SET_THRESHOLD) {
            rc = bq27z561_get_temp_lo_set_threshold(
                    (struct bq27z561 *) driver->bd_driver_data, &val.bpv_i8);
            property->bp_value.bpv_temperature = val.bpv_i8;
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_LOW_ALARM_CLEAR_THRESHOLD) {
            rc = bq27z561_get_temp_lo_clr_threshold(
                    (struct bq27z561 *) driver->bd_driver_data, &val.bpv_i8);
            property->bp_value.bpv_temperature = val.bpv_i8;
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_HIGH_ALARM_SET_THRESHOLD) {
            rc = bq27z561_get_temp_hi_set_threshold(
                    (struct bq27z561 *) driver->bd_driver_data, &val.bpv_i8);
            property->bp_value.bpv_temperature = val.bpv_i8;
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_HIGH_ALARM_CLEAR_THRESHOLD) {
            rc = bq27z561_get_temp_hi_clr_threshold(
                    (struct bq27z561 *) driver->bd_driver_data, &val.bpv_i8);
            property->bp_value.bpv_temperature = val.bpv_i8;
    } else {
        rc = -1;
        return rc;
    }
    if (rc == 0) {
        property->bp_valid = 1;
    } else {
        property->bp_valid = 0;
    }

    return rc;
}

static int
bq27z561_battery_property_set(struct battery_driver *driver,
                          struct battery_property *property)
{
    int rc = 0;

    if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
        property->bp_flags == BATTERY_PROPERTY_FLAGS_LOW_ALARM_SET_THRESHOLD) {
        rc = bq27z561_set_voltage_lo_set_threshold(
                (struct bq27z561 *)driver->bd_driver_data,
                property->bp_value.bpv_voltage);
    } else if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
               property->bp_flags == BATTERY_PROPERTY_FLAGS_LOW_ALARM_CLEAR_THRESHOLD) {
        rc = bq27z561_set_voltage_lo_clr_threshold(
                (struct bq27z561 *)driver->bd_driver_data,
                property->bp_value.bpv_voltage);
    } else if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
               property->bp_flags == BATTERY_PROPERTY_FLAGS_HIGH_ALARM_SET_THRESHOLD) {
        rc = bq27z561_set_voltage_hi_set_threshold(
                (struct bq27z561 *)driver->bd_driver_data,
                property->bp_value.bpv_voltage);
    } else if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
               property->bp_flags == BATTERY_PROPERTY_FLAGS_HIGH_ALARM_CLEAR_THRESHOLD) {
        rc = bq27z561_set_voltage_hi_clr_threshold(
                (struct bq27z561 *)driver->bd_driver_data,
                property->bp_value.bpv_voltage);
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
            property->bp_flags == BATTERY_PROPERTY_FLAGS_LOW_ALARM_SET_THRESHOLD) {
            rc = bq27z561_set_temp_lo_set_threshold(
                    (struct bq27z561 *) driver->bd_driver_data,
                    (int8_t)property->bp_value.bpv_temperature);
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
                   property->bp_flags == BATTERY_PROPERTY_FLAGS_LOW_ALARM_CLEAR_THRESHOLD) {
            rc = bq27z561_set_temp_lo_clr_threshold(
                    (struct bq27z561 *) driver->bd_driver_data,
                    (int16_t)property->bp_value.bpv_temperature);
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
                   property->bp_flags == BATTERY_PROPERTY_FLAGS_HIGH_ALARM_SET_THRESHOLD) {
            rc = bq27z561_set_temp_hi_set_threshold(
                    (struct bq27z561 *) driver->bd_driver_data,
                    (int16_t)property->bp_value.bpv_temperature);
    } else if (property->bp_type == BATTERY_PROP_TEMP_NOW &&
                   property->bp_flags == BATTERY_PROPERTY_FLAGS_HIGH_ALARM_CLEAR_THRESHOLD) {
            rc = bq27z561_set_temp_hi_clr_threshold(
                    (struct bq27z561 *) driver->bd_driver_data,
                    (int16_t)property->bp_value.bpv_temperature);
    } else {
        rc = -1;
    }
    return rc;
}

static int
bq27z561_enable(struct battery *battery)
{
    return 0;
}

static int
bq27z561_disable(struct battery *battery)
{
    return 0;
}

static const struct battery_driver_functions bq27z561_drv_funcs = {
    .bdf_property_get     = bq27z561_battery_property_get,
    .bdf_property_set     = bq27z561_battery_property_set,

    .bdf_enable           = bq27z561_enable,
    .bdf_disable          = bq27z561_disable,
};

static const struct battery_driver_property bq27z561_battery_properties[] = {
    { BATTERY_PROP_STATUS, 0, "Status" },
    { BATTERY_PROP_CAPACITY, 0, "Capacity" },
    { BATTERY_PROP_CAPACITY_FULL, 0, "FullChargeCapacity" },
    { BATTERY_PROP_TEMP_NOW, 0, "Temperature" },
    { BATTERY_PROP_VOLTAGE_NOW, 0, "Voltage" },
    { BATTERY_PROP_CURRENT_NOW, 0, "Current" },
    { BATTERY_PROP_SOC, 0, "SOC" },
    { BATTERY_PROP_SOH, 0, "SOH" },
    { BATTERY_PROP_TIME_TO_EMPTY_NOW, 0, "TimeToEmpty" },
    { BATTERY_PROP_TIME_TO_FULL_NOW, 0, "TimeToFull" },
    { BATTERY_PROP_CYCLE_COUNT, 0, "CycleCount" },
    { BATTERY_PROP_VOLTAGE_NOW,
            BATTERY_PROPERTY_FLAGS_LOW_ALARM_SET_THRESHOLD, "LoVoltAlarmSet" },
    { BATTERY_PROP_VOLTAGE_NOW,
            BATTERY_PROPERTY_FLAGS_LOW_ALARM_CLEAR_THRESHOLD, "LoVoltAlarmClear" },
    { BATTERY_PROP_VOLTAGE_NOW,
            BATTERY_PROPERTY_FLAGS_HIGH_ALARM_SET_THRESHOLD, "HiVoltAlarmSet" },
    { BATTERY_PROP_VOLTAGE_NOW,
            BATTERY_PROPERTY_FLAGS_HIGH_ALARM_CLEAR_THRESHOLD, "HiVoltAlarmClear" },
    { BATTERY_PROP_TEMP_NOW,
            BATTERY_PROPERTY_FLAGS_LOW_ALARM_SET_THRESHOLD, "LoTempAlarmSet" },
    { BATTERY_PROP_TEMP_NOW,
            BATTERY_PROPERTY_FLAGS_LOW_ALARM_CLEAR_THRESHOLD, "LoTempAlarmClear" },
    { BATTERY_PROP_TEMP_NOW,
            BATTERY_PROPERTY_FLAGS_HIGH_ALARM_SET_THRESHOLD, "LoTempAlarmSet" },
    { BATTERY_PROP_TEMP_NOW,
            BATTERY_PROPERTY_FLAGS_HIGH_ALARM_CLEAR_THRESHOLD, "HiTempAlarmClear" },
    { BATTERY_PROP_NONE },
};

int
bq27z561_init(struct os_dev *dev, void *arg)
{
    struct bq27z561 *bq27;
#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bq27z561_init_arg *init_arg = (struct bq27z561_init_arg *)arg;
#endif
    struct os_dev *battery;

    if (!dev || !arg) {
        return SYS_ENODEV;
    }

    bq27 = (struct bq27z561 *)dev;

    bq27->bq27_initialized = 0;

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    /* Copy the interface struct */
    bq27->bq27_itf = init_arg->itf;
#endif

    bq27->dev.bd_funcs = &bq27z561_drv_funcs;
    bq27->dev.bd_driver_properties = bq27z561_battery_properties;
    bq27->dev.bd_driver_data = bq27;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    battery = arg;
#else
    battery = init_arg->battery;
#endif

    battery_add_driver(battery, &bq27->dev);

    return 0;
}

int bq27z561_pkg_init(void)
{
#if MYNEWT_VAL(BQ27Z561_CLI)
    return bq27z561_shell_init();
#else
    return 0;
#endif
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    bq27z561_init((struct os_dev *)bnode, arg);
}

int
bq27z561_create_i2c_dev(struct bus_i2c_node *node, const char *name,
                        const struct bus_i2c_node_cfg *i2c_cfg,
                        struct os_dev *battery_dev)
{
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_i2c_node_create(name, node, i2c_cfg, battery_dev);

    return rc;
}
#endif
