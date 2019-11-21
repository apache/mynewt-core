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
#include <errno.h>
#include <assert.h>

#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"
#include "lis2mdl/lis2mdl.h"
#include "lis2mdl_priv.h"
#include "modlog/modlog.h"
#include "os/mynewt.h"
#include "sensor/mag.h"
#include "sensor/sensor.h"
#include "stats/stats.h"

/* Define the stats section and records */
STATS_SECT_START(lis2mdl_stat_section)
    STATS_SECT_ENTRY(samples_mag)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lis2mdl_stat_section)
    STATS_NAME(lis2mdl_stat_section, samples_mag)
    STATS_NAME(lis2mdl_stat_section, errors)
STATS_NAME_END(lis2mdl_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lis2mdl_stat_section) g_lis2mdlstats;

/* Exports for the sensor API */
static int lis2mdl_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int lis2mdl_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);
static int lis2mdl_sensor_reset(struct sensor *sensor);

static const struct sensor_driver g_lis2mdl_sensor_driver = {
    .sd_read               = lis2mdl_sensor_read,
    .sd_get_config         = lis2mdl_sensor_get_config,
    .sd_reset              = lis2mdl_sensor_reset,
};

/**
 * Writes a single byte to the specified register
 *
 * @itf    Sensor interface
 * @addr   I2C address to use
 * @reg    Register address to write to
 * @value  Value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lis2mdl_write(struct sensor_itf *itf, uint8_t addr,
              enum lis2mdl_registers_mag reg, uint32_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 2,
        .buffer = payload
    };

    rc = sensor_itf_lock(itf, MYNEWT_VAL(LIS2MDL_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(LIS2MDL_I2C_RETRIES));
    if (rc) {
        LIS2MDL_LOG_ERROR(
                "Failed to write to 0x%02X:0x%02X with value 0x%02lX\n",
                addr, reg, value);
        STATS_INC(g_lis2mdlstats, errors);
    }

    sensor_itf_unlock(itf);

    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @itf    Sensor interface
 * @addr   I2C address to use
 * @reg    Register address to read from
 * @value  Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lis2mdl_read(struct sensor_itf *itf, uint8_t addr,
             enum lis2mdl_registers_mag reg, uint8_t *value)
{
    int rc;
    uint8_t payload;

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 1,
        .buffer = &payload
    };

    rc = sensor_itf_lock(itf, MYNEWT_VAL(LIS2MDL_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    /* Register write */
    payload = reg;
    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(LIS2MDL_I2C_RETRIES));
    if (rc) {
        LIS2MDL_LOG_ERROR("I2C access failed at address 0x%02X\n", addr);
        STATS_INC(g_lis2mdlstats, errors);
        goto err;
    }

    /* Read one byte back */
    payload = 0;
    rc = i2cn_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                          MYNEWT_VAL(LIS2MDL_I2C_RETRIES));
    *value = payload;
    if (rc) {
        LIS2MDL_LOG_ERROR("Failed to read from 0x%02X:0x%02X\n",
                             addr, reg);
        STATS_INC(g_lis2mdlstats, errors);
    }

err:
    sensor_itf_unlock(itf);

    return rc;
}

/**
 * Reads a magnetometer data from the specified register
 *
 * @itf     Sensor interface
 * @addr    I2C address to use
 * @reg     Register address to read from
 * @buffer  Pointer to where the register value should be written
 * @len     Buffer len
 *
 * Buffer max len is 6 byte for magnetomenter axis read
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lis2mdl_read_len(struct sensor_itf *itf, uint8_t addr,
                 enum lis2mdl_registers_mag reg,
                 uint8_t *buffer, uint8_t len)
{
    int rc;
    /* Payload max is 6 byte for magnetomenter axis read */
    uint8_t payload[7] = { reg, 0, 0, 0, 0, 0, 0 };

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 1,
        .buffer = payload
    };

    rc = sensor_itf_lock(itf, MYNEWT_VAL(LIS2MDL_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    /* Register write */
    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(LIS2MDL_I2C_RETRIES));
    if (rc) {
        LIS2MDL_LOG_ERROR("I2C access failed at address 0x%02X\n", addr);
        STATS_INC(g_lis2mdlstats, errors);
        goto out;
    }

    /* Read len bytes back */
    data_struct.len = len;
    rc = i2cn_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                          MYNEWT_VAL(LIS2MDL_I2C_RETRIES));

    if (rc) {
        LIS2MDL_LOG_ERROR("Failed to read from 0x%02X:0x%02X\n", addr, reg);
        STATS_INC(g_lis2mdlstats, errors);
    }

    memcpy(buffer, payload, len);

out:
    sensor_itf_unlock(itf);

    return rc;
}

/**
 * Write register with bitmask
 *
 * @itf   Device interface
 * @reg   LIS2MDL register of lis2mdl_registers_mag
 * @mask  Register bitmask
 * @val   New value
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lis2mdl_write_with_mask_mode(struct sensor_itf *itf,
                                        enum lis2mdl_registers_mag reg,
                                        uint8_t mask, uint8_t val)
{
    int rc;
    uint8_t data;

    rc = lis2mdl_read(itf, LIS2MDL_ADDR, reg, &data);
    if (rc) {
        return rc;
    }

    data &= ~mask;
    data |= (val & mask);

    return lis2mdl_write(itf, LIS2MDL_ADDR, reg, data);
}

/**
 * Reset device
 *
 * @itf  Device interface
 *
 * When SOFT_RST is set, the configuration registers and user
 * registers are reset.
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lis2mdl_reset(struct sensor_itf *itf)
{
    return lis2mdl_write_with_mask_mode(itf, LIS2MDL_CFG_REG_A,
                         LIS2MDL_SOFT_RST_MASK, 1);
}

/**
 * Set output data rate
 *
 * @itf  Device interface
 * @odr  Device data rate
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lis2mdl_set_odr(struct sensor_itf *itf, enum lis2mdl_rate odr)
{
    return lis2mdl_write_with_mask_mode(itf, LIS2MDL_CFG_REG_A,
                         LIS2MDL_ODR_MASK, odr);
}

/**
 * Set sensor mode
 *
 * @itf   Device interface
 * @mode  Device system configuration
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lis2mdl_set_mode(struct sensor_itf *itf, enum lis2mdl_mode mode)
{
    return lis2mdl_write_with_mask_mode(itf, LIS2MDL_CFG_REG_A,
                         LIS2MDL_MODE_MASK, mode);
}

/**
 * Set block data update
 *
 * @itf  Device interface
 * @bdu  Block data update flag [0,1]
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lis2mdl_set_bdu(struct sensor_itf *itf, bool bdu)
{
    return lis2mdl_write_with_mask_mode(itf, LIS2MDL_CFG_REG_C,
                         LIS2MDL_BDU_MASK, bdu);
}

/**
 * Set magnetometer temperature compensation
 *
 * @itf  Device interface
 * @tc   Temperature compensation flag [0,1]
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lis2mdl_set_comp_temp(struct sensor_itf *itf, bool tc)
{
    return lis2mdl_write_with_mask_mode(itf, LIS2MDL_CFG_REG_A,
                         LIS2MDL_COMP_TEMP_EN_MASK, tc);
}

/**
 * Get chip ID
 *
 * @itf      Sensor interface
 * @chip_id  Pointer to chip id to be filled up
 *
 * @return 0 on success, non-zero on failure
 */
static int lis2mdl_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id)
{
    return lis2mdl_read(itf, LIS2MDL_ADDR, LIS2MDL_WHO_AM_I, chip_id);
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @dev  Device object associated with this sensor
 * @arg  Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2mdl_init(struct os_dev *dev, void *arg)
{
    struct lis2mdl *lis2mdl;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        return SYS_ENODEV;
    }

    lis2mdl = (struct lis2mdl *)dev;
    lis2mdl->cfg.mask = SENSOR_TYPE_ALL;
    sensor = &lis2mdl->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lis2mdlstats),
        STATS_SIZE_INIT_PARMS(g_lis2mdlstats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lis2mdl_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lis2mdlstats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto out;
    }

    /* Add the magnetometer driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_MAGNETIC_FIELD,
            (struct sensor_driver *)&g_lis2mdl_sensor_driver);
    if (rc != 0) {
        goto out;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto out;
    }

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        goto out;
    }

out:
    return rc;
}

/**
 * Set default sensor configuration
 *
 * @lis2mdl  Magnetometer data structure
 * @cfg      Sensor configuration structure
 *
 * @return 0 on success, non-zero error on failure.
 */
int lis2mdl_config(struct lis2mdl *lis2mdl, struct lis2mdl_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;
    uint8_t wai;

    if (!cfg) {
        return SYS_EINVAL;
    }

    itf = SENSOR_GET_ITF(&(lis2mdl->sensor));

    /* Check device ID (Who Am I) */
    rc = lis2mdl_get_chip_id(itf, &wai);
    if (rc) {
        goto out;
    }

    if (wai != LIS2MDL_WHO_AM_I_VAL) {
        rc = SYS_EINVAL;
        goto out;
    }

    /* Enable Block Data Update */
    rc = lis2mdl_set_bdu(itf, 1);
    if (rc) {
        goto out;
    }

    /* Enable magnetometer temperature compensation */
    rc = lis2mdl_set_comp_temp(itf, 1);
    if (rc) {
        goto out;
    }

    /* Set magnetometer system mode */
    rc = lis2mdl_set_mode(itf, cfg->mode);
    if (rc) {
        goto out;
    }

    /* Set magnetometer outout data rate */
    rc =lis2mdl_set_odr(itf, cfg->rate);
    if (rc) {
        goto out;
    }

    rc = sensor_set_type_mask(&(lis2mdl->sensor), cfg->mask);
    if (rc) {
        goto out;
    }

    /* Save to local configuration */
    lis2mdl->cfg.mode = cfg->mode;
    lis2mdl->cfg.rate = cfg->rate;
    lis2mdl->cfg.mask = cfg->mask;

out:
    return rc;
}

/**
 * Reset sensor device
 *
 * @sensor  Sensor structure
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lis2mdl_sensor_reset(struct sensor *sensor)
{
    struct lis2mdl *lis2mdl;
    struct sensor_itf *itf;

    lis2mdl = (struct lis2mdl *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(&(lis2mdl->sensor));

    return lis2mdl_reset(itf);
}

/**
 * Read magnetometer sensor data (polling mode)
 *
 * @sensor     Device interface
 * @type       Sensor type (should be SENSOR_TYPE_MAGNETIC_FIELD)
 * @data_func  Function data callback
 * @data_arg   Function data callback arguments
 * @timeout    Read timeout
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lis2mdl_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc;
    int16_t x, y, z;
    uint8_t payload[6];
    struct sensor_itf *itf;
    struct sensor_mag_data smd;
    struct lis2mdl *lis2mdl;

    if (!(type & SENSOR_TYPE_MAGNETIC_FIELD)) {
        return SYS_EINVAL;
    }

    /* Check supported system mode */
    lis2mdl = (struct lis2mdl *)SENSOR_GET_DEVICE(sensor);
    if (lis2mdl->cfg.mode != LIS2MDL_CONTINUOUS_MODE) {
        return SYS_ENOTSUP;
    }

    itf = SENSOR_GET_ITF(sensor);
    rc = lis2mdl_read_len(itf, LIS2MDL_ADDR, LIS2MDL_OUTX_L_REG, payload,
                          sizeof(payload));
    if (rc != 0) {
        return rc;
    }

    /*
     * Shift magnetometer data values into 16-bit int.
     * By default LIS2MDL is little endian
     */
    x = (int16_t)le16toh(*(uint16_t *)&payload[0]);
    y = (int16_t)le16toh(*(uint16_t *)&payload[2]);
    z = (int16_t)le16toh(*(uint16_t *)&payload[4]);

    STATS_INC(g_lis2mdlstats, samples_mag);

    /* Convert from LSB to uTesla */
    smd.smd_x = LIS2MDL_LSB_TO_UTESLA(x);
    smd.smd_y = LIS2MDL_LSB_TO_UTESLA(y);
    smd.smd_z = LIS2MDL_LSB_TO_UTESLA(z);

    smd.smd_x_is_valid = 1;
    smd.smd_y_is_valid = 1;
    smd.smd_z_is_valid = 1;

    return data_func(sensor, data_arg, &smd, SENSOR_TYPE_MAGNETIC_FIELD);
}

/**
 * Get sensor data configuration
 *
 * @type  Fensor type (SENSOR_TYPE_MAGNETIC_FIELD)
 * @cfg   Sensor configuration structure
 *
 * @return 0 on success, SYS_EINVAL error on failure.
 */
static int
lis2mdl_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                          struct sensor_cfg *cfg)
{
    if (type != SENSOR_TYPE_MAGNETIC_FIELD) {
        return SYS_EINVAL;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return 0;
}
