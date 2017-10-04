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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include "defs/error.h"
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "hal/hal_i2c.h"
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/mag.h"
#include "lsm303dlhc/lsm303dlhc.h"
#include "lsm303dlhc_priv.h"
#include "log/log.h"
#include "stats/stats.h"

/* Define the stats section and records */
STATS_SECT_START(lsm303dlhc_stat_section)
    STATS_SECT_ENTRY(samples_acc_2g)
    STATS_SECT_ENTRY(samples_acc_4g)
    STATS_SECT_ENTRY(samples_acc_8g)
    STATS_SECT_ENTRY(samples_acc_16g)
    STATS_SECT_ENTRY(samples_mag_1_3g)
    STATS_SECT_ENTRY(samples_mag_1_9g)
    STATS_SECT_ENTRY(samples_mag_2_5g)
    STATS_SECT_ENTRY(samples_mag_4_0g)
    STATS_SECT_ENTRY(samples_mag_4_7g)
    STATS_SECT_ENTRY(samples_mag_5_6g)
    STATS_SECT_ENTRY(samples_mag_8_1g)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lsm303dlhc_stat_section)
    STATS_NAME(lsm303dlhc_stat_section, samples_acc_2g)
    STATS_NAME(lsm303dlhc_stat_section, samples_acc_4g)
    STATS_NAME(lsm303dlhc_stat_section, samples_acc_8g)
    STATS_NAME(lsm303dlhc_stat_section, samples_acc_16g)
    STATS_NAME(lsm303dlhc_stat_section, samples_mag_1_3g)
    STATS_NAME(lsm303dlhc_stat_section, samples_mag_1_9g)
    STATS_NAME(lsm303dlhc_stat_section, samples_mag_2_5g)
    STATS_NAME(lsm303dlhc_stat_section, samples_mag_4_0g)
    STATS_NAME(lsm303dlhc_stat_section, samples_mag_4_7g)
    STATS_NAME(lsm303dlhc_stat_section, samples_mag_5_6g)
    STATS_NAME(lsm303dlhc_stat_section, samples_mag_8_1g)
    STATS_NAME(lsm303dlhc_stat_section, errors)
STATS_NAME_END(lsm303dlhc_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lsm303dlhc_stat_section) g_lsm303dlhcstats;

#define LOG_MODULE_LSM303DLHC (303)
#define LSM303DLHC_INFO(...)  LOG_INFO(&_log, LOG_MODULE_LSM303DLHC, __VA_ARGS__)
#define LSM303DLHC_ERR(...)   LOG_ERROR(&_log, LOG_MODULE_LSM303DLHC, __VA_ARGS__)
static struct log _log;

/* Exports for the sensor API */
static int lsm303dlhc_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int lsm303dlhc_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_lsm303dlhc_sensor_driver = {
    lsm303dlhc_sensor_read,
    lsm303dlhc_sensor_get_config
};

/**
 * Writes a single byte to the specified register
 *
 * @param The sensor interface
 * @param The I2C address to use
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm303dlhc_write8(struct sensor_itf *itf, uint8_t addr, uint8_t reg,
                  uint32_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LSM303DLHC_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                       addr, reg, value);
        STATS_INC(g_lsm303dlhcstats, errors);
    }

    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @param The sensor interface
 * @param The I2C address to use
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm303dlhc_read8(struct sensor_itf *itf, uint8_t addr, uint8_t reg,
                 uint8_t *value)
{
    int rc;
    uint8_t payload;

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 1,
        .buffer = &payload
    };

    /* Register write */
    payload = reg;
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LSM303DLHC_ERR("I2C access failed at address 0x%02X\n", addr);
        STATS_INC(g_lsm303dlhcstats, errors);
        goto err;
    }

    /* Read one byte back */
    payload = 0;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);
    *value = payload;
    if (rc) {
        LSM303DLHC_ERR("Failed to read from 0x%02X:0x%02X\n", addr, reg);
        STATS_INC(g_lsm303dlhcstats, errors);
    }

err:
    return rc;
}

/**
 * Reads a six bytes from the specified register
 *
 * @param The sensor interface
 * @param The I2C address to use
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm303dlhc_read48(struct sensor_itf *itf, uint8_t addr, uint8_t reg,
                  uint8_t *buffer)
{
    int rc;
    uint8_t payload[7] = { reg, 0, 0, 0, 0, 0, 0 };

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 1,
        .buffer = payload
    };

    /* Clear the supplied buffer */
    memset(buffer, 0, 6);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LSM303DLHC_ERR("I2C access failed at address 0x%02X\n", addr);
        STATS_INC(g_lsm303dlhcstats, errors);
        goto err;
    }

    /* Read six bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = 6;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        LSM303DLHC_ERR("Failed to read from 0x%02X:0x%02X\n", addr, reg);
        STATS_INC(g_lsm303dlhcstats, errors);
        goto err;
    }

    /* Copy the I2C results into the supplied buffer */
    memcpy(buffer, payload, 6);

err:
    return rc;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accellerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm303dlhc_init(struct os_dev *dev, void *arg)
{
    struct lsm303dlhc *lsm;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    lsm = (struct lsm303dlhc *) dev;

    lsm->cfg.mask = SENSOR_TYPE_ALL;

    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    sensor = &lsm->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lsm303dlhcstats),
        STATS_SIZE_INIT_PARMS(g_lsm303dlhcstats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lsm303dlhc_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lsm303dlhcstats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the accelerometer/magnetometer driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER |
            SENSOR_TYPE_MAGNETIC_FIELD,
            (struct sensor_driver *) &g_lsm303dlhc_sensor_driver);
    if (rc != 0) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
lsm303dlhc_config(struct lsm303dlhc *lsm, struct lsm303dlhc_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(lsm->sensor));

    /* Most sensor chips have a single address and just use different
     * registers to get data for different sensors
     */
    if (!cfg->acc_addr || !cfg->mag_addr) {
        rc = SYS_EINVAL;
        goto err;
    }

    /* Set accel data rate (or power down) and enable XYZ output */
    rc = lsm303dlhc_write8(itf, cfg->acc_addr,
                           LSM303DLHC_REGISTER_ACCEL_CTRL_REG1_A,
                           cfg->accel_rate | 0x07);
    if (rc) {
        goto err;
    }

    lsm->cfg.accel_rate = cfg->accel_rate;

    /* Set accel scale */
    rc = lsm303dlhc_write8(itf, cfg->acc_addr,
                           LSM303DLHC_REGISTER_ACCEL_CTRL_REG4_A,
                           cfg->accel_range);
    if (rc) {
        goto err;
    }

    lsm->cfg.accel_range = cfg->accel_range;

    /* Enable the magnetomer (set to continuous conversion mode) */
    rc = lsm303dlhc_write8(itf, cfg->mag_addr,
                           LSM303DLHC_REGISTER_MAG_MR_REG_M, 0x00);
    if (rc) {
        goto err;
    }

    /* Set mag rate */
    rc = lsm303dlhc_write8(itf, cfg->mag_addr,
                           LSM303DLHC_REGISTER_MAG_CRA_REG_M,
                           cfg->mag_rate);
    if (rc) {
        goto err;
    }

    lsm->cfg.mag_rate = cfg->mag_rate;

    /* Set mag gain */
    rc = lsm303dlhc_write8(itf, cfg->mag_addr,
                           LSM303DLHC_REGISTER_MAG_CRB_REG_M,
                           cfg->mag_gain);
    if (rc) {
        goto err;
    }

    lsm->cfg.mag_gain = cfg->mag_gain;

    rc = sensor_set_type_mask(&(lsm->sensor),  cfg->mask);
    if (rc) {
        goto err;
    }

    lsm->cfg.mask = cfg->mask;
    lsm->cfg.mag_addr = cfg->mag_addr;
    lsm->cfg.acc_addr = cfg->acc_addr;

    return 0;
err:
    return (rc);
}

static int
lsm303dlhc_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc;
    int16_t x, y, z;
    float mg_lsb;
    int16_t gauss_lsb_xy;
    int16_t gauss_lsb_z;
    uint8_t payload[6];
    struct sensor_itf *itf;
    struct lsm303dlhc *lsm;
    union {
        struct sensor_accel_data sad;
        struct sensor_mag_data smd;
    }databuf;

    /* If the read isn't looking for accel or mag data, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER) &&
       (!(type & SENSOR_TYPE_MAGNETIC_FIELD))) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);
    lsm = (struct lsm303dlhc *) SENSOR_GET_DEVICE(sensor);

    /* Get a new accelerometer sample */
    if (type & SENSOR_TYPE_ACCELEROMETER) {
        x = y = z = 0;
        rc = lsm303dlhc_read48(itf, lsm->cfg.acc_addr,
                               LSM303DLHC_REGISTER_ACCEL_OUT_X_L_A | 0x80,
                               payload);
        if (rc != 0) {
            goto err;
        }

        /* Shift 12-bit left-aligned accel values into 16-bit int */
        x = ((int16_t)(payload[0] | (payload[1] << 8))) >> 4;
        y = ((int16_t)(payload[2] | (payload[3] << 8))) >> 4;
        z = ((int16_t)(payload[4] | (payload[5] << 8))) >> 4;

        /* Determine mg per lsb based on range */
        switch(lsm->cfg.accel_range) {
            case LSM303DLHC_ACCEL_RANGE_2:
                STATS_INC(g_lsm303dlhcstats, samples_acc_2g);
                mg_lsb = 0.001F;
                break;
            case LSM303DLHC_ACCEL_RANGE_4:
                STATS_INC(g_lsm303dlhcstats, samples_acc_4g);
                mg_lsb = 0.002F;
                break;
            case LSM303DLHC_ACCEL_RANGE_8:
                STATS_INC(g_lsm303dlhcstats, samples_acc_8g);
                mg_lsb = 0.004F;
                break;
            case LSM303DLHC_ACCEL_RANGE_16:
                STATS_INC(g_lsm303dlhcstats, samples_acc_16g);
                mg_lsb = 0.012F;
                break;
            default:
                LSM303DLHC_ERR("Unknown accel range: 0x%02X. Assuming +/-2G.\n",
                    lsm->cfg.accel_range);
                mg_lsb = 0.001F;
                break;
        }

        /* Convert from mg to Earth gravity in m/s^2 */
        databuf.sad.sad_x = (float)x * mg_lsb * 9.80665F;
        databuf.sad.sad_y = (float)y * mg_lsb * 9.80665F;
        databuf.sad.sad_z = (float)z * mg_lsb * 9.80665F;

        databuf.sad.sad_x_is_valid = 1;
        databuf.sad.sad_y_is_valid = 1;
        databuf.sad.sad_z_is_valid = 1;

        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.sad, SENSOR_TYPE_ACCELEROMETER);
        if (rc != 0) {
            goto err;
        }
    }

    /* Get a new magnetometer sample */
    if (type & SENSOR_TYPE_MAGNETIC_FIELD) {
        x = y = z = 0;
        rc = lsm303dlhc_read48(itf, lsm->cfg.mag_addr,
                               LSM303DLHC_REGISTER_MAG_OUT_X_H_M,
                               payload);
        if (rc != 0) {
            goto err;
        }

        /* Shift mag values into 16-bit int */
        x = (int16_t)(payload[1] | ((int16_t)payload[0] << 8));
        y = (int16_t)(payload[3] | ((int16_t)payload[2] << 8));
        z = (int16_t)(payload[5] | ((int16_t)payload[4] << 8));

        /* Determine gauss per lsb based on gain */
        switch (lsm->cfg.mag_gain) {
            case LSM303DLHC_MAG_GAIN_1_3:
                STATS_INC(g_lsm303dlhcstats, samples_mag_1_3g);
                gauss_lsb_xy = 1100;
                gauss_lsb_z = 980;
                break;
            case LSM303DLHC_MAG_GAIN_1_9:
                STATS_INC(g_lsm303dlhcstats, samples_mag_1_9g);
                gauss_lsb_xy = 855;
                gauss_lsb_z = 760;
                break;
            case LSM303DLHC_MAG_GAIN_2_5:
                STATS_INC(g_lsm303dlhcstats, samples_mag_2_5g);
                gauss_lsb_xy = 670;
                gauss_lsb_z = 600;
                break;
            case LSM303DLHC_MAG_GAIN_4_0:
                STATS_INC(g_lsm303dlhcstats, samples_mag_4_0g);
                gauss_lsb_xy = 450;
                gauss_lsb_z = 400;
                break;
            case LSM303DLHC_MAG_GAIN_4_7:
                STATS_INC(g_lsm303dlhcstats, samples_mag_4_7g);
                gauss_lsb_xy = 400;
                gauss_lsb_z = 355;
                break;
            case LSM303DLHC_MAG_GAIN_5_6:
                STATS_INC(g_lsm303dlhcstats, samples_mag_5_6g);
                gauss_lsb_xy = 330;
                gauss_lsb_z = 295;
                break;
            case LSM303DLHC_MAG_GAIN_8_1:
                STATS_INC(g_lsm303dlhcstats, samples_mag_8_1g);
                gauss_lsb_xy = 230;
                gauss_lsb_z = 205;
                break;
            default:
                LSM303DLHC_ERR("Unknown mag gain: 0x%02X. Assuming +/-1.3g.\n",
                    lsm->cfg.mag_gain);
                gauss_lsb_xy = 1100;
                gauss_lsb_z = 980;
                break;
        }

        /* Convert from gauss to micro Tesla */
        databuf.smd.smd_x = (float)x / gauss_lsb_xy * 100.0F;
        databuf.smd.smd_y = (float)y / gauss_lsb_xy * 100.0F;
        databuf.smd.smd_z = (float)z / gauss_lsb_z * 100.0F;

        databuf.smd.smd_x_is_valid = 1;
        databuf.smd.smd_y_is_valid = 1;
        databuf.smd.smd_z_is_valid = 1;

        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.smd, SENSOR_TYPE_MAGNETIC_FIELD);
        if (rc != 0) {
            goto err;
        }
    }

    return (0);
err:
    return (rc);
}

static int
lsm303dlhc_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if ((type != SENSOR_TYPE_ACCELEROMETER) &&
        (type != SENSOR_TYPE_MAGNETIC_FIELD)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return (0);
err:
    return (rc);
}
